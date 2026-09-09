# Maze Escape (C++ / Win32)

A Windows GUI maze game. Walk with WASD or the on-screen buttons, pick up items, dodge enemies, and reach the exit. Built with the Windows API only (no extra libraries).

Uses a graph for the maze, BFS for hints, Dijkstra for enemy chase, a stack for undo, and a queue for the action log.

## Run from GitHub

This is a **Windows** app. Clone the repo, then either double-click `run.bat` or press **F5** in VS Code / Cursor.

```bash
git clone https://github.com/raiyanibnekamal/MazeEscape_CPP.git
cd MazeEscape_CPP
run.bat
```

You need **MinGW g++** on PATH (CodeBlocks MinGW, [WinLibs](https://winlibs.com/), or MSYS2).

### VS Code / Cursor

1. Open the cloned folder.
2. Press **F5** (or Run → Start Debugging).
3. The default task builds `maze_escape_gui.exe` and launches the window.

You can also run **Terminal → Run Build Task**, then double-click `maze_escape_gui.exe`.

### Manual build

```bat
build.bat
```

Or:

```bat
g++ maze_escape_gui.cpp maze_escape_gui_res.o -o maze_escape_gui.exe -std=c++17 -mwindows -lgdi32 -lmsimg32
```

GitHub Actions also builds the `.exe` on every push. Download it from the **Actions** tab → latest workflow → **MazeEscape** artifact.

## Controls

| Key / button | Action |
| --- | --- |
| WASD or arrow keys | Move |
| N / S / E / W buttons | Move |
| P / Pickup | Take item in this room |
| H / Hint (BFS) | Show shortest path to the exit |
| U / Undo | Go back one room |
| New Maze | Start a new maze |

Pick Small (5×5), Medium (7×7), or Large (9×9) on the start screen, type a name, then click **Begin**.
