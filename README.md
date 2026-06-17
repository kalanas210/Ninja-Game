# 🥷 Shadow Ninja

A 2D silhouette **ninja action‑platformer** written from scratch in **C++ & OpenGL** — where **every pixel is drawn procedurally in code**. No sprites, no textures, no audio files, no game engine. The whole game compiles to a single self‑contained `.exe`.

> Built for a *Graphic Design & Development* module: a study in pairing graphic‑design thinking (composition, colour, lighting, motion) with low‑level engineering (physics, collision, animation, game feel).

---

## 📸 Screenshots

> _Add your own captures here, e.g.:_
>
> ```markdown
> <img width="1857" height="982" alt="image" src="https://github.com/user-attachments/assets/936d1ddb-349f-40c3-aac7-c555d453c21b" />

> ```
>
> _Tip: a short gameplay GIF/video at the top makes the repo far more engaging._

---

## ▶️ Play

Download / clone the repo and run **`ninja.exe`** (Windows). It's fully standalone — no install, no DLLs.

---

## 🎮 Controls

| Action | Keys |
|--------|------|
| Move | `A` / `D`  ·  `←` / `→` |
| Jump (twice = **double jump**) | `W` / `Space` / `↑` |
| Dash | `L` |
| Shuriken | `J`  ·  left mouse (aims at cursor) |
| Katana slash | `K` |
| **Ultimate** (full chakra bar) | `U` |
| Shrink (fit low tunnels) | `S` |
| Drop through one‑way platform | `Down` + `W` |
| Restart from checkpoint | `R` |
| Start / confirm / next | `Enter` |

Also: wall‑slide & **wall‑jump**, coyote time, and jump buffering.

---

## ✨ Features

- **5 themed worlds** — misty bamboo forest, fiery sunset peaks, moonlit ruins, twilight ruins, and a volcanic boss arena. Each has its own sky gradient, glowing sun/moon, fog, parallax silhouettes and ambient particles.
- **Procedurally animated ninja** — an 11‑joint skeleton with keyframed clips (run, jump, flip, dash, throw, slash, hurt, death, victory) drawn as a black silhouette with a trailing head‑wrap, sash and back‑mounted katana.
- **Combat** — straight long‑range shurikens, a visible katana slash, a chakra‑powered **Ultimate** (invulnerable dash‑slash), and a **kill‑streak combo multiplier**.
- **6 enemy types + a 3‑phase boss** (the Crimson Shogun), each with telegraphed attacks.
- **Breakable walls** that shatter into gold, plus collectibles, checkpoints and a per‑level **star rating**.
- **Hazards & platforms** — spikes, saw blades, lava, fire‑jets, moving/crumbling/one‑way platforms.
- **Game juice** — hitstop, slow‑motion, screen shake, particles, floating combo text, and victory/finale cinematics.
- Runs at a locked **60 FPS**.

### Enemy roster
| Enemy | HP | Behaviour |
|-------|----|-----------|
| Sword Stalker | 30 | Patrols, then chases & melee‑slashes |
| Kunai Archer | 20 | Keeps distance, lobs arcing kunai |
| Shadow Dasher | 18 | Telegraphed high‑speed lunge |
| Hammer Brute | 110 | Slow tank — overhead slam + shockwave |
| Spider Mine | 8 | Scuttles in and explodes |
| **Warlord Boss** | 560 | 3 escalating phases; clears the game on defeat |

---

## 🛠️ Tech

| | |
|--|--|
| Language | C++17 |
| Graphics | OpenGL (immediate mode) + freeglut |
| Rendering | 100% procedural vector art (no assets) |
| Resolution | 1280 × 720, fixed 60 FPS timestep |
| Build | MinGW‑w64 / msys2 `g++`, fully static |
| Runtime deps | System DLLs only |

**Architecture (single file):** clearly sectioned — constants → math/GL helpers → themes → background → ninja skeleton & animation → platforms/collision → world objects → player → enemies → projectiles/hazards → levels → update loop & state machine → rendering/HUD.

**Collision:** axis‑separated swept AABB with sub‑stepping; one‑way & drop‑through platforms; coyote time + jump buffering.

**State machine:** `Menu → Level Intro → Play → Level Clear → … → Game Complete` (+ `Game Over`).

---

## 🔧 Build from source

Requires **MinGW‑w64 / msys2** with **freeglut** installed.

**Windows:** double‑click `build.bat` (edit the path inside if your msys2 isn't at `C:\msys64`).

**Command line:**
```bash
g++ ninja.cpp -o ninja.exe -O2 -std=c++17 -DFREEGLUT_STATIC -DGLUT_DISABLE_ATEXIT_HACK \
  -static -static-libgcc -static-libstdc++ \
  -lfreeglut -lopengl32 -lglu32 -lwinmm -lgdi32 -luser32 -lkernel32 -lole32
```

---

## 📂 Project structure

```
.
├── ninja.cpp     # complete game source (single file)
├── ninja.exe     # prebuilt game (Windows)
├── build.bat     # Windows build script
├── build.sh      # bash build script
├── README.md
└── LICENSE
```

---

## 📜 License

Released under the **MIT License** — see [LICENSE](LICENSE).

---

*Shadow Ninja — pure code, no assets. Built with C++ & OpenGL. 🥷*
"# Ninja-Game" 
