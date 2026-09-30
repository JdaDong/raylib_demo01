# Pitfalls (raylib 6.0 · macOS 27 · Apple clang 15)

Every item below was actually hit and verified in this project. Format: **symptom → root cause → workaround → where it's pinned**.

## A. raylib 6.0 bugs

| # | API | Symptom | Root cause | Workaround | Pinned by |
|---|---|---|---|---|---|
| A1 | `FileMove` | Returns -1 and prints "Failed to copy", **but the destination has actually been written and the source was not deleted** | Internally it checks `FileCopy()==0` for success, but `FileCopy` returns **1** on success | `Storage::moveInto`: try `FileRename` first (atomic on the same file system); on failure, check whether the destination exists and delete the source ourselves | `storage.raw_filemove_bug_is_pinned` |
| A2 | `TextInsert` / `TextInsertAlloc` | `TextInsert("hello","XX",2)` → `"heXXo"` (drops `"ll"`); reads out of bounds when `pos+len(insert) > len(text)` | The tail copy offset is computed as `pos+len(insert)` instead of `pos` | `TextInsertSafe()` (std::string implementation); the showcase shows both side by side, and the raw call is guarded by a length check | `raylib_text.subtext_find_replace_insert`, `ui_logic.text_insert_safe_fixes_raylib_bug` |
| A3 | `TakeScreenshot` | On HiDPI (dpi=2), the PNG is **5120×2880** and the actual frame fills only one quadrant | Render size is already in physical pixels, and it gets multiplied by DPI again | When dpi>1, use `LoadImageFromScreen()` + `ExportImage()` | Smoke screenshot dimensions |
| A4 | `SetWindowIcon(s)` | Warning `Cocoa: Regular windows do not have icons` | GLFW does not support window icons on macOS | Expected behavior; keep the call (cross-platform), treat the warning as harmless | — |
| A5 | `ImageDither(&img, 5,6,5,8)` | Warning "Unsupported dithering bpps (24bpp)" and nothing happens | Only a **total** of ≤16 bpp is supported | Use `5,6,5,0` (RGB565, alpha dropped) | — |
| A6 | `LoadShader` compile failure | `IsShaderValid()` still returns true | On failure it falls back to the **default shader id**, and valid only means id≠0 | Additionally check `shader.id != rlGetShaderIdDefault()`; keep the previous shader during hot reload | `PostFx::reload` |
| A7 | `GenImageCellular` | Different pattern every run with the same parameters | Uses the global `GetRandomValue` internally | Call `SetRandomSeed(seed)` first | `procgen.sprites_are_deterministic` |

## B. Inconsistent return value conventions (easy to get wrong)

| Function | Success value |
|---|---|
| `FileCopy` | **1** (non-zero) |
| `FileRename` / `FileRemove` / `FileMove` / `MakeDirectory` | **0** |
| `SaveFileText` / `SaveFileData` / `ExportImage` / `ExportWave` | `true` |
| `FileTextReplace` | number of replacements (>0) |
| `MakeDirectory` | **Creates intermediate directories recursively** (`mkdir -p` semantics), 0 even if the directory already exists |

## C. Static buffer lifetime

Most `Text*` functions return **pointers into an internal static buffer** (`TextFormat` rotates through 4 buffers, the rest usually have only 1):

```cpp
const char* a = TextToUpper("x");
const char* b = TextToUpper("y");   // a now also points to "Y"!
std::string key = TextToLower(TextRemoveSpaces(TextSubtext(s, 0, eq)));  // OK: each nested call uses a different function's buffer, and the result is copied right away
char** parts = TextSplit(line, '|', &n);  // copy parts[i] immediately; the next TextSplit overwrites them
```

The `*Alloc` variants (`TextReplaceAlloc` / `TextInsertAlloc` / `TextReplaceBetweenAlloc`) must be freed with `MemFree`.
`ComputeMD5/SHA1/SHA256` also return static arrays.

## D. Lifetimes / ownership

