# Architecture

## 1. Layering: split along "does it need a GPU / window / audio device?"

```mermaid
flowchart TB
  subgraph EXE["raylib_demo01 (executable, needs window/GL/audio device)"]
    main[main.cpp] --> app[app/App: main loop · hotkeys · drag&drop · replay · smoke]
    app --> scenes[scenes/*: Title · Gameplay · GameOver · Settings · Showcase]
    scenes --> gpu[gfx: assets · postfx · renderer · backdrop3d]
    scenes --> au[audio/audio_system]
    scenes --> inp[platform/input_mapper]
    scenes --> wid[ui/widgets]
  end
  subgraph CORE["rd_core (static library, testable without window/GPU/audio device)"]
    world[game/world + wave_director + particles]
    data[game/config · highscore · storage · hashing]
    cpu[gfx/procgen · image_lab]
    syn[audio/wavgen]
    uil[ui/ui_logic]
    base[core/rng · timer · easing · math2d · spatial_grid · camera_rig · dyn_array]
  end
  gpu --> cpu
  au --> syn
  wid --> uil
  scenes --> world
  app --> data
  world --> base
  TESTS[unit_tests] --> CORE
```

Dependency: raylib comes from the `third_party/raylib` submodule (pinned to 6.0) via `add_subdirectory(... EXCLUDE_FROM_ALL)`, and the `raylib` target
is linked PUBLIC into `rd_core`; see the README "raylib dependency" section for the fallback order.

Rule: **everything in `rd_core` must run without `InitWindow`**. raylib's CPU APIs (Image*, Text*, Wave*, Color*, CheckCollision*,
File*, Compress/Base64/Hash, GetWorldToScreen2D, …) are themselves testable, so the unit tests exercise real raylib code rather than mocks.

## 2. Main loop (`App::frame`)

```mermaid
sequenceDiagram
  participant L as App::frame
  participant R as Replay
  participant S as Scene
  participant P as PostFx
  L->>R: stepReplay() — PlayAutomationEvent for this frame (before any input is read)
  L->>L: window events / hotkeys / dropped files / SetMouseOffset+Scale (virtual resolution)
  L->>L: audio.update · shader hot reload poll · noise texture refresh
  L->>L: fade transition state machine (ctx.go(id) → fade-out → switchScene → fade-in)
  L->>S: update(dt)
  L->>S: prerender() — minimap / showcase RT (BeginTextureMode cannot nest)
  L->>P: beginScene() = BeginTextureMode(960x540)
  L->>S: draw()
  L->>P: endScene()
  L->>L: BeginDrawing → present(letterbox + post shader) → debug overlay → EndDrawing
```

- **Virtual resolution 960×540**: scenes always draw into a fixed-size RenderTexture, which is then scaled to the window with letterboxing.
  `SetMouseOffset/SetMouseScale` map mouse coordinates back to virtual coordinates, so scenes never need to think about window size or DPI.
- **dt clamp**: `dt ≤ 0.1s` (prevents big jumps after debugger pauses or window drags); in record/replay mode dt is fixed at 1/60.

## 3. Simulation: fixed step + event queue

```mermaid
flowchart LR
  IF[InputFrame<br/>move/aim/fire/dash] --> FS{FixedStep 1/120<br/>accumulator, max 8 steps}
  FS -->|n steps| W[World::step]
  W --> EV[(GameEvent queue)]
  EV -->|drainEvents| H[GameplayScene::handleEvents]
  H --> A[audio: pitch jitter + pan by screen x]
  H --> PS[particles.burst]
  H --> SH[Shake trauma²]
  H --> RB[gamepad vibration]
  H --> PF[postfx damage flash]
```

- `World` knows nothing about rendering or audio; it only produces events → presentation-layer effects are fully decoupled, and tests only assert on events and state.
- Determinism: all randomness goes through the World's own `Rng` (xorshift64*). **Visual randomness (particles, shake) uses a separate `fx_` Rng** so it never pollutes the simulation state.
- `stateHash()` = FNV-1a over quantized positions, HP, score, RNG state… used for the "same seed + same input → same state" regression test.
- Bullet collision uses a **swept test** (`CheckCollisionCircleLine` over prevPos→pos) against a grid broadphase (`SpatialGrid`), so fast bullets don't tunnel.
- Splitter children are pushed into `pendingEnemies_` and merged at the end of the step, so `enemies_` is never modified while being iterated.

## 4. Rendering passes

| Pass | Target | Content |
|---|---|---|
| prerender | minimap RT 180×100 | walls / enemies / player triangle / viewport frame |
| scene | virtual RT 960×540 | parallax background (WRAP_REPEAT) → `BeginMode2D`: floor, walls, pickups, enemies, trail spline, player → `BLEND_ADDITIVE`: bullet glow, particles, explosion sprite sheet → HUD |
| present | backbuffer | `BeginShaderMode(postfx)`: chromatic aberration, scanlines, grain (`noiseTex`), vignette, saturation matrix, vertical tint, damage flash |
| overlay | backbuffer (physical pixels) | FPS, F1 debug console (LoadFont mono) |

The post shader lives in `saves/shaders/postfx.fs`. Changes to it are detected via `GetFileModTime` and hot-reloaded. If compilation fails, the previous shader is kept; if it fails on the very first load, the embedded source is used.

## 5. Resource generation (all procedural)

| Resource | Generation path |
|---|---|
| Ship / enemies / gem | `GenImageColor` + `ImageDrawTriangleEx/Fan/Strip`… → `LoadTextureFromImage`; collision radius taken from `GetImageAlphaBorder` |
| Background | Perlin + Cellular (`SetRandomSeed` makes it reproducible) → tint / contrast / dither → GPU mipmaps |
| Fonts | TTF: `LoadFontEx` (codepoints collected from UI strings) / `LoadFontFromMemory` / `LoadFontData`+`GenImageFontAtlas` / `LoadFont`; fallback: bitmap font built with `LoadFontFromImage` |
| Sound effects | `wavgen` synthesizes PCM → RIFF bytes → `LoadWaveFromMemory` → `LoadSoundFromWave`; shots use 4 `LoadSoundAlias` instances round-robin |
| Music | 8-bar chiptune → `LoadMusicStreamFromMemory` (the bytes must outlive the Music) |
| Streams | engine hum = `SetAudioStreamCallback` (pull); ambient pad = `UpdateAudioStream` (push) |

## 6. Persistence

`Storage` wraps raylib's file APIs:

- `writeTextAtomic`: write `.tmp` → back up old file to `.bak` → `rename(2)` for an atomic replace; `readText` falls back to `.bak` if the main file is corrupt.
- Settings are in INI format (`TextSplit/TextFindIndex/TextToLower` parsing). Unknown keys produce a warning but are not fatal. The legacy `volume` key is migrated automatically.
- Screenshots are pruned to the newest 30 (`pruneOldest` by `GetFileModTime`).
