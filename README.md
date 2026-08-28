  ## Development
  The entire port was created using AI-assisted development, including implementation, debugging, VR rendering work, performance optimization, and platform adaptation.
  
  # The Simpsons: Hit & Run VR

  A standalone VR port of **The Simpsons: Hit & Run** for OpenXR headsets:
  Meta Quest 3 / 3S on Horizon OS, and Android XR devices such as Samsung
  Galaxy XR.

  This project is based on [Carlox33/The-Simpsons-Hit-and-Run-Android](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android) and adapts the Android version for standalone VR hardware.

  ## Features

  - Vendor-neutral OpenXR: no Meta-only runtime, loader, or extension is
    required at any point.
  - GLES 3.2 rendering.
  - Single-pass stereo/multiview rendering.
  - Stereoscopic game world and HUD.
  - VR-compatible menus and settings.
  - VR controller support.
  - Room-scale movement and recentering.
  - Seated mode.
  - Smooth and snap turning.
  - VR steering-wheel vehicle control.
  - Adjustable refresh rate and render scale.
  - Fixed foveated rendering, dynamically modulated by the runtime.
  - Cascaded shadow maps.
  - Enhanced materials and lighting.

  ## Platform Support

  A single APK targets both platforms. Everything platform-specific is
  resolved from the OpenXR runtime at start-up rather than compiled in:

  - **Loader.** The Khronos `openxr_loader_for_android` is used, not Meta's.
    It resolves Horizon OS's runtime on Quest and `libopenxr.google.so` on
    Android XR.
  - **Extensions.** Instance extensions are enumerated before
    `xrCreateInstance`. Only `XR_KHR_android_create_instance` and
    `XR_KHR_opengl_es_enable` are required; `XR_FB_color_space` and
    `XR_FB_display_refresh_rate` are enabled only where present, and the game
    runs without either.
  - **Controllers.** The Oculus Touch profile is the primary binding, which
    both Horizon OS and Android XR support for 6DoF controllers. The Khronos
    simple controller is suggested *only* if that is rejected: Android XR's
    porting guidance is to keep it out of the action map, because its
    presence interferes with binding the Galaxy XR controllers. The profile
    the runtime actually selects is written to logcat.
  - **Refresh rate.** The display menu is built from the rates the runtime
    enumerates (Galaxy XR reports 60/72/90). A saved preference is honoured
    when the headset offers it; otherwise the rate the runtime already chose
    is kept rather than guessed at. Where no refresh-rate control exists the
    row shows “Default”.
  - **Eye resolution.** Derived from the runtime's recommended view size,
    supersampled, then capped to a fixed per-eye pixel budget. The cap is what
    keeps Galaxy XR's 3552x3840 panels from asking this renderer for more
    pixels than it can shade in a 13.8 ms frame; it stays above Android XR's
    1856x2160 per-eye quality guideline. `XR_ANDROID_recommended_resolution`
    is enabled where present, so the eye buffers are rebuilt when the runtime
    revises its recommendation for thermal reasons.
  - **Blend mode.** Chosen from the modes the system advertises for the
    primary stereo view configuration.
  - **Foveation.** `XR_FB_foveation` is enabled where the runtime offers it,
    together with `XR_FB_swapchain_update_state` and
    `XR_FB_foveation_configuration`; both Horizon OS and Android XR advertise
    the set. The eye swapchain is created foveation-capable and a level
    profile is pushed at it, so nothing in the renderer or its shaders
    changes. The level is `XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB`, which lets
    the runtime ease off whenever there is GPU headroom, and it defaults to
    Medium. Off / Low / Medium / High are selectable from the VR settings
    screen, and the choice is saved in `vrsettings.cfg`. The row is only shown
    where the runtime supports foveation.
  - **16 KB pages.** Android 15 and newer run with 16 KB memory pages and
    Galaxy XR ships on Android 16, where a 4 KB-aligned `.so` will not load.
    The native build passes `-Wl,-z,max-page-size=16384` explicitly.
  - **Manifest.** Carries Horizon OS and Android XR entries side by side.
    Both platforms' feature declarations are marked optional on purpose, so
    that one package installs on either; the Android XR
    `PROPERTY_XR_ACTIVITY_START_MODE` property launches the game into Full
    Space.

  ## Installation

  You must provide your own legal copy of the PC version of **The Simpsons: Hit & Run**. Original game files are not included with this project.

  Enable developer mode and USB debugging on the headset, connect it over USB,
  and accept the debugging prompt that appears inside the headset.

  ### With the install script

  `tools/install.py` runs on Linux, Windows and macOS. It needs Python 3 and
  [Android platform-tools](https://developer.android.com/tools/releases/platform-tools);
  nothing else. It installs the APK, copies the game data, and grants the
  all-files permission so you never see the permission screen in the headset.

  ```
  python3 tools/install.py --game-dir "/path/to/Simpsons"
  python  tools\install.py --game-dir "C:\Games\Simpsons Hit & Run\Simpsons"
  ```

  Point `--game-dir` at the folder that holds `art/`, `movies/` and the `.rcf`
  files. The first run copies about 1.8 GB and takes a while; the copy is a
  sync, so it is safe to interrupt and re-run, and later runs only send what
  changed.

  Useful options: `--launch` starts the game when it finishes, `--skip-data`
  reinstalls only the APK, `--device SERIAL` picks one of several connected
  headsets (`--list-devices` shows them), and `--apk PATH` installs a specific
  build. `--help` lists the rest.

  ### By hand

  1. Create a `SimpsonsHitRun` folder in the root of the headset's internal
     shared storage (`Internal shared storage\SimpsonsHitRun` on Quest,
     `/sdcard/SimpsonsHitRun` on Android XR)
  2. Copy all the files from the PC version of the game (without mods, the
     original version) into that folder
  3. Install the APK: `adb install -r app-release.apk`

  On first launch the game opens the system “All files access” screen; grant
  it so the game can read that folder. To skip that step:
  `adb shell appops set com.simpsonsHitAndRun.vr MANAGE_EXTERNAL_STORAGE allow`

  ## Building

  Needs the Android SDK, an NDK matching `ndkVersion` in
  `android-project/app/build.gradle`, and a JDK 17 or newer (Android Studio's
  bundled one works).

  FFmpeg has to be built once before the first APK build. It comes from the
  source vendored in `libs/ffmpeg`:

  ```
  tools/build-ffmpeg-android.sh     # Linux/macOS
  tools\build-ffmpeg-android.bat    # Windows
  ```

  FFmpeg's configure is a POSIX shell script and its build is driven by make,
  so Windows cannot run it natively. The `.bat` is only a launcher: it finds a
  shell and calls the same `.sh`. Install [MSYS2](https://www.msys2.org) and
  `pacman -S make diffutils` first. Git Bash will not do, as it ships no make.
  A WSL shell works too, but then the NDK has to be the Linux one.

  Then build the APK:

  ```
  build-apk.bat                                                   # Windows
  ./android-project/gradlew -p android-project assembleRelease    # Linux/macOS
  ```

  The APK lands in
  `android-project/app/build/outputs/apk/release/app-release.apk`, which is
  where `tools/install.py` looks by default.

  ## Known Gaps on Android XR

  - **Controllers are required.** Android XR's quality guidelines expect an
    app to be usable with hand input alone. This is a port of a 2003 twin-stick
    game and needs thumbsticks, so `android.hardware.xr.input.controller` is
    declared (optional, so the package still installs) and the Galaxy XR
    controllers are needed to play.
  - **FFmpeg dependency.** `com.fpliu.ndk.pkg.prefab.android.21:ffmpeg:6.0` is
    not published on Maven Central, so it has to come from a local Maven
    repository. It is also a prebuilt `android-21` binary, which means its
    16 KB page alignment is not guaranteed. Verify before trusting an
    Android XR build:

    ```
    unzip -p app-release.apk lib/arm64-v8a/libavcodec.so > /tmp/libavcodec.so
    llvm-readelf -l /tmp/libavcodec.so | grep LOAD
    ```

    The `Align` column must read `0x4000`. `0x1000` means the library will
    fail to load on Galaxy XR.

  ## Project Status

  The port is actively developed. Core gameplay, VR rendering, menus, HUD, and
  controller support have been adapted for standalone VR. The Android XR path
  is written against the published Android XR OpenXR surface and the Galaxy XR
  hardware specification, and has not yet been verified on Galaxy XR hardware;
  Quest 3 remains the tested target. PCVR version in plans.

  ## Based On

  This project is based on:

  [Carlox33/The-Simpsons-Hit-and-Run-Android](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android)

  Special thanks to the original Android port authors and everyone contributing to the community effort to bring the game to modern platforms.

  ## Legal Notice

  **The Simpsons: Hit & Run** is the intellectual property of its respective rights holders. This project does not include original game assets and does not distribute pirated copies of the game.
