#!/usr/bin/env bash
# Build Shadow Ninja (MinGW-w64 / msys2 with freeglut).
# Run from an MSYS2 MinGW64 shell, or ensure the mingw64 g++ is on PATH.
set -e
g++ ninja.cpp -o ninja.exe -O2 -std=c++17 \
  -DFREEGLUT_STATIC -DGLUT_DISABLE_ATEXIT_HACK \
  -static -static-libgcc -static-libstdc++ \
  -lfreeglut -lopengl32 -lglu32 -lwinmm -lgdi32 -luser32 -lkernel32 -lole32
echo "Build OK -> ninja.exe"
