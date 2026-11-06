#!/usr/bin/env bash
# Segfault - build and run (macOS / Linux)
# Usage:  ./run.sh      (first run downloads raylib and takes 1-2 minutes)
set -e
cd "$(dirname "$0")"

if ! command -v cmake >/dev/null 2>&1; then
    echo "[ERROR] CMake not found."
    echo "  macOS:  xcode-select --install && brew install cmake"
    echo "  Ubuntu: sudo apt install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev"
    exit 1
fi

if [ ! -f build/CMakeCache.txt ]; then
    echo "[1/3] Configuring (downloads raylib the first time)..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
fi
echo "[2/3] Building..."
cmake --build build --parallel
echo "[3/3] Starting Segfault..."
./build/Segfault
