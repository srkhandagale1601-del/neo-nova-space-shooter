# VECTOR STORM - A 2D Computer Graphics Shooter

A colourful, easy-to-read 2D plane shooter written in C++ with OpenGL and FreeGLUT. You fly a cartoon fighter plane over a different planet in every sector, shoot waves of enemy planes, collect power-ups and beat a boss to move on. Every picture is drawn in code (polygons, triangles, circles and lines) - there are no image files, and the sound is synthesised too.

The game doubles as a Computer Graphics project: the main menu has two learning screens.

- **HOW TO PLAY** - the rules, with the real enemy sprites and power-up badges.
- **HOW IT WORKS (CODE)** - 10 pages of real code from this game, shown with line numbers and explained line by line (game loop, movement, shooting, enemies, hit detection, score, drawing the plane, the scrolling world, power-ups and boss, project map).
- **CG CONCEPTS** - the list of graphics techniques used and where they live.

## How to play

- **Goal:** shoot enemy planes to score. Fill the **BOSS** bar at the top, beat the boss, and fly on to the next planet.
- **You lose a heart** (you have 5) if a red bullet hits you, an enemy plane hits you, or an enemy **reaches the ground**.
- **Your combo** grows (up to x5) when you shoot planes quickly one after another. It resets when you are hit or an enemy lands.
- A short hint bar at the bottom explains the controls for the first 15 seconds of every game.

### Enemies

| Enemy | Look | Behaviour | Points |
| :--- | :--- | :--- | :--- |
| Scout | White body, red wings | Flies straight down, shoots straight down | 10 |
| Interceptor | Yellow and orange | Fires shots **aimed** at your plane | 25 |
| Bomber | Big, blue-grey, 2 hits (smokes after the first) | Fires a 3-bullet fan | 35 |

Enemies spawn in small waves (3 to 5 planes). They are spread over random lanes and different heights, so they never fly in a straight line side by side. They always fly **forward** (down the screen) faster than the ground scrolls. A new wave arrives when the screen is clear.

### Planets (sectors)

| Sector | World | Scenery |
| :--- | :--- | :--- |
| 1 | Earth coast | Grass, sea, winding path, trees, houses, boats |
| 2 | Desert planet | Sand, oasis lake, cacti, huts, rocks |
| 3 | Ice planet | Snow, frozen sea, pines, igloos, icebergs |
| 4 | Volcano planet | Ash ground, lava river, volcanoes, dark rocks |
| 5 | Deep space | Twinkling stars, nebula, ringed planets, asteroids |

Beating a sector boss gives a free shield and an EMP bomb and starts the next planet. After sector 5 the planets repeat with higher score targets.

### Power-ups (round lettered badges dropped by enemies)

| Badge | Effect |
| :--- | :--- |
| S | Shield: absorbs 3 hits |
| 3 | Triple shot (about 11 s) |
| 5 | Five-way shot (10 s) |
| R | Rapid fire (10 s) |
| W | Speed boost (12 s) |
| B | +1 EMP bomb |
| T | Slow-motion enemies (8 s) |
| + | +1 life (max 5) |

## Controls

| Action | Keys |
| :--- | :--- |
| Move | Arrow keys or `W` `A` `S` `D` |
| Fire | Hold `Space` |
| EMP bomb (clears enemy bullets, hurts everything) | `B` |
| Pause / resume | `P` or `Esc` |
| Quick restart | `R` |
| Menus | Up/Down or `W`/`S`, `Enter` or `Space` to confirm, or click with the mouse |
| HOW IT WORKS pages | Left/Right arrows or `A`/`D` (or the PREVIOUS / NEXT buttons), `Esc` for the menu |

Developer toggles during play: `F1` CG Lab panel (also shows the radar), `F2` wireframe, `F3` CRT scanlines, `F4` hit-box debug, `F5` software raster algorithms.

## Build and run (Windows)

Requires g++ (MinGW-w64 or MSYS2 UCRT64/MINGW64) with C++11. The FreeGLUT runtime DLLs are included in this folder.

```powershell
.\build.bat run     # compile and launch
.\build.bat         # compile only
.\build.bat clean   # remove game.exe
```

Linked libraries: `-lfreeglut -lopengl32 -lglu32 -lwinmm`.

## Project structure

```text
neo-nova-space-shooter/
  main.cpp                          Game logic: update(), display(), input, HUD, menus, planes, waves, boss
  src/Graphics/Starfield.cpp        The scrolling 2D worlds (5 planets) and the sector table
  src/Graphics/Renderer.cpp         Text and circle drawing helpers (outlined text, stroke title)
  src/Graphics/ParticleSystem.cpp   Explosions, smoke trails, floating score text
  src/Graphics/CGLab.cpp            Glow/lights, Bezier, 3D mesh renderer, clipping, raster algorithms
  src/Entities/Powerup.cpp          Power-up badges: spawn, fall, draw
  src/Audio/SoundEngine.cpp         Procedural sound effects and music (WinMM)
  include/                          Headers (Config.hpp holds window size and game states)
  build.bat, Makefile, CMakeLists.txt   Build files
```

## Tuning the game

- **Difficulty:** `spawnWave()` in `main.cpp` (wave size, enemy speed, lanes); the `shootTimer` code in `update()` (how often enemies fire); `spawnBoss()` (boss health).
- **Sector goals:** the `targetScore` values in `sectorThemes` in `Starfield.cpp`, and `sectorTargetScore()` in `main.cpp`.
- **Look:** `drawPlayer()` and `drawEnemyPlane()` for the planes; the palette table and scenery functions in `Starfield.cpp` for the worlds.

See [IMPROVEMENTS.md](IMPROVEMENTS.md) for the longer roadmap.
