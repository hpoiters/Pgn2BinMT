@echo off
setlocal
cd /d "%~dp0"
set "P2B_CXX=g++.exe"
if exist D:\msys64\mingw64\bin\g++.exe set "P2B_CXX=D:\msys64\mingw64\bin\g++.exe"
if exist C:\msys64\mingw64\bin\g++.exe set "P2B_CXX=C:\msys64\mingw64\bin\g++.exe"
if exist D:\msys64\ucrt64\bin\g++.exe set "P2B_CXX=D:\msys64\ucrt64\bin\g++.exe"
if exist C:\msys64\ucrt64\bin\g++.exe set "P2B_CXX=C:\msys64\ucrt64\bin\g++.exe"
set "P2B_SOURCES="
for %%I in ("%P2B_CXX%") do set "P2B_WINDRES=%%~dpIwindres.exe"
if not exist "%P2B_WINDRES%" set "P2B_WINDRES=windres.exe"
if not exist build mkdir build
"%P2B_WINDRES%" -I. src\app.rc -O coff -o build\app-res.o
if errorlevel 1 goto failed
for %%F in (src\polyglot\*.cpp) do call set "P2B_SOURCES=%%P2B_SOURCES%% %%F"
"%P2B_CXX%" -std=c++17 -O3 -DNDEBUG -static -static-libgcc -static-libstdc++ -pthread -municode -mwindows -Isrc src\windows.cpp src\core.cpp %P2B_SOURCES% build\app-res.o -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lpsapi -o Pgn2BinMT.exe
if errorlevel 1 goto failed
echo Pgn2BinMT.exe has been built.
exit /b 0
:failed
echo Build failed. See BUILD.md for compiler requirements.
exit /b 1
