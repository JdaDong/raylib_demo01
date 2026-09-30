// GameOver: stats, UTF-8 name entry (GetCharPressed), highscore save,
// clipboard share, retry/title.
#include "app/context.hpp"
#include "core/easing.hpp"
#include "ui/ui_logic.hpp"

#include <ctime>

namespace rd {

namespace {

class GameOverScene final : public Scene {
public:
    const char* name() const override { return "GameOver"; }

    void enter(AppContext& ctx) override {
        t_ = 0.f;
        qualifies_ = ctx.scores->qualifies(ctx.last.score);
        saved_ = false;
        rank_ = -1;
        input_.set(ctx.settings->playerName);
        nav_.setCount(2);
        ShowCursor();
    }

    void update(AppContext& ctx, float dt) override {
        t_ += dt;
        if (qualifies_ && !saved_) {
            // drain the unicode char queue (supports IME / CJK input)
            for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) input_.insertCodepoint(cp);
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) input_.backspace();
            if (IsKeyPressed(KEY_ENTER) || ctx.autopilot) save(ctx);
            return;
        }
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER)) && IsKeyPressed(KEY_C)) {
            SetClipboardText(TextFormat("I scored %d (wave %d) in NEON HARVEST, seed %llu", ctx.last.score, ctx.last.wave,
                                        static_cast<unsigned long long>(ctx.last.seed)));
            ctx.notify("Copied to clipboard");
        }
        if (ctx.input->menuLeft() || ctx.input->menuUp()) nav_.prev();
        if (ctx.input->menuRight() || ctx.input->menuDown()) nav_.next();
        if (ctx.input->menuAccept()) action(ctx, nav_.index());
        if (ctx.autopilot && t_ > 0.8f) action(ctx, 1);
    }

    void save(AppContext& ctx) {
        ScoreEntry e;
        e.name = input_.text();
        e.score = ctx.last.score;
        e.wave = ctx.last.wave;
        e.timestamp = static_cast<long long>(std::time(nullptr));
        rank_ = ctx.scores->insert(e);
        ctx.settings->playerName = HighscoreTable::SanitizeName(input_.text());
        const bool ok = ctx.saveScores() && ctx.saveSettings();
        ctx.notify(ok ? TextFormat("Saved! rank #%d", rank_ + 1) : "Save failed (see log)");
        saved_ = true;
    }

    void action(AppContext& ctx, int i) {
        ctx.audio->play(Sfx::Ui);
        if (i == 0) {
            ctx.go(SceneId::Gameplay);
        } else {
            ctx.go(SceneId::Title);
        }
    }

    void draw(AppContext& ctx) override {
        const UiTheme& th = ctx.theme;
        ClearBackground(GetColor(0x0A0610FF));
        DrawCircleGradient({ctx.vw * 0.5f, ctx.vh * 0.4f}, 380.f, Fade(GetColor(0xFF3B5CFF), 0.25f), BLANK);
        const float in = ease::backOut(std::min(1.f, t_ * 2.f));
        const char* title = "GAME OVER";
        const Vector2 m = MeasureTextEx(ctx.assets->title, title, 64 * in, 3);
        DrawTextEx(ctx.assets->title, title, {ctx.vw * 0.5f - m.x * 0.5f, 60}, 64 * in, 3, th.warn);

        const Rectangle stats{ctx.vw * 0.5f - 200, 150, 400, 150};
        UiPanel(th, stats);
        UiText(th, TextFormat("Score   %s", FormatScore(ctx.last.score).c_str()), {stats.x + 24, stats.y + 18}, 26, GOLD);
        UiText(th, TextFormat("Wave    %d", ctx.last.wave), {stats.x + 24, stats.y + 54}, 20, RAYWHITE);
        UiText(th, TextFormat("Kills   %d", ctx.last.kills), {stats.x + 24, stats.y + 80}, 20, RAYWHITE);
        UiText(th, TextFormat("Time    %s", FormatClock(ctx.last.time).c_str()), {stats.x + 24, stats.y + 106}, 20, RAYWHITE);

        if (qualifies_ && !saved_) {
            UiTextCentered(th, "NEW HIGHSCORE! enter your name, ENTER to save", {ctx.vw * 0.5f, 330}, 20, th.accent);
            const Rectangle box{ctx.vw * 0.5f - 160, 350, 320, 44};
            UiPanel(th, box, th.accent);
            // scissor keeps long names inside the box
            BeginScissorMode(static_cast<int>(box.x + 6), static_cast<int>(box.y), static_cast<int>(box.width - 12), static_cast<int>(box.height));
            const std::string shown = input_.text() + ((static_cast<int>(t_ * 2) % 2) ? "_" : " ");
            UiText(th, shown.c_str(), {box.x + 14, box.y + 11}, 22, RAYWHITE);
            EndScissorMode();
            UiText(th, TextFormat("%d/%d", input_.codepoints(), input_.maxCodepoints()), {box.x + box.width + 8, box.y + 12}, 16, th.muted);
        } else {
            if (rank_ >= 0) UiTextCentered(th, TextFormat("rank #%d saved", rank_ + 1), {ctx.vw * 0.5f, 330}, 20, GOLD);
            const Rectangle b0{ctx.vw * 0.5f - 210, 380, 200, 44}, b1{ctx.vw * 0.5f + 10, 380, 200, 44};
            if (UiButton(th, b0, "RETRY", nav_.index() == 0)) action(ctx, 0);
            if (UiButton(th, b1, "TITLE", nav_.index() == 1)) action(ctx, 1);
            UiTextCentered(th, "Ctrl+C copy result", {ctx.vw * 0.5f, 450}, 16, th.muted);
        }
    }

private:
    float t_ = 0.f;
    bool qualifies_ = false, saved_ = false;
    int rank_ = -1;
    TextInput input_{12};
    FocusNav nav_;
};

}  // namespace

std::unique_ptr<Scene> MakeGameOverScene() { return std::make_unique<GameOverScene>(); }

}  // namespace rd
