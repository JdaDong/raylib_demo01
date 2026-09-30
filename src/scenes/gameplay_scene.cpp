// Gameplay: fixed-step World simulation, events -> audio/particles/shake/rumble,
// Camera2D follow + zoom, minimap pre-pass, pause menu.
#include "app/context.hpp"
#include "core/camera_rig.hpp"
#include "core/timer.hpp"
#include "game/particles.hpp"
#include "game/world.hpp"
#include "gfx/renderer.hpp"
#include "ui/ui_logic.hpp"

#include <cmath>
#include <memory>

namespace rd {

namespace {

class GameplayScene final : public Scene {
public:
    const char* name() const override { return "Gameplay"; }

    void enter(AppContext& ctx) override {
        WorldConfig cfg;
        cfg.difficulty = DifficultyScale(ctx.settings->difficulty);
        seed_ = ctx.pickSeed();
        world_ = std::make_unique<World>(cfg, seed_);
        world_->player().radius = ctx.assets->shipRadius > 6.f ? ctx.assets->shipRadius : 14.f;
        particles_.clear();
        fx_.reseed(seed_ ^ 0xABCDEFull);
        rig_ = CameraRig{};
        rig_.snap(world_->player().pos, world_->bounds(), {static_cast<float>(ctx.vw), static_cast<float>(ctx.vh)});
        renderer_.init(180, 100);
        step_.reset();
        paused_ = false;
        deadTimer_ = 0.f;
        bannerT_ = 0.f;
        HideCursor();
        ctx.audio->setHum(true, 0.25f);
        ctx.audio->setPad(true);
        TraceLog(LOG_INFO, "GAME: new run seed=%llu difficulty=%s", static_cast<unsigned long long>(seed_), DifficultyName(ctx.settings->difficulty));
    }

    void exit(AppContext& ctx) override {
        renderer_.shutdown();
        ShowCursor();
        if (IsCursorHidden()) ShowCursor();
        ctx.audio->setHum(false);
        ctx.audio->setPad(false);
        ctx.audio->setDanger(0.f);
    }

    void update(AppContext& ctx, float dt) override {
        InputMapper& in = *ctx.input;
        if (in.pausePressed() && world_->state() == WorldState::Playing) setPaused(ctx, !paused_);
        if (paused_) {
            updatePauseMenu(ctx);
            return;
        }

        // Ctrl+C: share the run
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER)) && IsKeyPressed(KEY_C)) {
            SetClipboardText(TextFormat("NEON HARVEST seed %llu - score %d wave %d", static_cast<unsigned long long>(seed_), world_->score(), world_->wave()));
            ctx.notify("Run info copied to clipboard");
        }

        // zoom with the wheel (2D camera)
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.f) rig_.zoomTarget += wheel * 0.1f;

        InputFrame frame = in.poll(rig_.cam, world_->player().pos, ctx.vw, ctx.vh);
        if (ctx.autopilot) frame = autopilot();
        aimWorld_ = frame.aimWorld;
        aimScreen_ = in.lastAimScreen;

        // deterministic mode (record/replay): exactly 2 steps of 1/120 per frame
        const float frameDt = ctx.deterministic ? 1.f / 60.f : dt;
        const int steps = step_.advance(frameDt);
        for (int i = 0; i < steps; ++i) {
            world_->step(frame, step_.step());
            frame.dash = false;  // edge-triggered: only the first sub-step
            handleEvents(ctx);
        }

        particles_.update(dt);
        renderer_.update(dt);
        shake_.update(dt);
        if (bannerT_ > 0.f) bannerT_ += dt * 0.5f;
        const Vector2 view{static_cast<float>(ctx.vw), static_cast<float>(ctx.vh)};
        rig_.update(world_->player().pos, dt, world_->bounds(), view);

        const Player& p = world_->player();
        ctx.audio->setDanger(1.f - p.hp / p.maxHp);
        ctx.audio->setHum(true, 0.15f + Vector2Length(p.vel) / 900.f);

        if (world_->state() == WorldState::Dead) {
            deadTimer_ += dt;
            if (deadTimer_ > 1.6f) {
                ctx.last = {world_->score(), world_->wave(), world_->kills(), world_->elapsed(), seed_};
                ctx.go(SceneId::GameOver);
            }
        }
        if (ctx.autopilot && world_->elapsed() > 6.f) world_->damagePlayer(10000.f, {0, 0});
    }

    void prerender(AppContext& ctx) override {
        renderer_.renderMinimap(*world_, cameraWithShake(), {static_cast<float>(ctx.vw), static_cast<float>(ctx.vh)});
    }

    void draw(AppContext& ctx) override {
        ClearBackground(GetColor(0x05070FFF));
        renderer_.drawWorld(ctx, *world_, particles_, cameraWithShake(), aimWorld_);
        renderer_.drawHud(ctx, *world_, bannerT_, ctx.settings->crosshair && !paused_, aimScreen_);
        if (world_->state() == WorldState::Dead) {
            DrawRectangle(0, 0, ctx.vw, ctx.vh, Fade(BLACK, std::min(0.6f, deadTimer_ * 0.5f)));
            UiTextCentered(ctx.theme, "SHIP DESTROYED", {ctx.vw * 0.5f, ctx.vh * 0.5f}, 40, ctx.theme.warn);
        }
        if (paused_) drawPauseMenu(ctx);
    }

