#include "game/config.hpp"

#include "raylib.h"
#include "raymath.h"

#include <cstring>

namespace rd {

float DifficultyScale(int d) {
    switch (d) {
        case 0: return 0.7f;
        case 2: return 1.45f;
        default: return 1.0f;
    }
}

const char* DifficultyName(int d) {
    switch (d) {
        case 0: return "Easy";
        case 2: return "Hard";
        default: return "Normal";
    }
}

std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

bool ParseBool(const std::string& v, bool& out) {
    const std::string lower = TextToLower(v.c_str());  // raylib static buffer -> copy
    if (TextIsEqual(lower.c_str(), "true") || TextIsEqual(lower.c_str(), "1") || TextIsEqual(lower.c_str(), "yes") || TextIsEqual(lower.c_str(), "on")) {
        out = true;
        return true;
    }
    if (TextIsEqual(lower.c_str(), "false") || TextIsEqual(lower.c_str(), "0") || TextIsEqual(lower.c_str(), "no") || TextIsEqual(lower.c_str(), "off")) {
        out = false;
        return true;
    }
    return false;
}

std::string SerializeSettings(const Settings& s) {
    // TextAppend() writes into a caller-owned buffer and advances the cursor.
    char buf[2048] = {0};
    int pos = 0;
    auto line = [&](const char* l) { TextAppend(buf, l, &pos); };
    line("# raylib_demo01 settings\n");
    line(TextFormat("version = %d\n", s.version));
    line(TextFormat("master_volume = %.2f\n", s.masterVolume));
    line(TextFormat("music_volume = %.2f\n", s.musicVolume));
    line(TextFormat("sfx_volume = %.2f\n", s.sfxVolume));
    line(TextFormat("fullscreen = %s\n", s.fullscreen ? "true" : "false"));
    line(TextFormat("vsync = %s\n", s.vsync ? "true" : "false"));
    line(TextFormat("postfx = %s\n", s.postfx ? "true" : "false"));
    line(TextFormat("show_fps = %s\n", s.showFps ? "true" : "false"));
    line(TextFormat("screen_shake = %s\n", s.screenShake ? "true" : "false"));
    line(TextFormat("crosshair = %s\n", s.crosshair ? "true" : "false"));
    line(TextFormat("difficulty = %d\n", s.difficulty));
    line(TextFormat("target_fps = %d\n", s.targetFps));
    line(TextFormat("seed = %u\n", s.seed));
    line(TextFormat("player_name = %s\n", s.playerName.c_str()));
    return std::string(buf, static_cast<size_t>(pos));
}

bool ParseSettings(const char* text, Settings& out, std::vector<std::string>* warnings) {
    if (!text) return false;
    auto warn = [&](const std::string& w) { if (warnings) warnings->push_back(w); };
    int count = 0;
    char** lines = LoadTextLines(text, &count);
    if (!lines) return false;
    Settings s = out;
    for (int i = 0; i < count; ++i) {
        const std::string raw = Trim(lines[i]);
        if (raw.empty() || raw[0] == '#' || raw[0] == ';') continue;
        const int eq = TextFindIndex(raw.c_str(), "=");
        if (eq <= 0) {
            warn(TextFormat("line %d: missing '='", i + 1));
            continue;
        }
        // TextSubtext -> TextRemoveSpaces -> TextToLower each use their own static buffer
        const std::string key = TextToLower(TextRemoveSpaces(TextSubtext(raw.c_str(), 0, eq)));
        const std::string val = Trim(raw.substr(static_cast<size_t>(eq) + 1));
        bool b = false;
        auto asFloat01 = [&](float& dst) { dst = Clamp(TextToFloat(val.c_str()), 0.f, 1.f); };
        auto asBool = [&](bool& dst) {
            if (ParseBool(val, b)) dst = b;
            else warn("bad bool for " + key + ": " + val);
        };
        if (key == "version") s.version = TextToInteger(val.c_str());
        else if (key == "master_volume" || key == "volume") {
            asFloat01(s.masterVolume);
            if (key == "volume") warn("migrated legacy key 'volume' -> master_volume");
        }
        else if (key == "music_volume") asFloat01(s.musicVolume);
        else if (key == "sfx_volume") asFloat01(s.sfxVolume);
        else if (key == "fullscreen") asBool(s.fullscreen);
        else if (key == "vsync") asBool(s.vsync);
        else if (key == "postfx") asBool(s.postfx);
        else if (key == "show_fps") asBool(s.showFps);
        else if (key == "screen_shake") asBool(s.screenShake);
        else if (key == "crosshair") asBool(s.crosshair);
        else if (key == "difficulty") s.difficulty = static_cast<int>(Clamp(static_cast<float>(TextToInteger(val.c_str())), 0.f, 2.f));
        else if (key == "target_fps") {
            const int f = TextToInteger(val.c_str());
            s.targetFps = (f >= 15 && f <= 480) ? f : 60;
        }
        else if (key == "seed") s.seed = static_cast<unsigned int>(TextToInteger(val.c_str()));
        else if (key == "player_name") s.playerName = val.empty() ? std::string("PLAYER") : val.substr(0, 16);
        else warn("unknown key: " + key);
    }
    UnloadTextLines(lines, count);
    if (s.version < 2) s.version = 2;
    out = s;
    return true;
}

}  // namespace rd
