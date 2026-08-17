@echo off
setlocal enabledelayedexpansion

REM Accept optional arguments: build.bat [CMAKE_PATH] [GCC_PATH]
if not "%~1"=="" set "CMAKE_PATH=%~1"
if not "%~2"=="" set "GCC_PATH=%~2"

echo.
echo Building Reactive Drop VMF Prop Merger...
echo ===========================================
echo.

REM Locate CMake
set "CMAKE_CMD="
if defined CMAKE_PATH (
    if exist "%CMAKE_PATH%\bin\cmake.exe" set "CMAKE_CMD=%CMAKE_PATH%\bin\cmake.exe"
    if exist "%CMAKE_PATH%\cmake.exe" set "CMAKE_CMD=%CMAKE_PATH%\cmake.exe"
)

if "%CMAKE_CMD%"=="" (
    where cmake >nul 2>&1
    if not errorlevel 1 set "CMAKE_CMD=cmake"
)

if "%CMAKE_CMD%"=="" (
    echo ERROR: CMake was not found.
    echo.
    echo Provide a portable CMake location:
    echo   build.bat "C:\path\to\cmake"
    echo   or
    echo   set CMAKE_PATH=C:\path\to\cmake
    echo   build.bat
    echo.
    echo You can also install CMake and add it to PATH.
    echo.
    pause
    exit /b 1
)

REM Locate a GCC/MinGW toolchain
set "GCC_BIN="
if defined GCC_PATH (
    if exist "%GCC_PATH%\bin\gcc.exe" set "GCC_BIN=%GCC_PATH%\bin\"
    if exist "%GCC_PATH%\bin\g++.exe" if "%GCC_BIN%"=="" set "GCC_BIN=%GCC_PATH%\bin\"
    if "%GCC_BIN%"=="" if exist "%GCC_PATH%\gcc.exe" set "GCC_BIN=%GCC_PATH%\"
    if "%GCC_BIN%"=="" if exist "%GCC_PATH%\g++.exe" set "GCC_BIN=%GCC_PATH%\"
)

if "%GCC_BIN%"=="" (
    where gcc >nul 2>&1
    if not errorlevel 1 for /f "delims=" %%I in ('where gcc 2^>nul') do (
        set "GCC_BIN=%%~dpI"
        goto gcc_found
    )
)

:gcc_found
if "%GCC_BIN%"=="" (
    echo ERROR: No GCC toolchain was found.
    echo.
    echo Provide a portable GCC location:
    echo   build.bat "E:\cmake-4.4.2-windows-x86_64" "C:\mingw-w64\mingw64"
    echo   or
    echo   set GCC_PATH=C:\mingw-w64\mingw64
    echo   build.bat
    echo.
    echo If using MinGW, install it and add its bin folder to PATH or set GCC_PATH to it.
    echo.
    pause
    exit /b 1
)

if not exist "%GCC_BIN%gcc.exe" if not exist "%GCC_BIN%g++.exe" (
    echo ERROR: GCC compiler not found in %GCC_BIN%
    echo.
    echo Provide a portable GCC toolchain directory that contains gcc.exe and g++.exe.
    echo   Example: build.bat "C:\cmake" "C:\mingw-w64\mingw64"
    echo.
    pause
    exit /b 1
)

if not exist build mkdir build
cd build

echo Configuring project...
"%CMAKE_CMD%" -G "MinGW Makefiles" -DCMAKE_C_COMPILER="%GCC_BIN%gcc.exe" -DCMAKE_CXX_COMPILER="%GCC_BIN%g++.exe" ..

if errorlevel 1 (
    cd ..
    echo.
    echo ERROR: CMake configuration failed.
    echo.
    echo Make sure the GCC toolchain path is valid and contains gcc.exe and g++.exe.
    echo.
    echo Examples:
    echo   build.bat "E:\cmake-4.4.2-windows-x86_64" "C:\msys64\mingw64"
    echo   build.bat "E:\cmake-4.4.2-windows-x86_64" "C:\mingw-w64\mingw64"
    echo.
    pause
    exit /b 1
)

echo.
echo Compiling project...
"%CMAKE_CMD%" --build . --config Release --parallel 4

if errorlevel 1 (
    cd ..
    echo.
    echo ERROR: Build failed.
    echo.
    pause
    exit /b 1
)

cd ..

echo.
echo ========================================
echo Build complete!
echo.
echo Binary location: %cd%\build\vmfpropmerger.exe
echo.
echo To run: build\vmfpropmerger.exe --help
echo ========================================
echo.
pause