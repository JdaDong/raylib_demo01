#include "app/app.hpp"

#include "app/logger.hpp"
#include "core/math2d.hpp"
#include "rlgl.h"
#include "ui/ui_logic.hpp"

#include <cstring>
#include <string>

namespace rd {

// ---------------------------------------------------------------------------
// command line
// ---------------------------------------------------------------------------
const char* UsageText() {
    return "raylib_demo01 [options]\n"
           "  --smoke N          autopilot through every scene, exit after N frames (CI)\n"
           "  --save-dir DIR     where settings/scores/screenshots go (default ./saves)\n"
           "  --seed N           fixed world seed\n"
           "  --scene NAME       title|game|showcase|settings\n"
           "  --page N           first showcase page (1-8)\n"
           "  --size WxH         window size (default 1280x720)\n"
           "  --hidden           start with a hidden window\n"
           "  --log LEVEL        trace|debug|info|warn|error\n"
           "  --version | --help\n";
}

bool ParseArgs(int argc, char** argv, AppOptions& o, std::string& err) {
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        if (TextIsEqual(a, "--smoke")) {
            const char* v = next();
            o.smokeFrames = v ? TextToInteger(v) : 600;
        } else if (TextIsEqual(a, "--save-dir")) {
            const char* v = next();
            if (!v) { err = "--save-dir needs a value"; return false; }
            o.saveDir = v;
        } else if (TextIsEqual(a, "--seed")) {
            const char* v = next();
            o.seed = v ? static_cast<unsigned int>(TextToInteger(v)) : 0;
        } else if (TextIsEqual(a, "--scene")) {
            const char* v = next();
            const std::string s = v ? TextToLower(v) : "";
            if (s == "title") o.startScene = static_cast<int>(SceneId::Title);
            else if (s == "game") o.startScene = static_cast<int>(SceneId::Gameplay);
            else if (s == "showcase") o.startScene = static_cast<int>(SceneId::Showcase);
            else if (s == "settings") o.startScene = static_cast<int>(SceneId::Settings);
            else { err = "unknown scene: " + s; return false; }
        } else if (TextIsEqual(a, "--page")) {
            const char* v = next();
            o.showcasePage = v ? Clamp(static_cast<float>(TextToInteger(v) - 1), 0.f, 7.f) : 0;
        } else if (TextIsEqual(a, "--size")) {
            const char* v = next();
            int n = 0;
            char** parts = v ? TextSplit(v, 'x', &n) : nullptr;
            if (n != 2) { err = "--size expects WxH"; return false; }
            o.width = TextToInteger(parts[0]);
            o.height = TextToInteger(parts[1]);
            if (o.width < 320 || o.height < 200) { err = "window too small"; return false; }
        } else if (TextIsEqual(a, "--hidden")) {
            o.hidden = true;
        } else if (TextIsEqual(a, "--log")) {
            const char* v = next();
            const std::string s = v ? TextToLower(v) : "info";
            o.logLevel = s == "trace" ? LOG_TRACE : s == "debug" ? LOG_DEBUG : s == "warn" ? LOG_WARNING : s == "error" ? LOG_ERROR : LOG_INFO;
        } else {
            err = std::string("unknown option: ") + a;
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
App::App(AppOptions opts) : opts_(std::move(opts)), storage_(opts_.saveDir) {}

int App::run() {
    if (!startup()) {
        shutdown();
        return 3;
    }
    while (!WindowShouldClose() && !ctx_.quitRequested) {
        frame();
        if (opts_.smokeFrames > 0 && static_cast<int>(ctx_.frame) >= opts_.smokeFrames) {
            TraceLog(LOG_WARNING, "SMOKE: frame budget exhausted before the scripted tour finished");
            break;
        }
    }
    if (opts_.smokeFrames > 0) {
        const int errors = LogCount(LOG_ERROR);
        TraceLog(LOG_INFO, "SMOKE: frames=%u screenshots=%d errors=%d title-visits=%d", ctx_.frame, smokeShots_, errors, ctx_.smokeTitleVisits);
        if (errors > 0 || smokeShots_ < 4) exitCode_ = 1;
    }
    shutdown();
    return exitCode_;
}

bool App::startup() {
    InstallLogger(opts_.logLevel);
    TraceLog(LOG_INFO, "raylib_demo01 %s on raylib %s", RD_VERSION, RAYLIB_VERSION);

    // settings first: they decide the window flags
    if (!storage_.init()) return false;
    std::string text;
    bool fromBackup = false;
    if (storage_.migrateText("settings.ini", "volume =", "master_volume =")) TraceLog(LOG_INFO, "CONFIG: migrated legacy key in settings.ini");
    if (storage_.readText("settings.ini", text, &fromBackup)) {
        std::vector<std::string> warnings;
        ParseSettings(text.c_str(), settings_, &warnings);
        for (const auto& w : warnings) TraceLog(LOG_WARNING, "CONFIG: %s", w.c_str());
        if (fromBackup) TraceLog(LOG_WARNING, "CONFIG: settings.ini unreadable, restored from backup");
    }
    if (opts_.seed) settings_.seed = opts_.seed;
    if (storage_.readText("scores.sav", text)) {
        std::string err;
        if (!HighscoreTable::Deserialize(text, scores_, &err)) TraceLog(LOG_WARNING, "SCORES: rejected save (%s)", err.c_str());
    }

    unsigned int flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI;
    // macOS: with VSync an occluded window can block in SwapBuffers indefinitely -> the
    // unattended smoke test runs without VSync and keeps rendering when unfocused
    if (settings_.vsync && opts_.smokeFrames == 0) flags |= FLAG_VSYNC_HINT;
    if (opts_.smokeFrames > 0) flags |= FLAG_WINDOW_ALWAYS_RUN;
    if (opts_.hidden) flags |= FLAG_WINDOW_HIDDEN;
    SetConfigFlags(flags);
    InitWindow(opts_.width, opts_.height, "raylib_demo01 - NEON HARVEST");
    if (!IsWindowReady()) return false;
    SetWindowMinSize(640, 360);
    SetWindowMaxSize(7680, 4320);
    SetExitKey(KEY_NULL);  // Esc is used for pause/back; close via window button or menu
    SetTargetFPS(settings_.targetFps);
    SetRandomSeed(static_cast<unsigned int>(GetTime() * 1e6) ^ 0x5EEDu);
    if (settings_.fullscreen && !IsWindowFullscreen()) ToggleFullscreen();

    const bool assetsOk = assets_.load(storage_);
    audio_.init(storage_, settings_);
    postfx_.init(storage_, ctx_.vw, ctx_.vh);
    input_.init();

    ctx_.storage = &storage_;
    ctx_.settings = &settings_;
    ctx_.scores = &scores_;
    ctx_.assets = &assets_;
    ctx_.audio = &audio_;
    ctx_.postfx = &postfx_;
    ctx_.input = &input_;
    ctx_.theme.font = assets_.ui;
    ctx_.theme.panel = assets_.panel;
    ctx_.theme.patch = assets_.panelPatch;
    ctx_.autopilot = opts_.smokeFrames > 0;
    ctx_.showcasePage = opts_.showcasePage;

    sceneId_ = opts_.startScene >= 0 ? static_cast<SceneId>(opts_.startScene) : SceneId::Title;
    scene_ = make(sceneId_);
    scene_->enter(ctx_);
    fade_ = 1.f;
    fadeDir_ = -1;
    return assetsOk;
}

void App::shutdown() {
    if (scene_) {
        scene_->exit(ctx_);
        scene_.reset();
    }
    if (ctx_.recording) StopAutomationEventRecording();
    if (aeLoaded_) UnloadAutomationEventList(aeList_);
    audio_.fadeOutAndClose(0.25f);
    postfx_.shutdown();
    assets_.unload();
    if (IsWindowReady()) CloseWindow();
}

std::unique_ptr<Scene> App::make(SceneId id) {
    switch (id) {
        case SceneId::Gameplay: return MakeGameplayScene();
        case SceneId::GameOver: return MakeGameOverScene();
        case SceneId::Settings: return MakeSettingsScene();
        case SceneId::Showcase: return MakeShowcaseScene();
        default: return MakeTitleScene();
    }
}

void App::switchScene(SceneId id) {
    scene_->exit(ctx_);
    sceneId_ = id;
    scene_ = make(id);
    scene_->enter(ctx_);
    TraceLog(LOG_INFO, "SCENE: -> %s", scene_->name());
}

// ---------------------------------------------------------------------------
void App::frame() {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.1f;  // debugger pauses / window drags
    if (ctx_.deterministic) dt = 1.f / 60.f;
    ctx_.time += dt;
    ++ctx_.frame;

    stepReplay();  // inject recorded input before anything reads it
    handleWindowEvents();
    handleHotkeys();
    handleDroppedFiles();
    updateMouseMapping();
    UiBeginFrame();

    audio_.update(dt);
    postfx_.pollHotReload(dt);
    assets_.animateNoise(ctx_.frame);
    if (ctx_.toastTime > 0.f) ctx_.toastTime -= dt;

    // scene switching with a fade
    if (ctx_.hasPending && fadeDir_ <= 0) {
        fadeDir_ = 1;
        fadeTarget_ = ctx_.pending;
        ctx_.hasPending = false;
    }
    fade_ += fadeDir_ * dt * 5.f;
    if (fadeDir_ > 0 && fade_ >= 1.f) {
        fade_ = 1.f;
        switchScene(fadeTarget_);
        fadeDir_ = -1;
    } else if (fadeDir_ < 0 && fade_ <= 0.f) {
        fade_ = 0.f;
        fadeDir_ = 0;
    }
    if (fadeDir_ <= 0) scene_->update(ctx_, dt);

    // offscreen passes, then the scene into the virtual-resolution target
    scene_->prerender(ctx_);
    postfx_.beginScene();
    scene_->draw(ctx_);
    if (ctx_.toastTime > 0.f) {
        const float a = Clamp(ctx_.toastTime * 2.f, 0.f, 1.f);
        const Vector2 m = UiMeasure(ctx_.theme, ctx_.toast.c_str(), 18);
        const Rectangle r{ctx_.vw * 0.5f - m.x * 0.5f - 14, ctx_.vh - 64.f, m.x + 28, 32};
        UiPanel(ctx_.theme, r, Fade(WHITE, a));
        UiText(ctx_.theme, ctx_.toast.c_str(), {r.x + 14, r.y + 7}, 18, Fade(RAYWHITE, a));
    }
    if (ctx_.recording || ctx_.replaying) {
        const bool blink = static_cast<int>(ctx_.time * 2) % 2 == 0;
        DrawCircle(ctx_.vw - 20, 20, 7, Fade(ctx_.recording ? RED : GREEN, blink ? 1.f : 0.3f));
        DrawText(ctx_.recording ? "REC" : "REPLAY", ctx_.vw - 70, 14, 12, RAYWHITE);
    }
    if (fade_ > 0.f) DrawRectangle(0, 0, ctx_.vw, ctx_.vh, Fade(BLACK, fade_));
    postfx_.endScene();

    const Rectangle dest = Letterbox(static_cast<float>(ctx_.vw), static_cast<float>(ctx_.vh), static_cast<float>(GetScreenWidth()),
                                     static_cast<float>(GetScreenHeight()));
    BeginDrawing();
    ClearBackground(BLACK);
    postfx_.present(dest, ctx_.time, settings_.postfx, assets_.noise);
    drawOverlay(dest);
    EndDrawing();

    const int cursor = UiRequestedCursor();
    if (cursor != lastCursor_) {
        SetMouseCursor(cursor);
        lastCursor_ = cursor;
    }
    if (ctx_.autopilot) smokeStep();
}

void App::updateMouseMapping() {
    // GetMousePosition() then returns virtual-resolution coordinates everywhere
    const Rectangle dest = Letterbox(static_cast<float>(ctx_.vw), static_cast<float>(ctx_.vh), static_cast<float>(GetScreenWidth()),
                                     static_cast<float>(GetScreenHeight()));
    SetMouseOffset(static_cast<int>(-dest.x), static_cast<int>(-dest.y));
    SetMouseScale(ctx_.vw / dest.width, ctx_.vh / dest.height);
}

void App::handleWindowEvents() {
    if (IsWindowResized()) TraceLog(LOG_INFO, "WINDOW: resized to %dx%d (render %dx%d)", GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight());
    const bool minimized = IsWindowMinimized();
    if (minimized != minimized_) {
        minimized_ = minimized;
        TraceLog(LOG_INFO, "WINDOW: %s", minimized ? "minimized -> event waiting" : "restored -> polling");
        audio_.pauseAll(minimized);
        // sleep until the next OS event instead of spinning while iconified.
        // Never in unattended runs: glfwWaitEvents() would block forever (seen on macOS when
        // the window gets iconified/occluded without a user around to restore it).
        if (!ctx_.autopilot) {
            if (minimized) EnableEventWaiting();
            else DisableEventWaiting();
        }
    }
    static bool focused = true;
    const bool f = IsWindowFocused();
    if (f != focused && !ctx_.autopilot) {
        focused = f;
        SetTargetFPS(f ? settings_.targetFps : 30);  // power save in background
    }
}

void App::handleHotkeys() {
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER);
    if (IsKeyPressed(KEY_F1)) debug_ = !debug_;
    if (IsKeyPressed(KEY_F2)) {
        settings_.postfx = !settings_.postfx;
        ctx_.notify(settings_.postfx ? "post-processing ON" : "post-processing OFF");
    }
    if (IsKeyPressed(KEY_F3)) ctx_.notify(postfx_.reload() ? "shader reloaded: " + postfx_.status() : "shader: " + postfx_.status());
    if (IsKeyPressed(KEY_F4)) {
        input_.relativeAim = !input_.relativeAim;
        if (input_.relativeAim) DisableCursor();  // locks + hides, GetMouseDelta keeps working
        else EnableCursor();
        ctx_.notify(input_.relativeAim ? "relative aim (cursor locked)" : "absolute aim");
    }
    if (IsKeyPressed(KEY_F5)) {
        if (ctx_.recording) stopRecording();
        else if (!ctx_.replaying) startRecording();
    }
    if (IsKeyPressed(KEY_F6) && !ctx_.recording) startReplay();
    if (IsKeyPressed(KEY_F7)) {
        const int count = GetMonitorCount();
        if (count > 1) {
            SetWindowMonitor((GetCurrentMonitor() + 1) % count);
            ctx_.notify(TextFormat("moved to monitor %d", GetCurrentMonitor()));
        } else {
            ctx_.notify("single monitor");
        }
    }
    if (IsKeyPressed(KEY_F8)) {
        input_.touchMode = !input_.touchMode;
        ctx_.notify(input_.touchMode ? "touch joystick ON" : "touch joystick OFF");
    }
    if (IsKeyPressed(KEY_F9) && sceneId_ != SceneId::Showcase) {
        if (IsWindowState(FLAG_WINDOW_TOPMOST)) ClearWindowState(FLAG_WINDOW_TOPMOST);
        else SetWindowState(FLAG_WINDOW_TOPMOST);
        ctx_.notify(IsWindowState(FLAG_WINDOW_TOPMOST) ? "always on top" : "normal z-order");
    }
    if (IsKeyPressed(KEY_F10)) ToggleBorderlessWindowed();
    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
        settings_.fullscreen = IsWindowFullscreen();
    }
    if (IsKeyPressed(KEY_F12)) takeScreenshot("manual");
    if (ctrl && IsKeyPressed(KEY_O)) {
        static const float steps[] = {1.f, 0.85f, 0.6f};
        opacityStep_ = (opacityStep_ + 1) % 3;
        SetWindowOpacity(steps[opacityStep_]);
    }
    if (ctrl && IsKeyPressed(KEY_M)) {
        if (IsWindowMaximized()) RestoreWindow();
        else MaximizeWindow();
        SetWindowFocused();
    }
    if (ctrl && IsKeyPressed(KEY_N)) MinimizeWindow();
    if (ctrl && IsKeyPressed(KEY_R)) {  // reset + center on the current monitor
        const int m = GetCurrentMonitor();
        SetWindowSize(opts_.width, opts_.height);
        const Vector2 mp = GetMonitorPosition(m);
        SetWindowPosition(static_cast<int>(mp.x) + (GetMonitorWidth(m) - opts_.width) / 2, static_cast<int>(mp.y) + (GetMonitorHeight(m) - opts_.height) / 2);
        SetWindowTitle(TextFormat("raylib_demo01 - NEON HARVEST (%s)", GetMonitorName(m)));
    }
    if (ctrl && IsKeyPressed(KEY_E)) {
        const std::string blob = scores_.serialize();
        const bool ok = storage_.exportAsCode("exports/scores_blob.h", blob.data(), static_cast<int>(blob.size()));
        ctx_.notify(ok ? "ExportDataAsCode -> exports/scores_blob.h" : "export failed");
    }
}

void App::handleDroppedFiles() {
    if (!IsFileDropped()) return;
    FilePathList files = LoadDroppedFiles();
    for (unsigned int i = 0; i < files.count; ++i) {
        const char* p = files.paths[i];
        if (IsFileExtension(p, ".png;.bmp;.jpg;.jpeg;.qoi;.tga")) {
            Image img = LoadImage(p);
            ctx_.notify(assets_.setPlayerSkin(img) ? TextFormat("new ship skin: %s", GetFileName(p)) : "could not load image");
        } else if (IsFileExtension(p, ".gif")) {
            int frames = 0;
            // small files through the memory loader, big ones streamed from disk
            Image anim{};
            if (GetFileLength(p) < 16 * 1024 * 1024) {
                int size = 0;
                unsigned char* data = LoadFileData(p, &size);
                anim = LoadImageAnimFromMemory(".gif", data, size, &frames);
                UnloadFileData(data);
            } else {
                anim = LoadImageAnim(p, &frames);  // all frames stacked vertically
            }
            if (IsImageValid(anim) && frames > 0) {
                Image first = ImageFromImage(anim, {0, 0, static_cast<float>(anim.width), static_cast<float>(anim.height / frames)});
                assets_.setPlayerSkin(first);
                ctx_.notify(TextFormat("GIF with %d frames -> skin", frames));
            }
            UnloadImage(anim);
        } else if (IsFileExtension(p, ".ini")) {
            char* t = LoadFileText(p);
            if (t && ParseSettings(t, settings_)) {
                audio_.applySettings(settings_);
                ctx_.notify("settings imported");
            }
            if (t) UnloadFileText(t);
        } else if (IsFileExtension(p, ".rae")) {
            const std::string dst = storage_.path(std::string("replays/") + GetFileName(p));
            FileCopy(p, dst.c_str());
            ctx_.notify(TextFormat("replay copied to %s", dst.c_str()));
        } else {
            ctx_.notify(TextFormat("unsupported file: %s (%d bytes)", GetFileName(p), GetFileLength(p)));
        }
    }
    UnloadDroppedFiles(files);
}

void App::takeScreenshot(const char* tag) {
    const std::string name = TextFormat("shot_%s_%03d.png", tag, screenshotCounter_++);
    const Vector2 dpi = GetWindowScaleDPI();
    bool ok = false;
    if (dpi.x > 1.01f || dpi.y > 1.01f) {
        // raylib 6.0 BUG: TakeScreenshot() reads render size * DPI on HiDPI (render size is already
        // physical) -> 4x oversized PNG with the frame in one quadrant. Read the framebuffer ourselves.
        Image img = LoadImageFromScreen();
        if (ctx_.autopilot) ImageResize(&img, img.width / 2, img.height / 2);  // keep CI fast
        ok = ExportImage(img, storage_.path("screenshots/" + name).c_str());
        UnloadImage(img);
    } else {
        TakeScreenshot(name.c_str());  // written relative to the working directory
        std::string src = name;
        if (!FileExists(src.c_str())) src = std::string(GetWorkingDirectory()) + "/" + name;
        ok = storage_.moveInto(src, "screenshots/" + name);
    }
    if (ok) {
        ++smokeShots_;
        const int pruned = storage_.pruneOldest("screenshots", ".png", 30);
        if (!ctx_.autopilot) ctx_.notify(TextFormat("screenshot saved%s", pruned ? " (old ones pruned)" : ""));
    } else {
        TraceLog(LOG_WARNING, "SCREENSHOT: could not save %s", name.c_str());
    }
}

// ---------------------------------------------------------------------------
// automation events: record raw input, replay it frame-exact with the same seed
// ---------------------------------------------------------------------------
void App::startRecording() {
    if (aeLoaded_) UnloadAutomationEventList(aeList_);
    aeList_ = LoadAutomationEventList(nullptr);  // empty list, capacity MAX_AUTOMATION_EVENTS
    aeLoaded_ = true;
    SetAutomationEventList(&aeList_);
    SetAutomationEventBaseFrame(0);
    replaySeed_ = static_cast<uint64_t>(GetRandomValue(1, 0x7FFFFFFF));
    ctx_.forcedSeed = replaySeed_;
    ctx_.deterministic = true;
    ctx_.recording = true;
    ctx_.go(SceneId::Gameplay);
    StartAutomationEventRecording();
    ctx_.notify("recording input (F5 to stop)");
}

void App::stopRecording() {
    StopAutomationEventRecording();
    ctx_.recording = false;
    ctx_.deterministic = false;
    const std::string path = storage_.path("replays/last.rae");
    const bool ok = ExportAutomationEventList(aeList_, path.c_str());
    storage_.writeTextAtomic("replays/last.seed", std::to_string(replaySeed_));
    ctx_.notify(TextFormat("%u events %s -> replays/last.rae", aeList_.count, ok ? "saved" : "NOT saved"));
}

void App::startReplay() {
    const std::string path = storage_.path("replays/last.rae");
    std::string seedText;
    if (!FileExists(path.c_str()) || !storage_.readText("replays/last.seed", seedText)) {
        ctx_.notify("no replay recorded yet (F5)");
        return;
    }
    if (aeLoaded_) UnloadAutomationEventList(aeList_);
    aeList_ = LoadAutomationEventList(path.c_str());
    aeLoaded_ = true;
    if (aeList_.count == 0) {
        ctx_.notify("replay is empty");
        return;
    }
    ctx_.forcedSeed = static_cast<uint64_t>(TextToInteger(Trim(seedText).c_str()));
    ctx_.deterministic = true;
    ctx_.replaying = true;
    playFrame_ = 0;
    playIndex_ = 0;
    ctx_.go(SceneId::Gameplay);
    ctx_.notify(TextFormat("replaying %u events", aeList_.count));
}

void App::stepReplay() {
    if (!ctx_.replaying) return;
    while (playIndex_ < aeList_.count && aeList_.events[playIndex_].frame == playFrame_) {
        PlayAutomationEvent(aeList_.events[playIndex_]);
        ++playIndex_;
    }
    ++playFrame_;
    if (playIndex_ >= aeList_.count) {
        ctx_.replaying = false;
        ctx_.deterministic = false;
        ctx_.notify("replay finished");
    }
}

// ---------------------------------------------------------------------------
void App::smokeStep() {
    if (sceneId_ != smokeLastScene_) {
        smokeLastScene_ = sceneId_;
        smokeSceneFrames_ = 0;
    }
    ++smokeSceneFrames_;
    // time based so the tour is independent of the achieved frame rate
    const float prev = smokeSceneTime_;
    smokeSceneTime_ = smokeSceneFrames_ == 1 ? 0.f : smokeSceneTime_ + GetFrameTime();
    auto crossed = [&](float t) { return prev < t && smokeSceneTime_ >= t; };
    if (crossed(0.4f)) takeScreenshot(scene_->name());
    if (sceneId_ == SceneId::Gameplay && crossed(4.5f)) takeScreenshot("GameplayLate");
    if (sceneId_ == SceneId::Showcase)
        for (int p = 1; p < 8; ++p)
            if (crossed(0.4f + 0.5f * p)) takeScreenshot(TextFormat("ShowcasePage%d", p + 1));
    if (ctx_.frame == 5) {  // window API self-test (non destructive)
        SetWindowTitle("raylib_demo01 - smoke test");
        SetWindowOpacity(0.98f);
        SetWindowOpacity(1.0f);
        TraceLog(LOG_INFO, "SMOKE: monitor %d '%s' %dx%d @%dHz dpi=%.2f handle=%p", GetCurrentMonitor(), GetMonitorName(GetCurrentMonitor()),
                 GetMonitorWidth(GetCurrentMonitor()), GetMonitorHeight(GetCurrentMonitor()), GetMonitorRefreshRate(GetCurrentMonitor()),
                 GetWindowScaleDPI().x, GetWindowHandle());
    }
    if (ctx_.frame == 5)
        TraceLog(LOG_INFO, "SMOKE: screen %dx%d render %dx%d framebuffer %dx%d", GetScreenWidth(), GetScreenHeight(), GetRenderWidth(),
                 GetRenderHeight(), rlGetFramebufferWidth(), rlGetFramebufferHeight());
}

void App::drawOverlay(Rectangle dest) {
    if (settings_.showFps) DrawFPS(GetScreenWidth() - 90, GetScreenHeight() - 24);
    if (!debug_) return;
    const int m = GetCurrentMonitor();
    const Vector2 wp = GetWindowPosition(), dpi = GetWindowScaleDPI(), mp = GetMonitorPosition(m);
    std::vector<std::string> lines = {
        TextFormat("raylib %s | %s | %d fps | frame %u | %.2f ms | t=%.1fs", RAYLIB_VERSION, scene_->name(), GetFPS(), ctx_.frame,
                   GetFrameTime() * 1000.f, GetTime()),
        TextFormat("screen %dx%d render %dx%d dpi %.2fx%.2f  viewport %.0fx%.0f@%.0f,%.0f", GetScreenWidth(), GetScreenHeight(), GetRenderWidth(),
                   GetRenderHeight(), dpi.x, dpi.y, dest.width, dest.height, dest.x, dest.y),
        TextFormat("monitor %d/%d '%s' %dx%d (%dx%d mm) %dHz at %.0f,%.0f  window at %.0f,%.0f", m, GetMonitorCount(), GetMonitorName(m), GetMonitorWidth(m),
                   GetMonitorHeight(m), GetMonitorPhysicalWidth(m), GetMonitorPhysicalHeight(m), GetMonitorRefreshRate(m), mp.x, mp.y, wp.x, wp.y),
        TextFormat("fullscreen=%d maximized=%d minimized=%d focused=%d hidden=%d topmost=%d vsync=%d cursorHidden=%d onScreen=%d", IsWindowFullscreen(),
                   IsWindowMaximized(), IsWindowMinimized(), IsWindowFocused(), IsWindowHidden(), IsWindowState(FLAG_WINDOW_TOPMOST),
                   IsWindowState(FLAG_VSYNC_HINT), IsCursorHidden(), IsCursorOnScreen()),
        TextFormat("mouse %.0f,%.0f (virtual) raw-delta %.1f,%.1f  gesture %d  %s", GetMousePosition().x, GetMousePosition().y, GetMouseDelta().x,
                   GetMouseDelta().y, input_.lastGesture, input_.describeGamepad().c_str()),
        TextFormat("audio %s master %.2f vu %.3f | postfx %s", audio_.ready() ? "ready" : "OFF", audio_.ready() ? GetMasterVolume() : 0.f, audio_.vuLevel(),
                   postfx_.status().c_str()),
        TextFormat("cwd %s | app %s | handle %p", GetWorkingDirectory(), GetApplicationDirectory(), GetWindowHandle()),
        TextFormat("rec=%d replay=%d events=%u seed(next)=%llu  F5 rec F6 replay", ctx_.recording, ctx_.replaying, aeLoaded_ ? aeList_.count : 0u,
                   static_cast<unsigned long long>(ctx_.forcedSeed)),
    };
    for (const auto& l : RecentLogs(8)) lines.push_back(TextFormat("[%s] %s", LogLevelName(l.level), l.text.c_str()));
    const int h = static_cast<int>(lines.size()) * 16 + 12;
    DrawRectangle(8, 8, GetScreenWidth() - 16, h, Fade(BLACK, 0.78f));
    for (size_t i = 0; i < lines.size(); ++i)
        DrawTextEx(assets_.mono, lines[i].c_str(), {16, 14 + static_cast<float>(i) * 16}, 15, 1, i < 8 ? RAYWHITE : Fade(SKYBLUE, 0.9f));
}

}  // namespace rd
