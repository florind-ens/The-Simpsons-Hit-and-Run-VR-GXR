#!/usr/bin/env python3
"""Install the VR port and its game data onto a connected headset.

Runs on Linux, Windows and macOS with a stock Python 3 and nothing else; adb
does the actual work. Copies the APK, syncs the PC game files to
/sdcard/SimpsonsHitRun, and grants the all-files permission the game needs to
read them.

Safe to re-run. The data copy uses "adb push --sync", so a second run only
transfers what changed, and an interrupted first run can simply be repeated.

  python3 tools/install.py --game-dir "/path/to/Simpsons"
  python  tools\\install.py --game-dir "C:\\Games\\Simpsons Hit & Run\\Simpsons"

Run with --help for the rest of the options.
"""
import argparse
import os
import shutil
import subprocess
import sys

PACKAGE = "com.simpsonsHitAndRun.vr"
ACTIVITY = PACKAGE + "/.SimpsonsActivity"
REMOTE_DIR = "/storage/emulated/0/SimpsonsHitRun"

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_APK = os.path.join(REPO_ROOT, "android-project", "app", "build",
                           "outputs", "apk", "release", "app-release.apk")

# A stock PC install has all of these. Enough to catch the usual mistakes:
# pointing at the installer, at the parent folder, or at a half-copied tree.
GAME_MARKERS = ("art", "movies", "scripts", "scripts.rcf", "soundfx.rcf")


def fail(message):
    sys.stderr.write("error: %s\n" % message)
    sys.exit(1)


def find_adb(explicit):
    """Locate adb: an explicit path, then PATH, then the usual SDK locations."""
    if explicit:
        if not os.path.isfile(explicit):
            fail("no adb at %s" % explicit)
        return explicit

    found = shutil.which("adb")
    if found:
        return found

    name = "adb.exe" if os.name == "nt" else "adb"
    candidates = []
    for variable in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        root = os.environ.get(variable)
        if root:
            candidates.append(os.path.join(root, "platform-tools", name))
    home = os.path.expanduser("~")
    if os.name == "nt":
        local = os.environ.get("LOCALAPPDATA", os.path.join(home, "AppData", "Local"))
        candidates.append(os.path.join(local, "Android", "Sdk", "platform-tools", name))
    elif sys.platform == "darwin":
        candidates.append(os.path.join(home, "Library", "Android", "sdk",
                                       "platform-tools", name))
    else:
        candidates.append(os.path.join(home, "Android", "Sdk", "platform-tools", name))
        candidates.append("/usr/lib/android-sdk/platform-tools/" + name)
        candidates.append("/opt/android-sdk/platform-tools/" + name)

    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate

    fail("adb not found. Install Android platform-tools, or pass --adb PATH.\n"
         "       https://developer.android.com/tools/releases/platform-tools")


class Adb(object):
    def __init__(self, executable, serial=None):
        self.executable = executable
        self.serial = serial

    def _argv(self, args):
        argv = [self.executable]
        if self.serial:
            argv += ["-s", self.serial]
        return argv + list(args)

    def run(self, *args, **kwargs):
        """Runs adb and returns stdout. check=False tolerates a failure."""
        check = kwargs.pop("check", True)
        result = subprocess.run(self._argv(args), stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, universal_newlines=True)
        if check and result.returncode != 0:
            fail("adb %s failed:\n%s" % (" ".join(args), result.stdout.strip()))
        return result.stdout.strip()

    def stream(self, *args):
        """Runs adb with its output going straight to the terminal, for progress."""
        # adb writes to the terminal directly while our own prints sit in
        # Python's buffer, so flush first or the progress appears above the
        # heading that introduces it.
        sys.stdout.flush()
        return subprocess.call(self._argv(args))

    def shell(self, command, check=True):
        return self.run("shell", command, check=check)


def list_devices(adb):
    devices = []
    for line in adb.run("devices", "-l").splitlines()[1:]:
        line = line.strip()
        if not line:
            continue
        fields = line.split()
        serial, state = fields[0], fields[1]
        model = ""
        for field in fields[2:]:
            if field.startswith("model:"):
                model = field[len("model:"):]
        devices.append((serial, state, model))
    return devices


def pick_device(adb, requested):
    devices = list_devices(adb)
    if requested:
        for serial, state, _ in devices:
            if serial == requested:
                if state != "device":
                    fail("device %s is '%s', not ready" % (serial, state))
                return requested
        fail("device %s is not connected" % requested)

    ready = [d for d in devices if d[1] == "device"]
    if not ready:
        unauthorized = [d for d in devices if d[1] == "unauthorized"]
        if unauthorized:
            fail("headset is unauthorized. Put it on and accept the "
                 "'Allow USB debugging' prompt, then re-run.")
        fail("no headset detected. Connect it over USB, enable developer mode "
             "and USB debugging, then re-run.")
    if len(ready) > 1:
        print("More than one device is connected:")
        for serial, _, model in ready:
            print("  %-24s %s" % (serial, model or "?"))
        fail("pick one with --device SERIAL")
    serial, _, model = ready[0]
    print("Device: %s (%s)" % (model or "?", serial))
    return serial


