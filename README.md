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

  1. Create a “SimpsonsHitRun” folder in the root of the headset's internal
     shared storage (`Internal shared storage\SimpsonsHitRun` on Quest,
     `/sdcard/SimpsonsHitRun` on Android XR)
  2. Copy all the files from the PC version of the game (without mods, the original version) into folder SimpsonsHitRun
  3. Install the apk and play

  On first launch the game opens the system “All files access” screen; grant
  it so the game can read that folder.

  ## Known Gaps on Android XR

  - **Controllers are required.** Android XR's quality guidelines expect an
    app to be usable with hand input alone. This is a port of a 2003 twin-stick
    game and needs thumbsticks, so `android.hardware.xr.input.controller` is
    declared (optional, so the package still installs) and the Galaxy XR
    controllers are needed to play.
  - **No foveated rendering.** `XR_FB_foveation` is available on both
    platforms and would be the highest-value next optimisation at this eye
    resolution. It is not wired up.
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
