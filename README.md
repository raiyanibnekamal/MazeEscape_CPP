# Maze Escape

Maze Escape is a small Windows desktop maze game written in C++ with the Win32 API and GDI. Explore a randomly generated dungeon, collect useful items, avoid enemies, use hints to find the exit, and try to finish with the highest score.

The project is also a practical data-structures and algorithms demonstration: the maze is a graph, maze generation uses an iterative depth-first backtracker, hints use BFS, enemy movement uses Dijkstra's algorithm, undo uses a stack, and the action history uses a queue.

![Build status](https://github.com/raiyanibnekamal/MazeEscape_CPP/actions/workflows/build.yml/badge.svg)

## Download and play

**Windows:** [Download the latest Maze Escape release](https://github.com/raiyanibnekamal/MazeEscape_CPP/releases/latest/download/maze_escape_gui.exe), then run `maze_escape_gui.exe`.

**Browser:** [Play the live Web Edition](https://raiyanibnekamal.github.io/MazeEscape_CPP/). It is the same maze adventure rebuilt for HTML5 Canvas, so it works without installing anything.

The download link is updated automatically whenever a version tag is pushed. You can also download the executable from the [latest GitHub release](https://github.com/raiyanibnekamal/MazeEscape_CPP/releases).

## Build from source

Clone the repository and run the included script from a Windows Command Prompt or PowerShell:

```bash
git clone https://github.com/raiyanibnekamal/MazeEscape_CPP.git
cd MazeEscape_CPP
run.bat
```

You need **MinGW g++** and `windres` on PATH. [MSYS2](https://www.msys2.org/), [WinLibs](https://winlibs.com/), and Code::Blocks MinGW are suitable options.

### VS Code

Open the cloned folder and press **F5**. The default build task compiles `maze_escape_gui.exe` and launches the game. You can also run `build.bat` directly.

### Manual build

```bat
build.bat
```

The GitHub Actions workflow builds the Windows executable on every push and publishes it as a release asset for version tags such as `v1.0.0`.

The web edition is deployed automatically to GitHub Pages whenever `web/` changes on `main`.

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

## About

This project is designed as a self-contained Win32 C++ game with no third-party runtime libraries. It combines a playable dungeon adventure with clear examples of graph traversal, shortest-path search, game-state management, and native Windows GUI programming.

## License

Maze Escape is available under the [MIT License](LICENSE).
