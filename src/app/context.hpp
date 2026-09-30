// context.hpp - shared services handed to every scene
#pragma once
#include "audio/audio_system.hpp"
#include "game/config.hpp"
#include "game/highscore.hpp"
#include "game/storage.hpp"
#include "gfx/assets.hpp"
#include "gfx/postfx.hpp"
#include "platform/input_mapper.hpp"
#include "ui/widgets.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace rd {

enum class SceneId { Title, Gameplay, GameOver, Settings, Showcase, Count };

struct RunResult {
    int score = 0, wave = 0, kills = 0;
    float time = 0.f;
    uint64_t seed = 0;
};

struct AppContext {
    Storage* storage = nullptr;
    Settings* settings = nullptr;
    HighscoreTable* scores = nullptr;
    Assets* assets = nullptr;
    AudioSystem* audio = nullptr;
    PostFx* postfx = nullptr;
    InputMapper* input = nullptr;
    UiTheme theme;

    int vw = 960, vh = 540;       // virtual resolution
    float time = 0.f;             // seconds since start
    unsigned int frame = 0;
    RunResult last;

    // replay / automation
    bool deterministic = false;   // fixed dt per frame (recording or replaying)
    bool recording = false, replaying = false;
    uint64_t forcedSeed = 0;      // != 0 -> next run uses this seed
    bool autopilot = false;       // smoke test drives the game
    int smokeTitleVisits = 0;     // autopilot: Title -> Play, Showcase, Settings, Quit

    int showcasePage = 0;
    bool quitRequested = false;

    // scene switching (applied by App at end of frame, with a fade)
    void go(SceneId id) { pending = id; hasPending = true; }
    SceneId pending = SceneId::Title;
    bool hasPending = false;

    // toast notifications
    std::string toast;
    float toastTime = 0.f;
    void notify(const std::string& msg, float seconds = 2.5f) {
        toast = msg;
        toastTime = seconds;
        TraceLog(LOG_INFO, "TOAST: %s", msg.c_str());
    }

    bool saveSettings() const { return storage->writeTextAtomic("settings.ini", SerializeSettings(*settings)); }
    bool saveScores() const { return storage->writeTextAtomic("scores.sav", scores->serialize()); }
    uint64_t pickSeed() {
        if (forcedSeed) { const uint64_t s = forcedSeed; forcedSeed = 0; return s; }
        if (settings->seed) return settings->seed;
        return static_cast<uint64_t>(GetRandomValue(1, 0x7FFFFFFF));
    }
};

class Scene {
public:
    virtual ~Scene() = default;
    virtual const char* name() const = 0;
    virtual void enter(AppContext&) {}
    virtual void exit(AppContext&) {}
    virtual void update(AppContext& ctx, float dt) = 0;
    // offscreen passes (render textures) BEFORE the main target is bound;
    // raylib texture modes cannot be nested
    virtual void prerender(AppContext&) {}
    virtual void draw(AppContext& ctx) = 0;  // into the virtual render target
};

std::unique_ptr<Scene> MakeTitleScene();
std::unique_ptr<Scene> MakeGameplayScene();
std::unique_ptr<Scene> MakeGameOverScene();
std::unique_ptr<Scene> MakeSettingsScene();
std::unique_ptr<Scene> MakeShowcaseScene();

}  // namespace rd
