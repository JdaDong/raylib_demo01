// Title: 3D wireframe backdrop (BeginMode3D + orbital camera), animated logo,
// menu (mouse/keyboard/gamepad), highscore table, seed from clipboard.
#include "app/context.hpp"
#include "core/easing.hpp"
#include "gfx/backdrop3d.hpp"
#include "ui/ui_logic.hpp"

#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace rd {

namespace {

class TitleScene final : public Scene {
public:
    const char* name() const override { return "Title"; }

    void enter(AppContext& ctx) override {
        cam_ = Camera3D{};
        cam_.position = {13.f, 7.5f, 13.f};
        cam_.target = {0.f, 0.f, 0.f};
        cam_.up = {0.f, 1.f, 0.f};
        cam_.fovy = 45.f;
        cam_.projection = CAMERA_PERSPECTIVE;
        nav_.setCount(kItems);
        t_ = 0.f;
        ShowCursor();
        ctx.audio->setHum(false);
        backdrop_.init(*ctx.assets, *ctx.storage);
        selfCheckCameraApi(ctx);
    }

    void exit(AppContext&) override { backdrop_.shutdown(); }

    // runtime parity check: the window-sized helpers must agree with their *Ex twins
    void selfCheckCameraApi(AppContext&) {
        const Vector3 p{1.f, 2.f, 3.f};
        const Vector2 a = GetWorldToScreen(p, cam_), b = GetWorldToScreenEx(p, cam_, GetScreenWidth(), GetScreenHeight());
        const Vector2 m{GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f};
        const Ray r1 = GetScreenToWorldRay(m, cam_), r2 = GetScreenToWorldRayEx(m, cam_, GetScreenWidth(), GetScreenHeight());
        const Matrix view = GetCameraMatrix(cam_);
        const bool ok = Vector2Distance(a, b) < 0.01f && Vector3Distance(r1.direction, r2.direction) < 1e-4f;
        TraceLog(ok ? LOG_INFO : LOG_WARNING, "TITLE: camera API parity %s (view m12=%.2f)", ok ? "ok" : "MISMATCH", view.m12);
    }

