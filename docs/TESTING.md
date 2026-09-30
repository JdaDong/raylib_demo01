# Testing

## Test pyramid

| Layer | What | How to run | Time |
|---|---|---|---|
| Unit tests | 16 suites / 149 cases / 19k+ assertions, **no window, GPU or audio device needed** | `./scripts/test.sh` | ~0.2 s |
| Sanitizers | Same tests under UBSan (+ASan when available), UB aborts immediately | `./scripts/test.sh --asan` | ~4 s |
| API coverage gate | Coverage of raylib functions in the 2D scope must be ≥ 95% | `./scripts/coverage.sh 95` | <1 s |
| GUI smoke | Autopilot tours every scene and all 8 showcase pages, 15 screenshots, fails on any LOG_ERROR | `./scripts/smoke.sh` | ~18 s |
| Full pipeline | doctor → -Werror → unit → sanitizers → full build → coverage → smoke | `./scripts/ci.sh [--no-gui]` | ~1 min |

## Framework `tests/testing.hpp`

Written in ~100 lines of C++17, no gtest dependency:

```cpp
TEST(world, bullet_kills_enemy_and_scores) {
    World w(Quiet(), 1);
    ...
    CHECK_EQ(kills, 1);          // on failure, prints both values and continues
    CHECK_NEAR(x, 960.0, 1e-3);  // floating-point comparison
    REQUIRE(ptr != nullptr);     // on failure, aborts the current test case
    CHECK_STREQ(s, "abc");       // std::string / const char* comparison
}
```

```bash
./build/unit_tests                   # all
./build/unit_tests --suite world     # one suite (used by CTest: ut.<suite>)
./build/unit_tests --filter stress   # substring match on the test name
./build/unit_tests --list            # list all tests
./scripts/test.sh --suite storage -v
```

## Suites

| Suite | Cases | Focus |
|---|---|---|
| core | 17 | RNG determinism and ranges, easing endpoints, Cooldown, FixedStep spiral-of-death guard, DynArray (MemAlloc/MemRealloc), deadzone, circle-rect push-out, letterbox, frame-rate-independent ExpDecay, saturation matrix, Camera2D world↔screen round-trip (incl. `GetCameraMatrix2D`), screen shake |
| spatial_grid | 4 | Deduplication, out-of-bounds clamping, **300 random rects cross-checked against brute force (never misses a hit)** |
| world | 24 | Movement / clamping / walls, fire rate, swept bullets without tunneling, 3-level Splitter split (7 kills total), i-frames, death freezes state, dash, magnet pickup, combo decay, Tank line of sight, Weaver spline, spawn points, **same seed ⇒ same stateHash**, reset, **150-seed × 8 s fuzz** |
| wave_director | 5 | Budget formula, unlock order, composition always within budget |
| particles | 5 | Lifetime, capacity limit, color/size interpolation |
| config | 7 | INI round-trip, tolerant parsing (CRLF / case / clamping / bad values), legacy key migration |
| highscore | 7 | Sorting, capacity limit, ties, round-trip with Chinese names, **tamper detection (magic / CRC / base64 / header)**, name sanitization |
| storage | 15 | Atomic writes + .bak fallback, binary round-trip, file info, prune, migrate, ExportDataAsCode, **in-memory virtual FS via custom IO callbacks**, pinned FileMove bug |
| hashing | 7 | CRC32/MD5/SHA1/SHA256 against **standard test vectors**, deflate / base64 round-trip and invalid input |
| procgen | 12 | Every generated sprite is valid, non-empty and deterministic, alpha border radius, palette, average color |
| image_lab | 7 | All 28 Image operations keep the image valid; size / pixel / format / alpha effects of each |
| wavgen | 11 | Synthesis length and range, envelope, RIFF header byte layout, `LoadWaveFromMemory`, WaveCopy/Crop/Format/LoadWaveSamples, export + reload, low-pass filter frequency response |
| ui_logic | 8 | VStack/HGrid layout, slider mapping, focus navigation wrap-around, **UTF-8 text input (backspace removes a whole 4-byte emoji)**, word wrap, number formatting, TextInsertSafe |
| raylib_text | 10 | Behavior of raylib Text* / codepoint APIs (**the TextInsert bug is pinned by a test**) |
| raylib_shapes | 5 | All 2D CheckCollision* functions, spline point evaluation |
| raylib_color | 5 | Color int/hex/HSV/normalize, tint/brightness/contrast/alpha blending, pixel format helpers |

### Pinning tests

For known raylib 6.0 bugs, the tests **assert the buggy behavior as it is today** (for example FileMove returning -1 while the source file remains).
Once a raylib upgrade fixes the bug, these tests turn red and remind you to remove the corresponding workaround. See [PITFALLS.md](PITFALLS.md).

## GUI smoke test

```bash
./build/raylib_demo01 --smoke 6000 --save-dir /tmp/smoke
```

- Autopilot: Title → Play (orbits the arena, auto-aims at the nearest enemy, dies after 6 s) → GameOver → Title → Showcase (flips through 8 pages) → Title → Settings → Title → Quit.
- Screenshots are **triggered by time rather than frame count** (independent of frame rate). With HiDPI, `LoadImageFromScreen`+`ExportImage` is used instead of TakeScreenshot.
- Exit code: 0 on success; 1 if `LOG_ERROR > 0` or fewer than 4 screenshots; `smoke.sh` enforces a 120 s timeout (124 means hung).
- VSync is disabled in smoke mode and `FLAG_WINDOW_ALWAYS_RUN` is set, so an occluded window cannot block SwapBuffers.
- Stability check: 6 consecutive runs all passed, 3 of which hit spurious minimize events.

## Sanitizers and toolchain notes

`RD_SANITIZE=ON` enables sanitizers only for `unit_tests`, with `-fno-sanitize-recover=undefined`.
`test.sh --asan` **first compiles and runs a tiny probe program**. If the ASan runtime is unusable (on macOS 27, both Apple clang 15 and
Homebrew LLVM 21 fail before `main`: `asan_init_is_running` / hang), it automatically falls back to UBSan only and prints a warning,
so CI does not go red for environment reasons. On Linux CI or older macOS, full ASan+UBSan is used.
