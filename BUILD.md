# Building Pgn2BinMT 2.0

The supplied executable is a native Windows x86-64 GUI, statically linked with
the C++ runtime. The source and direct code dependencies are included.
The GUI, runtime messages and generated reports use English.

## Windows cross-build used for this package

Compiler: Ubuntu g++-mingw-w64-x86-64-posix 13.2.0-6ubuntu1+26.1,
reporting GCC 13-posix. Headers: mingw-w64 11.0.1-3build1.
Binutils: 2.41.90.20240122-1ubuntu1+11.4. POSIX thread runtime statically linked.

```sh
CXX=x86_64-w64-mingw32-g++-posix ./build-windows.sh
```

The script compiles src/windows.cpp, src/core.cpp and src/polyglot/*.cpp.
MinGW windres embeds src/app.rc and assets/Pgn2BinMT.ico into the executable.
Set WINDRES to the resource compiler path when it is not in PATH.
BuildManifest.json records source hashes and the supplied executable hash.
A different compiler may produce a different binary. The EXE is unsigned.

## Building on Windows

Install MSYS2 with a 64-bit MinGW/UCRT g++ compiler. build-windows.cmd searches
C:\msys64 and D:\msys64, mingw64 then ucrt64, and falls back to g++.exe in PATH.
Run it from the extracted package. This route was documented but not run here;
the supplied EXE was produced using the cross-build above.

## Native core and tests on Linux

Native compiler: g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0.

```sh
./build-linux.sh
python3 -m pip install chess==1.11.2
python3 tests/test_core.py --classic /usr/games/polyglot
python3 tests/test_dedup.py
python3 tests/test_safety.py
```

The independent chess reference is python-chess 1.11.2. The optional classic
reference is PolyGlot 2.0.4+git20210322-1. Without --classic, only the independent
python-chess comparison runs. --million adds long RC1-compatible stress tests.
Classic comparisons disable deduplication explicitly, preserving RC1 behavior.
The separate deduplication suite tests the RC2 default behavior.
Classic BIN comparisons check key/move/weight/learn content; equal-weight move
ordering can differ. This program uses a deterministic move tiebreaker.

The CLI is a development/test helper. Daily Windows use needs only the EXE.

```sh
build/Pgn2BinMT-cli --threads 6 --ply 60 --min-games 3 \
  --output /existing/output/folder/Book.bin source.pgn
```

Options: --threads, --ply, --min-games, --memory-mib, --output, --overwrite,
--no-dedup, --run-folder. Deduplication is enabled by default.
The GUI always uses dated run subfolders. --run-folder enables that same
behavior in the CLI; without it, the CLI preserves its exact-path behavior. Ctrl+C requests controlled stop.
The GUI uses automatic RAM. The CLI memory limit lets tests force disk batches.

RC2.9 batching regression: python3 tests/test_queue_batches.py
Performance comparison: see tests/benchmark_queue.py. Run benchmarks serially.
