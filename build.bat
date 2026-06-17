@echo off
REM ===========================================================================
REM  Build Shadow Ninja  (Windows, MinGW-w64 / msys2 with freeglut)
REM  Edit MINGW below if your msys2 install lives somewhere else.
REM ===========================================================================
set "MINGW=C:\msys64\mingw64\bin"

"%MINGW%\g++.exe" ninja.cpp -o ninja.exe -O2 -std=c++17 ^
  -DFREEGLUT_STATIC -DGLUT_DISABLE_ATEXIT_HACK ^
  -static -static-libgcc -static-libstdc++ ^
  -lfreeglut -lopengl32 -lglu32 -lwinmm -lgdi32 -luser32 -lkernel32 -lole32

if %errorlevel%==0 (echo. & echo Build OK  -^>  ninja.exe) else (echo. & echo Build FAILED)
