@echo off
setlocal EnableExtensions

cd /d "%~dp0"

set "QT=D:\Qt\6.11.2\mingw_64"
set "MINGW=D:\Qt\Tools\mingw1310_64"

set "PATH=%MINGW%\bin;%QT%\bin;%PATH%"

echo ==========================================
echo   VMF Prop Merger - Release Build
echo ==========================================
echo.

if not exist "%MINGW%\bin\g++.exe" (
    echo ERROR: Qt MinGW compiler not found.
    pause
    exit /b 1
)

if not exist "%QT%\bin\windeployqt.exe" (
    echo ERROR: windeployqt not found.
    pause
    exit /b 1
)

echo [1/3] Compiling...

g++ -std=c++17 -O2 -mwindows ^
    -Iinclude ^
    -I"%QT%\include" ^
    -I"%QT%\include\QtCore" ^
    -I"%QT%\include\QtGui" ^
    -I"%QT%\include\QtWidgets" ^
    gui.cpp ^
    -o vmfpropmerger_gui.exe ^
    -L"%QT%\lib" ^
    -lQt6Widgets ^
    -lQt6Gui ^
    -lQt6Core ^
    -lshell32 ^
    -lole32

if errorlevel 1 (
    echo.
    echo BUILD FAILED.
    pause
    exit /b 1
)

echo.
echo [2/3] Deploying Qt runtime...

"%QT%\bin\windeployqt.exe" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    vmfpropmerger_gui.exe

if errorlevel 1 (
    echo.
    echo DEPLOYMENT FAILED.
    pause
    exit /b 1
)

echo.
echo [3/3] Build complete.
echo.
echo Output:
echo %CD%\vmfpropmerger_gui.exe
echo.
echo Qt runtime files have been deployed beside the executable.
echo.

pause