- `LoadMusicStreamFromMemory`: **the byte buffer must outlive the Music** (it decodes while streaming) → kept in the `AudioSystem::musicBytes_` member.
- `LoadSoundAlias`: an alias shares the source's buffer, so **unload the alias before the source**.
- `UnloadFont(GetFontDefault())` is ignored, but font structs you assemble yourself (LoadFontData + GenImageFontAtlas) need `UnloadFont` to free glyphs/recs.
- `UnloadMaterial` on a `LoadMaterialDefault()` material only frees the maps and does not touch the default shader or texture.
- `DrawMesh` with a hand-built `Mesh`: CPU arrays must be allocated with `MemAlloc` (`UnloadMesh` calls `MemFree`); a dynamic mesh needs `UploadMesh(&m, true)` before `UpdateMeshBuffer` can be used.

## E. Rendering

- A RenderTexture is stored **upside down** in OpenGL, so draw it with `source.height = -h`.
- **`BeginTextureMode` cannot be nested** → offscreen passes such as the minimap and showcase RT go into a separate `prerender()` phase.
- Winding order: `DrawTriangle`/`DrawTriangleFan` require **counter-clockwise order on screen** (y points down), otherwise back-face culling drops them. The CPU-side `ImageDrawTriangle*` doesn't care about winding.
- `BLEND_CUSTOM(_SEPARATE)` needs `rlSetBlendFactors(Separate)` **before** `BeginBlendMode`.
- `SetShapesTexture` changes the texture of every subsequent shape; save it with `GetShapesTexture/GetShapesTextureRectangle` and restore afterwards.
- Virtual resolution + letterbox: without `SetMouseOffset/SetMouseScale`, `GetMousePosition` stays in window coordinates, and every piece of UI hit testing is off.

## F. Platform / process (macOS)

| # | Symptom | Root cause | Workaround |
|---|---|---|---|
| F1 | **SIGSEGV on exit**, stack shows infinite recursion `Callback → glfwGetTime → _glfwInputError → TraceLog → Callback` | Calling `GetTime()` inside the `SetTraceLogCallback` callback: after CloseWindow, glfwGetTime raises a GLFW error → another log line | **Never call raylib APIs inside the log callback**; use `std::chrono` for timestamps |
| F2 | Smoke test **hangs** occasionally (~1 in 5), `sample` shows the main thread stuck in `_glfwWaitEventsCocoa` | macOS sends a spurious minimize/occlude event → the code runs `EnableEventWaiting()` → `PollInputEvents` becomes a blocking `glfwWaitEvents` and nobody ever sends another event | Don't enable event waiting in unattended (autopilot) mode; also disable VSync + `FLAG_WINDOW_ALWAYS_RUN` in smoke mode |
| F3 | Frame-count based smoke screenshots never fire on a 120 Hz Retina display | Frame rate differs by device | Trigger screenshots by **accumulated time** |
| F4 | **The ASan runtime aborts before main** (even for a 1-line program) | Apple clang 15 / Homebrew LLVM 21 ASan is incompatible with macOS 27 | `test.sh --asan` runs a probe first and falls back to UBSan automatically |
| F5 | CI script `extra[@]: unbound variable` | macOS ships bash 3.2, where expanding an empty array under `set -u` is an error | `"$@"` pass-through / `${arr[@]+"${arr[@]}"}` |
| F6 | `cmd \| grep … \|\| true` then `PIPESTATUS` always 0 / script exits unexpectedly | Under `set -eo pipefail`, `\|\| true` masks the real failure, and a failing trailing `ls` exits the script | Redirect build output to a log file and check the command's own exit code |

## G. Tooling

- **Don't edit the same file in parallel**: several concurrent edits to one file overwrite each other (hit twice while building this project) → edits to a single file must be sequential.
- Env vars `LDFLAGS=-L/opt/homebrew/Cellar/llvm/19.1.7/lib` point to a path that no longer exists → the linker's `search path not found` warnings are harmless; filter them out when looking at compiler warnings (`grep "warning: [^s]"`).
