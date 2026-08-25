@echo off
setlocal

cd /d "%~dp0"

if not defined ANDROID_HOME (
    if defined ANDROID_SDK_ROOT (
        set "ANDROID_HOME=%ANDROID_SDK_ROOT%"
    ) else (
        set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
    )
)
set "ANDROID_SDK_ROOT=%ANDROID_HOME%"

if not exist "%ANDROID_HOME%" (
    echo ERROR: Android SDK was not found at:
    echo   %ANDROID_HOME%
    echo Set ANDROID_HOME to the correct SDK directory and try again.
    exit /b 1
)

if not exist "android-project\gradlew.bat" (
    echo ERROR: android-project\gradlew.bat was not found.
    exit /b 1
)

echo Building release APK...
call "android-project\gradlew.bat" -p android-project assembleRelease --console=plain
if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

set "APK=%CD%\android-project\app\build\outputs\apk\release\app-release.apk"
if not exist "%APK%" (
    echo ERROR: Gradle completed, but the APK was not found at:
    echo   %APK%
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL
echo APK: %APK%
exit /b 0
