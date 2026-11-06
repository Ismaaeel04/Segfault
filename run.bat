@echo off
rem ==========================================================================
rem  Segfault - build and run (Windows)
rem  Double-click this file. The first run downloads raylib (needs internet)
rem  and takes 1-2 minutes. Later runs are fast.
rem ==========================================================================
setlocal
cd /d "%~dp0"
title Segfault - build and run

rem ---- 1) find CMake: first in PATH, then the one inside Visual Studio ----
set "CMAKE="
where cmake >nul 2>nul && set "CMAKE=cmake"
if defined CMAKE goto have_cmake

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_cmake
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto no_cmake
set "VSCMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if exist "%VSCMAKE%" set "CMAKE=%VSCMAKE%"
if defined CMAKE goto have_cmake

:no_cmake
echo.
echo  [ERROR] CMake was not found.
echo  Install Visual Studio 2022 with the workload
echo  "Desktop development with C++"  (it includes CMake and the compiler),
echo  or install CMake from https://cmake.org/download/
echo.
pause
exit /b 1

:have_cmake
echo Using CMake: "%CMAKE%"
echo.

rem ---- 2) configure (only the first time) ----
if exist "build\CMakeCache.txt" goto build
echo [1/3] Configuring (downloads raylib the first time)...
"%CMAKE%" -S . -B build
if errorlevel 1 goto fail

:build
rem ---- 3) build the app and the tests ----
echo.
echo [2/3] Building...
"%CMAKE%" --build build --config Release --parallel
if errorlevel 1 goto fail

rem ---- 4) run ----
set "EXE=build\Release\Segfault.exe"
if not exist "%EXE%" set "EXE=build\Segfault.exe"
if not exist "%EXE%" goto fail
echo.
echo [3/3] Starting Segfault...
start "" "%EXE%"
exit /b 0

:fail
echo.
echo  [ERROR] Something went wrong. Copy the messages above and ask for help.
echo  Tip: if you changed compilers, delete the "build" folder and try again.
echo.
pause
exit /b 1
