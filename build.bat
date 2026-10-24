@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   2D Space Shooter - Build System
echo ========================================================

:: Check for clean argument
if "%1"=="clean" (
    echo Cleaning build artifacts...
    if exist game.exe del game.exe
    echo Done.
    goto :end
)

:: Locate g++ compiler
set "COMPILER_FOUND=0"

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    set "COMPILER_FOUND=1"
    set "CXX=g++"
) else (
    if exist "C:\msys64\ucrt64\bin\g++.exe" (
        set "PATH=C:\msys64\ucrt64\bin;!PATH!"
        set "COMPILER_FOUND=1"
        set "CXX=C:\msys64\ucrt64\bin\g++.exe"
    ) else if exist "C:\msys64\mingw64\bin\g++.exe" (
        set "PATH=C:\msys64\mingw64\bin;!PATH!"
        set "COMPILER_FOUND=1"
        set "CXX=C:\msys64\mingw64\bin\g++.exe"
    )
)

if "!COMPILER_FOUND!"=="0" (
    echo [ERROR] g++ compiler not found in PATH or standard MSYS2 directories.
    echo Please ensure MinGW or MSYS2 is installed and g++ is accessible.
    exit /b 1
)

echo Using compiler: !CXX!
echo Compiling main.cpp...

!CXX! -O2 -Wall main.cpp -o game.exe -lfreeglut -lopengl32 -lglu32 -lwinmm
if %errorlevel% neq 0 (
    echo [ERROR] Build failed! Check the error messages above.
    exit /b %errorlevel%
)

:: Ensure libfreeglut.dll is in the project directory
if not exist "libfreeglut.dll" (
    if exist "C:\msys64\ucrt64\bin\libfreeglut.dll" (
        copy /y "C:\msys64\ucrt64\bin\libfreeglut.dll" "libfreeglut.dll" >nul
    ) else if exist "C:\msys64\mingw64\bin\libfreeglut.dll" (
        copy /y "C:\msys64\mingw64\bin\libfreeglut.dll" "libfreeglut.dll" >nul
    )
)

echo [SUCCESS] game.exe built successfully!

:: Check if user requested to run the game
if "%1"=="run" (
    echo Launching game.exe...
    start "" game.exe
)

:end
endlocal
