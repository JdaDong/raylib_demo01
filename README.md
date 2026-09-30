# raylib_demo01 — NEON HARVEST 霓虹收割者

A 2D arena shooter built with **raylib 6.0 + C++17**. It doubles as an interactive tour of the raylib API:
**588 of the 600 `RLAPI` functions are called for real (98%). Within the 2D-game scope, coverage is 541/543 = 99.6%**
(see [docs/API_COVERAGE.md](docs/API_COVERAGE.md)).

- **No asset files**: every sprite, font atlas, sound effect, music track and shader is generated at runtime (Image API + PCM synthesis).
- **Testable core logic**: all window-, GPU- and audio-device-independent logic lives in the `rd_core` static library and is covered by
  **149 unit tests / 19k+ assertions** (a self-written zero-dependency framework). The whole suite runs in about 0.2 s.
- **Deterministic simulation**: fixed step at 120 Hz plus a seeded xorshift RNG. The same seed and the same input always give the same `stateHash()`. On top of that, raylib Automation Events provide input recording and replay.
- **GUI smoke test**: `--smoke` lets an autopilot walk through every scene and all 8 showcase pages, saving 15 screenshots along the way. Any `LOG_ERROR` fails the run.

## Quick start

```bash
git clone --recursive <repo> raylib_demo01     # or, after a plain clone: git submodule update --init --depth 1
./scripts/doctor.sh              # read-only environment check
./scripts/run.sh                 # build (incremental) + run; saves go to ./saves
./scripts/test.sh                # all unit tests (CTest, one entry per suite)
./scripts/ci.sh                  # full pipeline: -Werror build / unit tests / ASan+UBSan / coverage gate / GUI smoke
```

### raylib dependency

raylib is a **git submodule at `third_party/raylib`, pinned to tag `6.0` (`dbc56a87`)**, and is built from source with the project.
CMake resolves it in this order (`-DRD_RAYLIB_SOURCE=AUTO|SUBMODULE|SYSTEM|FETCH`, default AUTO):

| Priority | Source | When |
|---|---|---|
| 1 | `third_party/raylib` submodule | Submodule initialized (default; version pinned by the gitlink) |
| 2 | `find_package(raylib)` | Submodule missing, raylib installed (`brew install raylib`) |
| 3 | FetchContent tag 6.0 | Neither of the above (needs network) |

Upgrading raylib: `cd third_party/raylib && git fetch --depth 1 origin tag <X> && git checkout <X>`, then commit the new gitlink.
Afterwards, check the pinning tests in `docs/PITFALLS.md`: once upstream fixes a bug, the corresponding test turns red and its workaround should be removed.

Common launch options (`raylib_demo01 --help`):

| Option | Meaning |
|---|---|
| `--scene title\|game\|showcase\|settings` | Start directly in a given scene |
| `--page N` | Open showcase page N (1–8) |
| `--seed N` | Fixed world seed (reproducible runs) |
| `--size 1600x900` / `--hidden` | Window size / hidden window |
| `--smoke N` | Autopilot smoke test, at most N frames, exit code = result |
| `--save-dir DIR` / `--log debug` | Save directory / log level |

## Controls

| Context | Key | Action |
|---|---|---|
| Gameplay | WASD / arrow keys / left stick | Move (with inertia and friction) |
| | Mouse / right stick | Aim |
| | LMB / J / RT | Fire |
| | Space / Shift / RMB / A button | Dash (i-frames; ramming enemies damages them) |
| | Mouse wheel | Camera zoom |
| | Esc / P / Start | Pause (resume / restart with the same seed / back to title) |
| | Ctrl+C | Copy seed and score to the clipboard |
| Title | Ctrl+V | Read a seed from the clipboard |
| Global | F1 | Debug console (window, monitor, DPI, input, audio, log ring buffer) |
| | F2 / F3 | Toggle post-processing / force shader reload |
| | F4 | Relative aim (`DisableCursor` locks the cursor) |
| | F5 / F6 | Record input / replay the last recording |
| | F7 / F8 | Move to next monitor / virtual touch joystick |
| | F9 / F10 / F11 / F12 | Always on top / borderless window / fullscreen / screenshot |
| | Ctrl+O / M / N / R | Window opacity / maximize / minimize / reset size and center |
| | Ctrl+E | Export highscores as a C array (`ExportDataAsCode`) |
| Showcase | Q / E, click tabs | Switch page; each page has its own on-screen hints |
| Drag & drop | `.png/.jpg/.qoi/.bmp/.tga` | Use the image as the ship skin |
| | `.gif` | First frame becomes the skin (`LoadImageAnimFromMemory`) |
| | `.ini` / `.rae` | Import settings / import a replay |

