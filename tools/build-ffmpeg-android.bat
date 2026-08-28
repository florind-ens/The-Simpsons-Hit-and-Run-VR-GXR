@echo off
setlocal

rem Builds FFmpeg for Android on Windows.
rem
rem FFmpeg's configure is a POSIX shell script and its build is driven by make,
rem neither of which Windows can run on its own. So this is a launcher, not a
rem second implementation: it finds a suitable shell and hands it
rem tools\build-ffmpeg-android.sh, which is the real build script on every
rem platform.
rem
rem MSYS2 is the one that works out of the box, because it ships make and
rem cygpath and drives the ordinary Windows NDK.

cd /d "%~dp0.."

set "BASH="
call :try "C:\msys64\usr\bin\bash.exe"
call :try "C:\msys32\usr\bin\bash.exe"
call :try "%ProgramFiles%\Git\bin\bash.exe"
call :try "%ProgramFiles(x86)%\Git\bin\bash.exe"
call :try "%LOCALAPPDATA%\Programs\Git\bin\bash.exe"

if not defined BASH goto noshell

rem Git Bash ships without make, and a bare MSYS2 install needs it added.
rem Checking now beats failing several minutes into a build.
"%BASH%" -lc "command -v make >/dev/null 2>&1"
if errorlevel 1 goto nomake

echo Using %BASH%
"%BASH%" -lc "cd \"$(cygpath -u '%CD%')\" && exec ./tools/build-ffmpeg-android.sh %*"
if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)
echo.
echo FFmpeg is built. Now build the APK with build-apk.bat
exit /b 0

:try
if defined BASH exit /b 0
if exist %~1 set "BASH=%~1"
exit /b 0

:noshell
echo ERROR: no POSIX shell found, and FFmpeg cannot be configured without one.
echo.
echo Install MSYS2 from https://www.msys2.org and then, in its terminal:
echo     pacman -S make diffutils
echo and run this script again.
echo.
echo Alternatively, from a WSL shell with a Linux NDK installed:
echo     ./tools/build-ffmpeg-android.sh
exit /b 1

:nomake
echo ERROR: found %BASH% but it has no 'make'.
echo.
echo If that is MSYS2, install it with:
echo     pacman -S make diffutils
echo.
echo Git Bash does not ship make and cannot build FFmpeg. Install MSYS2 from
echo https://www.msys2.org instead.
exit /b 1
