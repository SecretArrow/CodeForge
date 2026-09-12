@echo off
rem ============================================================
rem  CodeForge - Packaging
rem   1. Release build
rem   2. windeployqt -> release\ (self-contained folder)
rem   3. Optional: Inno Setup installer if ISCC.exe is found
rem ============================================================
setlocal enabledelayedexpansion

if "%QT6_DIR%"=="" (
    echo [ERROR] QT6_DIR is not set (e.g. C:\Qt\6.7.3\msvc2019_64)
    exit /b 1
)

rem --- 1. build ---
call build-release.bat || exit /b 1

rem --- 2. stage output ---
set STAGE=%~dp0release
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%" || exit /b 1
copy /y build\windows-release\bin\CodeForge.exe "%STAGE%" >nul || exit /b 1

rem --- 3. deploy Qt runtime ---
"%QT6_DIR%\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%STAGE%\CodeForge.exe" || exit /b 1

echo.
echo [OK] Portable folder created: release\CodeForge.exe
echo      Copy release\ anywhere; place a "portable" folder next to it for portable mode.

rem --- 4. optional installer ---
set ISCC="%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist %ISCC% set ISCC="%ProgramFiles%\Inno Setup 6\ISCC.exe"
if exist %ISCC% (
    echo [..] Building installer with Inno Setup...
    %ISCC% installer\CodeForge.iss
    if !ERRORLEVEL!==0 (
        echo [OK] Installer created: installer\Output\CodeForgeSetup.exe
    )
) else (
    echo [INFO] Inno Setup 6 not found - skipping installer.
    echo        Install from https://jrsoftware.org/isinfo.php to build CodeForgeSetup.exe
)
endlocal
