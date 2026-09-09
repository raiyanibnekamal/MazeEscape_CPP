@echo off
cd /d "%~dp0"

where g++ >nul 2>&1
if errorlevel 1 (
  echo g++ not found. Install MinGW-w64 and add it to PATH.
  echo Example: CodeBlocks MinGW, WinLibs, or MSYS2.
  exit /b 1
)

if exist maze_icon.ico (
  where windres >nul 2>&1
  if not errorlevel 1 (
    windres maze_escape_gui.rc -O coff -o maze_escape_gui_res.o
  )
)

if exist maze_escape_gui_res.o (
  g++ maze_escape_gui.cpp maze_escape_gui_res.o -o maze_escape_gui.exe -std=c++17 -mwindows -lgdi32 -lmsimg32
) else (
  g++ maze_escape_gui.cpp -o maze_escape_gui.exe -std=c++17 -mwindows -lgdi32 -lmsimg32
)

if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Built maze_escape_gui.exe
exit /b 0
