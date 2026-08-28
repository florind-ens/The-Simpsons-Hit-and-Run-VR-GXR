#!/usr/bin/env bash
#
# Cross-compiles the vendored FFmpeg tree (libs/ffmpeg) for Android arm64-v8a.
#
# The Android build used to pull FFmpeg from a prefab AAR
# (com.fpliu.ndk.pkg.prefab.android.21:ffmpeg:6.0) that is not published on
# Maven Central or anywhere else reachable, so nobody but the original author
# could resolve it. The source is already in this repository, so build it here
# instead and keep the whole thing self-contained.
#
# Only what the movie player needs is enabled. The game's FMVs are Bink
# (.rmv files with a BIKi magic), so that is one demuxer, one video decoder and
# the two Bink audio decoders.
#
# Usage: tools/build-ffmpeg-android.sh [ndk-dir]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/libs/ffmpeg"
PREFIX="$ROOT/libs/ffmpeg-android/arm64-v8a"
API=24

# Windows has no POSIX shell of its own, so this runs under MSYS2 (or WSL with
# a Linux NDK). tools/build-ffmpeg-android.bat finds one and calls this script.
CLANG_SUFFIX=""
case "$(uname -s)" in
    Darwin)               HOST_TAG=darwin-x86_64 ;;
    Linux)                HOST_TAG=linux-x86_64 ;;
    MINGW*|MSYS*|CYGWIN*) HOST_TAG=windows-x86_64; CLANG_SUFFIX=".cmd" ;;
    *) echo "error: unsupported build host $(uname -s)" >&2; exit 1 ;;
esac

WINDOWS_NDK_ROOT=""
if [ -n "${LOCALAPPDATA:-}" ] && command -v cygpath >/dev/null 2>&1; then
    WINDOWS_NDK_ROOT="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/ndk"
fi

NDK="${1:-${ANDROID_NDK_HOME:-}}"
if [ -z "$NDK" ]; then
    # Quoted one per line rather than split from a single string, so a home
    # directory with a space in it still resolves. Highest version wins.
    for root in \
        "$HOME/Library/Android/sdk/ndk" \
        "$HOME/Android/Sdk/ndk" \
        "$WINDOWS_NDK_ROOT"
    do
        [ -n "$root" ] && [ -d "$root" ] || continue
        for candidate in "$root"/*; do
            [ -d "$candidate" ] && NDK="$candidate"
        done
    done
fi
[ -d "$NDK" ] || { echo "error: set ANDROID_NDK_HOME or pass the NDK directory" >&2; exit 1; }
TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/$HOST_TAG"
[ -d "$TOOLCHAIN" ] || { echo "error: no LLVM toolchain at $TOOLCHAIN" >&2; exit 1; }

# FFmpeg's configure refuses to run when the source path contains whitespace,
# and a checkout can live anywhere. Stage the whole build somewhere clean and
# copy only the finished headers and archives back into the repository.
WORK="${TMPDIR:-/tmp}/shar-ffmpeg-android"
case "$WORK" in
    *[[:space:]]*) echo "error: staging directory $WORK contains whitespace" >&2; exit 1 ;;
esac

echo "FFmpeg source : $SRC"
echo "NDK           : $NDK"
echo "Staging in    : $WORK"
echo "Install prefix: $PREFIX"

mkdir -p "$WORK/src"
# Kept between runs so a rebuild is incremental; delete $WORK to start clean.
if command -v rsync >/dev/null 2>&1; then
    rsync -a --delete "$SRC/" "$WORK/src/"
else
    # MSYS2 has no rsync by default.
    cp -R "$SRC/." "$WORK/src/"
fi
# The vendored tree was committed without exec bits, so restore them on the
# staging copy. configure and the ffbuild helpers are run as programs.
find "$WORK/src" -type f \( -name '*.sh' -o -name '*.pl' -o -name 'configure' \) \
    -exec chmod +x {} +
mkdir -p "$WORK/build"
cd "$WORK/build"

# Invoked through bash rather than directly: the vendored tree was committed
# without exec bits, so configure is not executable in a fresh clone.
#
# --disable-asm: the archives are linked into libmain.so, and FFmpeg's aarch64
# assembly reaches its constant tables with ADRP/ADD pairs that lld rejects
# against a shared object. Plain C costs nothing measurable here, because the
# only thing being decoded is 640x480 Bink.
bash "$WORK/src/configure" \
    --prefix="$WORK/out" \
    --target-os=android \
    --arch=aarch64 \
    --cpu=armv8-a \
    --enable-cross-compile \
    --sysroot="$TOOLCHAIN/sysroot" \
    --cc="$TOOLCHAIN/bin/aarch64-linux-android$API-clang$CLANG_SUFFIX" \
    --cxx="$TOOLCHAIN/bin/aarch64-linux-android$API-clang++$CLANG_SUFFIX" \
    --ar="$TOOLCHAIN/bin/llvm-ar" \
    --nm="$TOOLCHAIN/bin/llvm-nm" \
    --ranlib="$TOOLCHAIN/bin/llvm-ranlib" \
    --strip="$TOOLCHAIN/bin/llvm-strip" \
    --enable-static \
    --disable-shared \
    --enable-pic \
    --disable-asm \
    --disable-programs \
    --disable-doc \
    --disable-avdevice \
    --disable-avfilter \
    --disable-network \
    --disable-debug \
    --disable-everything \
    --enable-demuxer=bink \
    --enable-decoder=bink,binkaudio_dct,binkaudio_rdft \
    --enable-protocol=file \
    --extra-cflags="-O2 -fPIC" \
    --extra-ldflags="-Wl,-z,max-page-size=16384"

JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)"
make -j"$JOBS"
make install

rm -rf "$PREFIX"
mkdir -p "$PREFIX"
cp -R "$WORK/out/include" "$PREFIX/include"
cp -R "$WORK/out/lib" "$PREFIX/lib"

echo
echo "Built:"
ls -la "$PREFIX/lib"/*.a