    void update(AppContext& ctx, float dt) override {
        t_ += dt;
        UpdateCamera(&cam_, CAMERA_ORBITAL);
        // UpdateCameraPro: extra slow bob, no user input
        UpdateCameraPro(&cam_, {0.f, 0.f, std::sin(t_) * 0.01f}, {0.f, 0.f, 0.f}, 0.f);
        backdrop_.update(dt);
        InputMapper& in = *ctx.input;
        if (in.menuUp()) { nav_.prev(); ctx.audio->play(Sfx::Ui); }
        if (in.menuDown()) { nav_.next(); ctx.audio->play(Sfx::Ui); }
        if (in.menuAccept()) activate(ctx, nav_.index());

        // Ctrl+V: paste a seed from the clipboard
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER)) && IsKeyPressed(KEY_V)) {
            const char* clip = GetClipboardText();
            const int seed = clip ? TextToInteger(clip) : 0;
            if (seed > 0) {
                ctx.forcedSeed = static_cast<uint64_t>(seed);
                ctx.notify(TextFormat("Seed %d loaded from clipboard", seed));
            } else {
                ctx.notify("Clipboard has no numeric seed");
            }
        }
        if (ctx.autopilot && t_ > 0.6f && !ctx.hasPending) {
            static const int plan[] = {0, 1, 2, 4};  // play, showcase, settings, quit (never the URL)
            activate(ctx, plan[std::min(ctx.smokeTitleVisits++, 3)]);
        }
    }

    void activate(AppContext& ctx, int i) {
        ctx.audio->play(Sfx::Ui);
        switch (i) {
            case 0: ctx.go(SceneId::Gameplay); break;
            case 1: ctx.go(SceneId::Showcase); break;
            case 2: ctx.go(SceneId::Settings); break;
            case 3: OpenURL("https://www.raylib.com/cheatsheet/cheatsheet.html"); break;  // explicit click only
            case 4: ctx.quitRequested = true; break;
            default: break;
        }
    }

    void draw(AppContext& ctx) override {
        const UiTheme& th = ctx.theme;
        ClearBackground(GetColor(0x070A14FF));
        DrawRectangleGradientV(0, 0, ctx.vw, ctx.vh, GetColor(0x0B1030FF), GetColor(0x1A0B2AFF));

        // --- 3D backdrop --------------------------------------------------------
        BeginMode3D(cam_);
        DrawGrid(24, 1.f);
        backdrop_.draw(cam_, GetMousePosition(), ctx.vw, ctx.vh);
        DrawCubeWires({0, 1.2f, 0}, 2, 2, 2, Fade(th.accent, 0.5f));
        DrawCube({0, 3.9f + std::sin(t_) * 0.2f, 0}, 0.3f, 0.3f, 0.3f, Fade(GetColor(0xFF4D6DFF), 0.8f));
        DrawSphereWires({4, 1, -3}, 1.2f, 8, 12, Fade(GOLD, 0.35f));
        DrawCylinderWires({-4, 0, 3}, 0.f, 1.2f, 2.5f, 6, Fade(VIOLET, 0.45f));
        EndMode3D();
        const Vector2 label = GetWorldToScreenEx({0, 4.4f, 0}, cam_, ctx.vw, ctx.vh);
        DrawText("raylib 3D", static_cast<int>(label.x) - 26, static_cast<int>(label.y), 10, Fade(th.muted, 0.8f));
        if (!backdrop_.hoverInfo().empty()) UiText(th, backdrop_.hoverInfo().c_str(), {16, ctx.vh - 64.f}, 14, GOLD);

        // --- logo -------------------------------------------------------------------
        const char* title = "NEON HARVEST";
        const float s = 72.f * (0.9f + 0.1f * ease::elasticOut(std::min(1.f, t_ * 0.8f)));
        const Vector2 m = MeasureTextEx(ctx.assets->title, title, s, 4);
        const Vector2 pos{ctx.vw * 0.5f, 100.f};
        DrawTextPro(ctx.assets->title, title, {pos.x + 4, pos.y + 4}, {m.x * 0.5f, m.y * 0.5f}, std::sin(t_ * 0.7f) * 1.5f, s, 4, Fade(BLACK, 0.5f));
        DrawTextPro(ctx.assets->title, title, pos, {m.x * 0.5f, m.y * 0.5f}, std::sin(t_ * 0.7f) * 1.5f, s, 4,
                    ColorFromHSV(std::fmod(t_ * 40.f, 360.f), 0.55f, 1.f));
        const char* sub = ctx.assets->uiIsTtf ? "霓虹收割者 · a raylib 6.0 API tour" : "a raylib 6.0 API tour";
        UiTextCentered(th, sub, {ctx.vw * 0.5f, 148.f}, 20, th.muted);

        // --- menu ---------------------------------------------------------------------
        static const char* items[kItems] = {"PLAY", "API SHOWCASE", "SETTINGS", "CHEATSHEET (web)", "QUIT"};
        const auto rows = VStack({ctx.vw * 0.5f - 130, 180, 260, 280}, kItems, 42, 10);
        for (int i = 0; i < kItems; ++i) {
            const Rectangle r = rows[static_cast<size_t>(i)];
            if (CheckCollisionPointRec(GetMousePosition(), r) && Vector2Length(GetMouseDelta()) > 0) nav_.set(i);
            if (UiButton(th, r, items[i], nav_.index() == i)) activate(ctx, i);
        }

        // --- highscores ------------------------------------------------------------------
        const Rectangle hs{ctx.vw - 250.f, 190, 230, 270};
        UiPanel(th, hs, Fade(WHITE, 0.9f));
        UiText(th, "HIGHSCORES", {hs.x + 14, hs.y + 10}, 20, th.accent);
        const auto& e = ctx.scores->entries();
        if (e.empty()) UiText(th, "no runs yet", {hs.x + 14, hs.y + 44}, 18, th.muted);
        for (size_t i = 0; i < e.size() && i < 10; ++i) {
            const float y = hs.y + 40 + static_cast<float>(i) * 22;
            UiText(th, TextFormat("%2d. %s", static_cast<int>(i + 1), e[i].name.c_str()), {hs.x + 14, y}, 18, RAYWHITE);
            const std::string sc = FormatScore(e[i].score);
            const Vector2 w = UiMeasure(th, sc.c_str(), 18);
            UiText(th, sc.c_str(), {hs.x + hs.width - w.x - 14, y}, 18, GOLD);
        }

        UiText(th, ctx.input->describeBindings().c_str(), {16, ctx.vh - 44.f}, 16, th.muted);
        UiText(th, "F1 debug  F5 rec  F6 replay  F11 fullscreen  F12 screenshot  Ctrl+V paste seed", {16, ctx.vh - 24.f}, 16, th.muted);
    }

private:
    static constexpr int kItems = 5;
    Camera3D cam_{};
    Backdrop3D backdrop_;
    FocusNav nav_;
    float t_ = 0.f;
};

}  // namespace

std::unique_ptr<Scene> MakeTitleScene() { return std::make_unique<TitleScene>(); }

}  // namespace rd
