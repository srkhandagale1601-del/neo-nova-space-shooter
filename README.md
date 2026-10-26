# NEO NOVA: 2D Space Shooter - Sector Defense Edition

A high-octane, arcade-style 2D Space Shooter built in C++ using OpenGL, FreeGLUT, and a custom procedural sound synthesis engine. Blast your way through dynamic galactic sectors, confront formidable capital ship bosses, collect game-changing tactical power-ups, and customize your experience with a full-featured audio and settings suite!

---

## 🌟 What's New in this Major Upgrade

### 🔊 Procedural 16-bit Synthesizer & Dynamic Audio Engine
- **Zero-Dependency WinMM Audio Engine**: Runs directly on a dedicated background thread synthesizing 44.1 kHz 16-bit audio mathematically with zero external audio assets or audio DLLs needed!
- **Polyphonic Arcade Sound Effects**:
  - High-frequency laser sweeps (Standard wingtip blaster, Triple spread, and Penta-Nova cannon)
  - Deep resonant explosions with decaying noise and sub-bass rumbles
  - Crystalline ascending power-up arpeggios
  - Metallic forcefield deflection pings
  - Rocket afterburner whooshes and screen-wide EMP electrical detonations
  - Sector clearance victory fanfare and game over themes
  - Incoming Capital Ship warning klaxon
- **Dynamic Chiptune Synthwave Soundtrack**:
  - Procedural 16-step retro bassline, cosmic lead arpeggios, and synthesized 808-style percussion.
  - Scales and tempos dynamically shift between Sectors and Boss encounters!

---

### 🌌 Level-Wise Progression & Dynamic Themes
The game now features 5 distinct Galactic Sectors with evolving color palettes, parallax starfields, atmospheric nebulae, and specialized enemy squadrons:

| Sector | Theme & Environment | Visual Style | Sector Boss |
| :--- | :--- | :--- | :--- |
| **Sector 1: Orion Abyss** | Deep Space Reconnaissance | Indigo/Navy cosmos with ice-cyan stars and dust | *Crimson Dread-Scout* |
| **Sector 2: Solar Flare** | Solar Storm Hazard Zone | Scorching crimson & amber skies with solar embers | *Solar Destroyer Helios* |
| **Sector 3: Acid Void** | Bio-Corrosive Asteroid Expanse | Toxic emerald & acid-teal clouds with armed corvettes | *Venom Hive Carrier* |
| **Sector 4: Warp Stream** | Hyper-Space Quantum Corridor | Synthwave magenta/violet skies with hyper-speed stars | *Quantum Shadow Flagship* |
| **Sector 5: Omega Core** | Apex Capital Fleet Core | Blazing golden starbursts and intense particle storms | *Omega Mothership Prime* |

- **Capital Ship Boss Battles**: Reach the sector target score to trigger an emergency siren! Bosses feature real-time health bars, multi-cannon tracking fire, and radial plasma bursts.
- Defeating a Boss initiates a **Warp Jump**, restores shields, awards bonus EMP bombs, and advances to the next sector!

---

### ⚡ Enhanced Power-Ups & Ship Enhancements
- 🛡️ **[S] Shield Forcefield**: Deploys an animated hexagonal energy bubble absorbing 2 to 3 direct hits.
- ⚡ **[3] Triple Spread Cannons**: 3-way laser spread (11s duration).
- 🌟 **[5] Penta-Nova Cannons**: 5-way wide blaster barrage that decimates whole formations (10s duration).
- 🔥 **[R] Rapid Overcharge**: Triples blaster firing cadence with glowing golden barrels (10s duration).
- 🚀 **[W] Warp Hyper-Thruster**: Increases flight speed by +60% with electric-blue afterburner exhaust and glowing warp particle ribbons (12s duration).
- 💣 **[B] EMP Nova Bomb**: Clears all enemy projectiles on screen, obliterates standard fighters, and inflicts 120 damage on Bosses (press **`B`**).
- ⏱️ **[T] Chrono-Warp**: Matrix-like time dilation that slows enemy motion and hostile lasers to 40% speed for 8 seconds.
- 💖 **[+] Nano-Repair**: Restores +1 hull integrity (up to 5 max).

---

### ⚙️ Interactive Settings & Menu System
- **Main Menu**: Smooth interactive navigation between Mission Start, Settings, How-to-Play, and Quit.
- **In-Game Pause Menu**: Press `P` or `ESC` during combat to access **Resume**, **Restart Sector**, **Settings**, or **Main Menu**.
- **Settings Screen**:
  - **Master Volume Slider** (0% - 100%)
  - **SFX / Bullet Volume Slider** (0% - 100%)
  - **Music / Ambient Noise Slider** (0% - 100%)
  - **Screen Shake Toggle** (NORMAL / HEAVY / OFF)
  - **Ship Hull Accent Customizer** (NEON CYAN / EMERALD / GOLD / CRIMSON)
  - Full **Keyboard** (Arrow Keys / WASD) AND **Mouse Interaction** (click buttons, click/drag sliders)!

---

## 🎮 Controls

| Action | Primary Key(s) | Alternate / Mouse |
| :--- | :--- | :--- |
| **Move Left / Right** | `A` / `D` | `Left Arrow` / `Right Arrow` |
| **Continuous Fire (Cadence + Momentum)** | `Spacebar` (Hold) | — |
| **Detonate EMP Nova Bomb** | `B` | — |
| **Pause / Resume** | `P` or `ESC` | `Spacebar` (while paused) |
| **Navigate Menus / Sliders** | `W` / `S` (Up/Down) & `A` / `D` (Left/Right) | `Arrow Keys` or **Mouse Click / Drag** |
| **Confirm Selection** | `Enter` or `Spacebar` | **Left Mouse Click** |
| **Quick Restart** | `R` | — |
| **Quit Game** | `ESC` (from Main Menu) | — |

---

## 🛠️ Building & Running

### Build & Run on Windows
Launch with one command:
- In **PowerShell**:
  ```powershell
  .\build.bat run
  ```
- In **Command Prompt (CMD)**:
  ```cmd
  build.bat run
  ```

To only compile `game.exe`:
```powershell
.\build.bat
```

To clean previous build artifacts:
```powershell
.\build.bat clean
```

---

## 🚀 Team Roadmap & Improvements
See [IMPROVEMENTS.md](file:///d:/2D-Space-Shooter/IMPROVEMENTS.md) for the complete 3-developer project breakdown, user experience enhancements, game feel polish, and milestone roadmap!

### Compiler Requirements
- **g++** (MinGW-w64 or MSYS2 UCRT64/MINGW64) with C++11 support.
- Linked libraries: `-lfreeglut -lopengl32 -lglu32 -lwinmm` (all natively included or provided in directory).

---

## 📁 Project Structure

```text
2D-Space-Shooter/
├── main.cpp            # Complete game engine, procedural audio synth, sectors, powerups & HUD
├── game.exe            # Compiled Windows binary
├── libfreeglut.dll     # FreeGLUT dynamic runtime library
├── build.bat           # Automated Windows build script
├── Makefile            # Standard GNU Makefile
├── CMakeLists.txt      # Modern CMake configuration
└── README.md           # Documentation and gameplay guide
```
