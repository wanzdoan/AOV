@echo off
REM ============================================================================
REM  AOV Zygisk — Windows Build Script
REM  Builds the Zygisk .so with NDK CMake + Ninja, then packages flashable ZIP
REM  Usage: scripts\build.bat  (run from anywhere)
REM  Env override: set ANDROID_NDK=D:\path\to\ndk
REM ============================================================================

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%.." >nul
set "PROJECT_DIR=%CD%"
popd >nul

set "JNI_DIR=%PROJECT_DIR%\jni"
set "OUT_DIR=%PROJECT_DIR%\build"
set "ZYGISK_DIR=%PROJECT_DIR%\zygisk"

REM ── Configuration ───────────────────────────────────────────────────────────
if defined ANDROID_NDK (
    set "NDK_PATH=%ANDROID_NDK%"
) else (
    set "NDK_PATH=D:\Android\Sdk\ndk\27.2.12479018"
)
set "ABI=arm64-v8a"
set "MODULE_ID=aov_zygisk"
for /f "tokens=2 delims==" %%V in ('findstr /b "version=" "%PROJECT_DIR%\module.prop"') do set "MODULE_VERSION=%%V"

REM ── Check NDK ───────────────────────────────────────────────────────────────
if not exist "%NDK_PATH%\build\cmake\android.toolchain.cmake" (
    echo [ERROR] NDK toolchain not found at: %NDK_PATH%
    echo Set ANDROID_NDK env var to your NDK path and retry.
    exit /b 1
)

echo ============================================
echo   AOV Zygisk Build Script
echo   Project: %PROJECT_DIR%
echo   NDK: %NDK_PATH%
echo   ABI: %ABI%
echo ============================================

REM ── Check required vendor files ─────────────────────────────────────────────
if not exist "%JNI_DIR%\vendor\imgui\imgui.cpp" (
    echo [ERROR] ImGui sources missing in jni\vendor\imgui\
    exit /b 1
)
if not exist "%JNI_DIR%\vendor\dobby\lib\%ABI%\libdobby.a" (
    echo [ERROR] Dobby lib missing: jni\vendor\dobby\lib\%ABI%\libdobby.a
    exit /b 1
)
if not exist "%JNI_DIR%\vendor\zygisk\include\zygisk.hpp" (
    echo [ERROR] Zygisk header missing: jni\vendor\zygisk\include\zygisk.hpp
    exit /b 1
)

REM ── Build with CMake via NDK ─────────────────────────────────────────────────
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo.
echo [1/3] Configuring CMake...
cmake -S "%JNI_DIR%" -B "%OUT_DIR%\cmake_%ABI%" ^
    -DCMAKE_TOOLCHAIN_FILE="%NDK_PATH%\build\cmake\android.toolchain.cmake" ^
    -DANDROID_ABI=%ABI% ^
    -DANDROID_PLATFORM=android-21 ^
    -DANDROID_STL=c++_static ^
    -DCMAKE_BUILD_TYPE=Release ^
    -G "Ninja"

if errorlevel 1 (
    echo [ERROR] CMake configuration failed!
    exit /b 1
)

echo.
echo [2/3] Building %MODULE_ID%.so...
cmake --build "%OUT_DIR%\cmake_%ABI%" --config Release

if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)

REM ── Copy output .so to zygisk/ ───────────────────────────────────────────────
echo.
echo [3/3] Packaging...

if not exist "%ZYGISK_DIR%" mkdir "%ZYGISK_DIR%"
copy /Y "%OUT_DIR%\cmake_%ABI%\lib%MODULE_ID%.so" "%ZYGISK_DIR%\%ABI%.so" >nul
if errorlevel 1 (
    echo [ERROR] Copy .so failed!
    exit /b 1
)
echo Copied: %ZYGISK_DIR%\%ABI%.so

REM ── Create flashable ZIP (staged copy, excludes build/ jni/ scripts/) ───────
set "ZIP_NAME=%MODULE_ID%_%MODULE_VERSION%.zip"
set "ZIP_PATH=%PROJECT_DIR%\%ZIP_NAME%"
set "STAGE=%OUT_DIR%\stage"

if exist "%STAGE%" rmdir /S /Q "%STAGE%"
mkdir "%STAGE%"
copy /Y "%PROJECT_DIR%\module.prop"   "%STAGE%\" >nul
copy /Y "%PROJECT_DIR%\customize.sh"  "%STAGE%\" >nul
copy /Y "%PROJECT_DIR%\post-mount.sh" "%STAGE%\" >nul
copy /Y "%PROJECT_DIR%\post-fs-data.sh" "%STAGE%\" >nul
copy /Y "%PROJECT_DIR%\service.sh" "%STAGE%\" >nul
mkdir "%STAGE%\zygisk"
copy /Y "%ZYGISK_DIR%\%ABI%.so" "%STAGE%\zygisk\" >nul
if exist "%PROJECT_DIR%\webroot" (
    mkdir "%STAGE%\webroot"
    xcopy /E /I /Y "%PROJECT_DIR%\webroot" "%STAGE%\webroot" >nul
)
mkdir "%STAGE%\META-INF\com\google\android"
copy /Y "%PROJECT_DIR%\META-INF\com\google\android\update-binary"  "%STAGE%\META-INF\com\google\android\" >nul
copy /Y "%PROJECT_DIR%\META-INF\com\google\android\updater-script" "%STAGE%\META-INF\com\google\android\" >nul

if exist "%ZIP_PATH%" del "%ZIP_PATH%"

REM NOTE: Do NOT use .NET ZipFile here — it stores backslash separators,
REM which Android busybox unzip treats as literal filename chars, so
REM 'zygisk/*' patterns silently match nothing on-device. 7z/Python
REM always write forward slashes.
where 7z >nul 2>nul
if not errorlevel 1 (
    pushd "%STAGE%" >nul
    7z a -tzip "%ZIP_PATH%" * -r -mx=9 >nul
    set "ZIPERR=!errorlevel!"
    popd >nul
) else (
    where python >nul 2>nul
    if not errorlevel 1 (
        python -c "import zipfile,os; s=r'%STAGE%'; z=zipfile.ZipFile(r'%ZIP_PATH%','w',zipfile.ZIP_DEFLATED); [z.write(os.path.join(r,n),os.path.relpath(os.path.join(r,n),s).replace(os.sep,'/')) for r,n,_ in os.walk(s) for n in n]; z.close()"
        set "ZIPERR=!errorlevel!"
    ) else (
        echo [ERROR] Need 7z or python to create the ZIP with forward slashes!
        exit /b 1
    )
)

if not "!ZIPERR!" == "0" (
    echo [ERROR] ZIP creation failed!
    exit /b 1
)

REM ── Cleanup stage dir ─────────────────────────────────────────────────────────
rmdir /S /Q "%STAGE%" 2>nul

echo.
echo ============================================
echo   BUILD COMPLETE
echo   .so: %ZYGISK_DIR%\%ABI%.so
echo   ZIP: %ZIP_PATH%
echo ============================================
echo.
echo Next steps:
echo   1. Copy %ZIP_NAME% to your Android device
echo   2. Flash via KernelSU Next Manager or Magisk
echo       - Requires ZygiskNext enabled + reboot
echo   3. Reboot, launch AOV, tap [M] button to open menu
echo.
