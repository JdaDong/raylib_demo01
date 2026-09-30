// app.hpp - window lifecycle, main loop, scene transitions, global hotkeys,
// debug overlay, drag&drop, automation (input record / replay), smoke test.
#pragma once
#include "app/context.hpp"

#include <memory>
#include <string>

namespace rd {

struct AppOptions {
    int width = 1280, height = 720;
    std::string saveDir = "saves";
    int smokeFrames = 0;  // > 0: autopilot, exit after N frames with a status code
    bool hidden = false;
    unsigned int seed = 0;
    int startScene = -1;  // SceneId
    int showcasePage = 0;
    int logLevel = 3;     // LOG_INFO
};

bool ParseArgs(int argc, char** argv, AppOptions& out, std::string& err);
const char* UsageText();

class App {
public:
    explicit App(AppOptions opts);
    int run();

private:
    bool startup();
    void shutdown();
    void frame();
    void switchScene(SceneId id);
    void handleHotkeys();
    void handleDroppedFiles();
    void handleWindowEvents();
    void updateMouseMapping();
    void drawOverlay(Rectangle dest);
    void takeScreenshot(const char* tag);
    void startRecording();
    void stopRecording();
    void startReplay();
    void stepReplay();
    void smokeStep();
    std::unique_ptr<Scene> make(SceneId id);

    AppOptions opts_;
    Storage storage_;
    Settings settings_;
    HighscoreTable scores_;
    Assets assets_;
    AudioSystem audio_;
    PostFx postfx_;
    InputMapper input_;
    AppContext ctx_;

    std::unique_ptr<Scene> scene_;
    SceneId sceneId_ = SceneId::Title;
    float fade_ = 1.f;         // 1 = black
    int fadeDir_ = -1;         // -1 fading in, +1 fading out
    SceneId fadeTarget_ = SceneId::Title;

    bool debug_ = false;
    int opacityStep_ = 0;
    int screenshotCounter_ = 0;
    int lastCursor_ = -1;
    bool minimized_ = false;

    AutomationEventList aeList_{};
    bool aeLoaded_ = false;
    unsigned int playFrame_ = 0, playIndex_ = 0;
    uint64_t replaySeed_ = 0;

    int exitCode_ = 0;
    int smokeShots_ = 0;
    SceneId smokeLastScene_ = SceneId::Count;
    int smokeSceneFrames_ = 0;
    float smokeSceneTime_ = 0.f;
};

}  // namespace rd