private:
    Camera2D cameraWithShake() {
        Camera2D c = rig_.cam;
        if (shakeEnabled_) {
            const Vector2 o = shake_.offset(fx_);
            c.target = Vector2Add(c.target, o);
            c.rotation = shake_.angle(fx_);
        }
        return c;
    }

    void setPaused(AppContext& ctx, bool p) {
        paused_ = p;
        nav_.setCount(3);
        nav_.set(0);
        ctx.audio->pauseAll(p);
        if (p) ShowCursor();
        else HideCursor();
    }

    void handleEvents(AppContext& ctx) {
        shakeEnabled_ = ctx.settings->screenShake;
        const float halfW = ctx.vw * 0.5f;
        for (const GameEvent& ev : world_->drainEvents()) {
            const Vector2 sp = GetWorldToScreen2D(ev.pos, rig_.cam);
            const float pan = 0.5f + Clamp((sp.x - halfW) / ctx.vw, -0.5f, 0.5f) * 0.8f;
            switch (ev.type) {
                case EventType::Shot:
                    ctx.audio->play(Sfx::Shoot, 0.08f, pan);
                    break;
                case EventType::EnemyHit: {
                    ctx.audio->play(Sfx::Hit, 0.15f, pan);
                    BurstParams bp;
                    bp.count = 6;
                    bp.color = WHITE;
                    bp.colorEnd = Fade(SKYBLUE, 0.f);
                    bp.size = 2.5f;
                    particles_.burst(ev.pos, bp, fx_);
                    break;
                }
                case EventType::EnemyKilled: {
                    ctx.audio->play(Sfx::Explode, 0.2f, pan);
                    BurstParams bp;
                    bp.count = 28;
                    bp.speedMax = 320.f;
                    bp.color = ColorFromHSV(static_cast<float>(static_cast<int>(ev.kind) * 70 + 330 % 360), 0.8f, 1.f);
                    bp.colorEnd = Fade(ORANGE, 0.f);
                    bp.size = 4.f;
                    particles_.burst(ev.pos, bp, fx_);
                    renderer_.addExplosion(ev.pos, ev.kind == EnemyKind::Tank ? 1.6f : 0.9f);
                    shake_.add(ev.kind == EnemyKind::Tank ? 0.45f : 0.18f);
                    break;
                }
                case EventType::PlayerHit:
                    ctx.audio->play(Sfx::Hurt, 0.1f, pan);
                    shake_.add(0.5f);
                    ctx.input->rumble(0.8f, 0.25f);
                    ctx.postfx->flash = {0.25f, 0.f, 0.f};
                    break;
                case EventType::PlayerDied: {
                    ctx.audio->play(Sfx::Explode, 0.f, pan);
                    BurstParams bp;
                    bp.count = 120;
                    bp.speedMax = 480.f;
                    bp.lifeMax = 1.4f;
                    bp.color = SKYBLUE;
                    bp.colorEnd = Fade(VIOLET, 0.f);
                    particles_.burst(ev.pos, bp, fx_);
                    renderer_.addExplosion(ev.pos, 2.5f);
                    shake_.add(1.f);
                    ctx.input->rumble(1.f, 0.6f);
                    break;
                }
                case EventType::PickupGem: ctx.audio->play(Sfx::Pickup, 0.1f, pan); break;
                case EventType::PickupHeal: ctx.audio->play(Sfx::Heal, 0.f, pan); break;
                case EventType::Dash: {
                    ctx.audio->play(Sfx::Dash, 0.05f, pan);
                    BurstParams bp;
                    bp.count = 14;
                    bp.color = SKYBLUE;
                    bp.colorEnd = Fade(BLUE, 0.f);
                    bp.direction = std::atan2(-world_->player().dashDir.y, -world_->player().dashDir.x);
                    bp.spread = 1.2f;
                    particles_.burst(ev.pos, bp, fx_);
                    break;
                }
                case EventType::WaveStarted:
                    ctx.audio->play(Sfx::Wave);
                    bannerT_ = 0.001f;
                    break;
                case EventType::EnemyShot: ctx.audio->play(Sfx::Shoot, 0.3f, pan); break;
                default: break;
            }
        }
        // decay the red damage flash in the post shader
        ctx.postfx->flash = Vector3Scale(ctx.postfx->flash, 0.9f);
    }

    InputFrame autopilot() {
        // circle around the arena, shoot the nearest enemy (used by --smoke)
        InputFrame f;
        const float t = world_->elapsed();
        f.move = {std::cos(t * 0.9f), std::sin(t * 1.3f)};
        f.fire = true;
        f.hasAim = true;
        f.aimWorld = Vector2Add(world_->player().pos, {100, 0});
        float best = 1e9f;
        for (const Enemy& e : world_->enemies()) {
            const float d = Vector2Distance(e.pos, world_->player().pos);
            if (d < best) { best = d; f.aimWorld = e.pos; }
        }
        f.dash = std::fmod(t, 2.f) < 0.02f;
        return f;
    }

    void updatePauseMenu(AppContext& ctx) {
        InputMapper& in = *ctx.input;
        if (in.menuUp()) nav_.prev();
        if (in.menuDown()) nav_.next();
        if (in.menuAccept()) pauseAction(ctx, nav_.index());
    }

    void pauseAction(AppContext& ctx, int i) {
        ctx.audio->play(Sfx::Ui);
        if (i == 0) setPaused(ctx, false);
        else if (i == 1) {
            setPaused(ctx, false);
            ctx.forcedSeed = seed_;
            ctx.go(SceneId::Gameplay);
        } else ctx.go(SceneId::Title);
    }

    void drawPauseMenu(AppContext& ctx) {
        DrawRectangle(0, 0, ctx.vw, ctx.vh, Fade(BLACK, 0.55f));
        const Rectangle panel{ctx.vw * 0.5f - 160, ctx.vh * 0.5f - 130, 320, 260};
        UiPanel(ctx.theme, panel);
        UiTextCentered(ctx.theme, "PAUSED", {panel.x + panel.width * 0.5f, panel.y + 34}, 32, ctx.theme.accent);
        static const char* items[3] = {"RESUME", "RESTART (same seed)", "QUIT TO TITLE"};
        const auto rows = VStack({panel.x + 30, panel.y + 60, panel.width - 60, panel.height - 70}, 3, 40, 10);
        for (int i = 0; i < 3; ++i)
            if (UiButton(ctx.theme, rows[static_cast<size_t>(i)], items[i], nav_.index() == i)) pauseAction(ctx, i);
        UiTextCentered(ctx.theme, TextFormat("seed %llu", static_cast<unsigned long long>(seed_)), {panel.x + panel.width * 0.5f, panel.y + panel.height + 18}, 16, ctx.theme.muted);
    }

    std::unique_ptr<World> world_;
    ParticleSystem particles_;
    Renderer renderer_;
    CameraRig rig_;
    Shake shake_;
    Rng fx_;
    FixedStep step_{1.f / 120.f, 8};
    FocusNav nav_;
    uint64_t seed_ = 1;
    bool paused_ = false, shakeEnabled_ = true;
    float deadTimer_ = 0.f, bannerT_ = 0.f;
    Vector2 aimWorld_{0, 0}, aimScreen_{0, 0};
};

}  // namespace

std::unique_ptr<Scene> MakeGameplayScene() { return std::make_unique<GameplayScene>(); }

}  // namespace rd
