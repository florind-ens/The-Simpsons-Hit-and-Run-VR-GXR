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
  - **Controllers.** Bindings are suggested for the Oculus Touch profile and
    for the Khronos simple controller, each offered separately so an
    unrecognised profile cannot take the other down with it. The profile the
    runtime actually selects is written to logcat.
  - **Refresh rate.** The display menu is built from the rates the runtime
    enumerates. Where no refresh-rate control exists the row shows
    “Default” and the runtime's own choice is left alone.
  - **Eye resolution.** Derived from the runtime's recommended view size
    rather than from a hard-coded panel resolution.
  - **Blend mode.** Chosen from the modes the system advertises for the
    primary stereo view configuration.
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

  ## Project Status

  The port is actively developed. Core gameplay, VR rendering, menus, HUD, and
  controller support have been adapted for standalone VR. The Android XR path
  is written against the published Android XR OpenXR surface and has not yet
  been verified on Galaxy XR hardware; Quest 3 remains the tested target.
  PCVR version in plans.

  ## Based On

  This project is based on:

  [Carlox33/The-Simpsons-Hit-and-Run-Android](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android)

  Special thanks to the original Android port authors and everyone contributing to the community effort to bring the game to modern platforms.

  ## Legal Notice

  **The Simpsons: Hit & Run** is the intellectual property of its respective rights holders. This project does not include original game assets and does not distribute pirated copies of the game.
