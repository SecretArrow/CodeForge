@echo off
rem ============================================================
rem  CodeForge - Release build (configure + compile)
rem  Requires: Qt 6.5+ (set QT6_DIR), CMake 3.21+, Ninja
rem ============================================================
setlocal

if "%QT6_DIR%"=="" (
    echo [ERROR] QT6_DIR is not set. Point it to your Qt installation, e.g.:
    echo    set QT6_DIR=C:\Qt\6.7.3\msvc2019_64
    exit /b 1
)

where cmake >nul 2>nul || (echo [ERROR] cmake not found in PATH & exit /b 1)
where ninja  >nul 2>nul || echo [WARN] ninja not found; falling back to the default generator

if exist build\windows-release\build.ninja (
    cmake --build build\windows-release -j %NUMBER_OF_PROCESSORS%
) else (
    cmake --preset windows-release || exit /b 1
    cmake --build build\windows-release -j %NUMBER_OF_PROCESSORS%
)

if %ERRORLEVEL%==0 (
    echo.
    echo [OK] Release build finished: build\windows-release\bin\CodeForge.exe
    echo      Next step: package.bat  (deploys Qt DLLs + optional installer)
)
endlocal
