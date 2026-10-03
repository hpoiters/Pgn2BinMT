#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${CXX:=x86_64-w64-mingw32-g++}"
: "${WINDRES:=x86_64-w64-mingw32-windres}"
mkdir -p build
"$WINDRES" -I. src/app.rc -O coff -o build/app-res.o
"$CXX" -std=c++17 -O3 -DNDEBUG -static -static-libgcc -static-libstdc++ -pthread -municode -mwindows -Isrc src/windows.cpp src/core.cpp src/polyglot/*.cpp build/app-res.o -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lpsapi -o Pgn2BinMT.exe
