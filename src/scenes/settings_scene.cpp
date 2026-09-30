// Settings: sliders/toggles/cycles applied live (volume, fullscreen, vsync, fps),
// persisted atomically to saves/settings.ini.
#include "app/context.hpp"
#include "ui/ui_logic.hpp"

namespace rd {

namespace {

class SettingsScene final : public Scene {
public:
    const char* name() const override { return "Settings"; }

    void enter(AppContext&) override {
        nav_.setCount(kRows);
        nav_.set(0);
        dirty_ = false;
    }

    void exit(AppContext& ctx) override {
        if (dirty_) ctx.notify(ctx.saveSettings() ? "Settings saved" : "Settings save FAILED");
    }

    void update(AppContext& ctx, float) override {
        if (ctx.input->menuUp()) nav_.prev();
        if (ctx.input->menuDown()) nav_.next();
        if (ctx.input->menuBack()) ctx.go(SceneId::Title);
        if (ctx.autopilot) ctx.go(SceneId::Title);
    }

    static void ApplyWindow(AppContext& ctx) {
        Settings& s = *ctx.settings;
        if (s.fullscreen != IsWindowFullscreen()) ToggleFullscreen();
        if (s.vsync) SetWindowState(FLAG_VSYNC_HINT);
        else ClearWindowState(FLAG_VSYNC_HINT);
        SetTargetFPS(s.targetFps);
    }

    void draw(AppContext& ctx) override {
        const UiTheme& th = ctx.theme;
        Settings& s = *ctx.settings;
        ClearBackground(GetColor(0x080C18FF));
        DrawRectangleGradientV(0, 0, ctx.vw, ctx.vh, GetColor(0x0D1430FF), GetColor(0x05070FFF));
        UiTextCentered(th, ctx.assets->uiIsTtf ? "SETTINGS 设置" : "SETTINGS", {ctx.vw * 0.5f, 36}, 34, th.accent);

        const auto rows = VStack({ctx.vw * 0.5f - 230, 64, 460, 440}, kRows, 34, 5);
        auto R = [&](int i) { return rows[static_cast<size_t>(i)]; };
        auto F = [&](int i) { return nav_.index() == i; };
        bool audioChanged = false;
        audioChanged |= UiSlider(th, R(0), "Master volume", &s.masterVolume, 0.f, 1.f, F(0));
        audioChanged |= UiSlider(th, R(1), "Music volume", &s.musicVolume, 0.f, 1.f, F(1));
        audioChanged |= UiSlider(th, R(2), "SFX volume", &s.sfxVolume, 0.f, 1.f, F(2));
        if (audioChanged) {
            ctx.audio->applySettings(s);
            dirty_ = true;
        }
        bool winChanged = false;
        winChanged |= UiToggle(th, R(3), "Fullscreen (F11)", &s.fullscreen, F(3));
        winChanged |= UiToggle(th, R(4), "VSync", &s.vsync, F(4));
        dirty_ |= UiToggle(th, R(5), "Post-processing shader (F2)", &s.postfx, F(5));
        dirty_ |= UiToggle(th, R(6), "Show FPS", &s.showFps, F(6));
        dirty_ |= UiToggle(th, R(7), "Screen shake", &s.screenShake, F(7));
        dirty_ |= UiToggle(th, R(8), "Crosshair", &s.crosshair, F(8));
        int d = 0;
        if (UiCycle(th, R(9), "Difficulty", DifficultyName(s.difficulty), F(9), &d)) {
            s.difficulty = (s.difficulty + d + 3) % 3;
            dirty_ = true;
        }
        static const int fpsSteps[] = {30, 60, 120, 144, 240};
        int idx = 1;
        for (int i = 0; i < 5; ++i)
            if (fpsSteps[i] == s.targetFps) idx = i;
        if (UiCycle(th, R(10), "Target FPS", TextFormat("%d", s.targetFps), F(10), &d)) {
            s.targetFps = fpsSteps[(idx + d + 5) % 5];
            winChanged = true;
        }
        if (UiButton(th, R(11), "BACK (Esc)", F(11))) ctx.go(SceneId::Title);
        if (winChanged) {
            ApplyWindow(ctx);
            dirty_ = true;
        }
        UiText(th, TextFormat("saved to %s   master=%.2f (GetMasterVolume)", ctx.storage->path("settings.ini").c_str(),
                              ctx.audio->ready() ? GetMasterVolume() : 0.f),
               {16, ctx.vh - 24.f}, 14, th.muted);
    }

private:
    static constexpr int kRows = 12;
    FocusNav nav_;
    bool dirty_ = false;
};

}  // namespace

std::unique_ptr<Scene> MakeSettingsScene() { return std::make_unique<SettingsScene>(); }

}  // namespace rd