## Gameplay

- 4 enemy types: **Chaser** (chases you), **Weaver** (moves along a Catmull-Rom spline), **Splitter** (splits in two when killed, 2 levels, 7 kills total), **Tank** (fires only with line of sight).
- Wave budget: `budget = 4 + 3w + w²/4`, enemy types unlock with wave number, spawn points avoid walls and the player.
- Combo multiplier `1 + 0.1 × combo` (capped at 40); the combo resets when you get hit or after 2.5 s without a kill. Kills drop gems (score) and health packs, which get pulled towards you inside the magnet radius.
- Highscores are stored as `NH1:<CRC32>:<base64(deflate(payload))>`; tampered files are rejected.

## Showcase pages

| # | Page | Main APIs |
|---|---|---|
| 1 | Shapes | 28 grid cells covering every basic 2D `Draw*` function (including Pixel/Line/Circle/Ellipse/Ring/Rect/Triangle/Poly variants) |
| 2 | Splines | 5 spline types + `GetSplinePoint*` + segment variants, control points draggable |
| 3 | Collision | All 10 2D `CheckCollision*` functions + `GetCollisionRec`, driven by the mouse probe |
| 4 | Image Lab | 28 CPU image operations; palette / average color / alpha border; paint canvas (`UpdateTextureRec`); PNG export |
| 5 | Textures | 9-patch / 3-patch, all 8 blend modes (including custom factors), render texture, `SetShapesTexture`, rlgl matrix stack |
| 6 | Text | TTF with CJK, codepoint APIs, glyph inspector, all 30+ `Text*` utility functions, **raylib TextInsert bug side by side with the fix** |
| 7 | Audio | 9 synthesized sound effects with waveforms, music seeking / pitch / pan / low-pass processor, callback stream and push stream, VU meter |
| 8 | Input/System | Keyboard / mouse / gamepad / touch / gestures, random sequences, CRC32/MD5/SHA1/SHA256, file list, clipboard image |

The title screen has a 3D backdrop with procedural meshes (heightmap / cubicmap / torus / knot…), a dynamic mesh updated via `UpdateMeshBuffer`, and mouse ray picking with 5 `GetRayCollision*` functions.

## Directory layout

```
raylib_demo01/
├── CMakeLists.txt        # rd_core (static lib) + raylib_demo01 (game) + unit_tests; find_package → FetchContent fallback
├── src/
│   ├── core/             # rng / easing / timer / math2d / spatial_grid / camera_rig / dyn_array (MemAlloc)
│   ├── game/             # world simulation, wave_director, particles, config (INI), highscore, storage (file IO), hashing
│   ├── gfx/              # procgen / image_lab (CPU, testable) · assets / postfx / renderer / backdrop3d (GPU)
│   ├── audio/            # wavgen (PCM synthesis, testable) · audio_system (device, streams, processors)
│   ├── ui/               # ui_logic (layout/slider/UTF-8 input, testable) · widgets (immediate-mode drawing)
│   ├── platform/         # input_mapper: keyboard/mouse/gamepad/touch → InputFrame
│   ├── app/              # app main loop, context, logger (TraceLog callback)
│   └── scenes/           # title / gameplay / gameover / settings / showcase
├── tests/                # testing.hpp (framework) + 16 test suites
├── scripts/              # doctor / build / run / test / smoke / coverage / ci / format / clean
└── docs/                 # ARCHITECTURE / TESTING / PITFALLS / API_COVERAGE
```

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — layering, main loop, fixed step, event flow, rendering passes
- [docs/TESTING.md](docs/TESTING.md) — test framework, suite list, smoke test, sanitizers, CI
- [docs/PITFALLS.md](docs/PITFALLS.md) — **raylib 6.0 bugs and pitfalls found in this project (with repro steps and workarounds)**
- [docs/API_COVERAGE.md](docs/API_COVERAGE.md) — per-function coverage report (generated by the script)