def validate_game_dir(game_dir):
    if not os.path.isdir(game_dir):
        fail("no such directory: %s" % game_dir)
    present = set(name.lower() for name in os.listdir(game_dir))
    missing = [m for m in GAME_MARKERS if m.lower() not in present]
    if not missing:
        return
    # Pointing one level too high is the usual slip.
    for name in os.listdir(game_dir):
        nested = os.path.join(game_dir, name)
        if not os.path.isdir(nested):
            continue
        try:
            inner = set(n.lower() for n in os.listdir(nested))
        except OSError:
            continue
        if all(m.lower() in inner for m in GAME_MARKERS):
            fail("%s does not look like the game folder, but %s does.\n"
                 "       Point --game-dir at that one instead." % (game_dir, nested))
    fail("%s does not look like a PC install of the game.\n"
         "       Missing: %s" % (game_dir, ", ".join(missing)))


def push_game_data(adb, game_dir):
    entries = sorted(os.listdir(game_dir))
    if not entries:
        fail("%s is empty" % game_dir)
    print("\nCopying game data to %s" % REMOTE_DIR)
    print("(first run moves about 1.8 GB and takes a while; re-runs only send "
          "what changed)")
    adb.shell("mkdir -p %s" % REMOTE_DIR)
    # One argument per top-level entry. Pushing the folder itself would nest it
    # inside an existing destination on a second run.
    argv = ["push", "--sync"] + [os.path.join(game_dir, e) for e in entries]
    argv.append(REMOTE_DIR)
    if adb.stream(*argv) != 0:
        fail("copying game data failed")


def verify_game_data(adb, game_dir):
    local = sum(len(files) for _, _, files in os.walk(game_dir))
    remote = adb.shell("find %s -type f 2>/dev/null | wc -l" % REMOTE_DIR,
                       check=False)
    try:
        remote_count = int(remote.split()[-1])
    except (ValueError, IndexError):
        print("Could not count files on the device; skipping verification.")
        return
    if remote_count == local:
        print("Verified: %d files on the device." % remote_count)
    else:
        print("Warning: %d files locally but %d on the device. Re-run to finish "
              "the copy." % (local, remote_count))


def install_apk(adb, apk):
    if not os.path.isfile(apk):
        fail("no APK at %s\n"
             "       Build one first, or pass --apk PATH.\n"
             "       Windows: build-apk.bat\n"
             "       Linux/macOS: ./android-project/gradlew -p android-project "
             "assembleRelease" % apk)
    size_mb = os.path.getsize(apk) / (1024.0 * 1024.0)
    print("\nInstalling %s (%.1f MB)" % (os.path.basename(apk), size_mb))
    if adb.stream("install", "-r", apk) != 0:
        fail("installing the APK failed")


def grant_storage(adb):
    """The game reads its data straight off the shared storage.

    Granting this here saves finding the All files access screen inside the
    headset, but it is not fatal if the device refuses.
    """
    adb.shell("appops set %s MANAGE_EXTERNAL_STORAGE allow" % PACKAGE, check=False)
    state = adb.shell("appops get %s MANAGE_EXTERNAL_STORAGE" % PACKAGE, check=False)
    if "allow" in state:
        print("Granted all-files access.")
        return
    print("Could not grant all-files access automatically.\n"
          "  On first launch the game opens the 'All files access' screen; "
          "enable it there.")


def main():
    parser = argparse.ArgumentParser(
        description="Install The Simpsons: Hit & Run VR on a connected headset.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="You must supply your own legal copy of the PC game. Point\n"
               "--game-dir at the folder holding art/, movies/ and the .rcf files.")
    parser.add_argument("--game-dir", metavar="DIR",
                        help="PC install of the game to copy to the headset")
    parser.add_argument("--apk", metavar="PATH", default=DEFAULT_APK,
                        help="APK to install (default: the release build)")
    parser.add_argument("--device", metavar="SERIAL",
                        help="target this device when several are connected")
    parser.add_argument("--adb", metavar="PATH", help="path to the adb executable")
    parser.add_argument("--skip-apk", action="store_true",
                        help="only copy game data")
    parser.add_argument("--skip-data", action="store_true",
                        help="only install the APK")
    parser.add_argument("--no-grant", action="store_true",
                        help="do not grant all-files access")
    parser.add_argument("--launch", action="store_true",
                        help="start the game when finished")
    parser.add_argument("--list-devices", action="store_true",
                        help="list connected devices and exit")
    args = parser.parse_args()

    adb = Adb(find_adb(args.adb))
    adb.run("start-server", check=False)

    if args.list_devices:
        devices = list_devices(adb)
        if not devices:
            print("No devices connected.")
        for serial, state, model in devices:
            print("%-24s %-14s %s" % (serial, state, model or "?"))
        return

    if args.skip_apk and args.skip_data:
        fail("--skip-apk and --skip-data together leave nothing to do")
    if not args.skip_data and not args.game_dir:
        fail("--game-dir is required (or pass --skip-data to only install the APK)")

    game_dir = None
    if not args.skip_data:
        game_dir = os.path.abspath(os.path.expanduser(args.game_dir))
        validate_game_dir(game_dir)

    adb.serial = pick_device(adb, args.device)

    if not args.skip_apk:
        install_apk(adb, os.path.abspath(os.path.expanduser(args.apk)))
        if not args.no_grant:
            grant_storage(adb)

    if game_dir:
        push_game_data(adb, game_dir)
        verify_game_data(adb, game_dir)

    if args.launch:
        print("\nLaunching...")
        adb.shell("am start -n %s" % ACTIVITY, check=False)

    print("\nDone. Put the headset on and start "
          "'The Simpsons Hit & Run' from the app library.")


if __name__ == "__main__":
    main()
