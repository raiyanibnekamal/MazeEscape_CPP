@echo off
cd /d "%~dp0"
if not exist maze_escape_gui.exe call build.bat
if exist maze_escape_gui.exe (
  start "" maze_escape_gui.exe
) else (
  echo Could not build or find maze_escape_gui.exe
  exit /b 1
)
