@echo off
setlocal

set ROOT=%~dp0..
set BUILD_DIR=%ROOT%\build\windows-clang-debug
set EXE=%BUILD_DIR%\WindowManager.exe
set WINDEPLOYQT=C:\Qt\6.10.2\msvc2022_64\bin\windeployqt6.exe

echo ================================
echo  WindowManager - Build Script
echo ================================
echo.

rem Load MSVC + clang-cl environment
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to load VS Build Tools environment.
    pause & exit /b 1
)

rem Delete previous build output for a guaranteed clean build
echo [1/3] Cleaning previous build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

rem Configure
echo [2/3] Configuring...
cmake --preset windows-clang-debug -S "%ROOT%" >nul
if errorlevel 1 (
    echo ERROR: CMake configure failed.
    pause & exit /b 1
)

rem Build
echo [3/3] Building...
cmake --build "%BUILD_DIR%" --target WindowManager
if errorlevel 1 (
    echo ERROR: Build failed.
    pause & exit /b 1
)

rem Deploy Qt DLLs so the exe is self-contained
echo.
echo Deploying Qt runtime...
"%WINDEPLOYQT%" "%EXE%"
if errorlevel 1 (
    echo ERROR: windeployqt6 failed.
    pause & exit /b 1
)

echo.
echo ================================
echo  Build complete!
echo  %EXE%
echo ================================
echo.

rem Open the output folder in Explorer
explorer "%BUILD_DIR%"

endlocal
