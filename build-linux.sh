#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
g++ -std=c++17 -O3 -DNDEBUG -pthread -Isrc src/cli.cpp src/core.cpp src/polyglot/*.cpp -o build/Pgn2BinMT-cli
