// config.hpp - user settings + INI (de)serialization using raylib text utilities
#pragma once
#include <string>
#include <vector>

namespace rd {

struct Settings {
    int version = 2;
    float masterVolume = 0.8f;
    float musicVolume = 0.5f;
    float sfxVolume = 0.8f;
    bool fullscreen = false;
    bool vsync = true;
    bool postfx = true;
    bool showFps = true;
    bool screenShake = true;
    bool crosshair = true;
    int difficulty = 1;  // 0 easy, 1 normal, 2 hard
    int targetFps = 60;
    unsigned int seed = 0;  // 0 -> random per run
    std::string playerName = "PLAYER";
};

float DifficultyScale(int difficulty);
const char* DifficultyName(int difficulty);

// "key = value" lines, '#' / ';' comments.
std::string SerializeSettings(const Settings& s);
// Unknown keys / bad values are reported in warnings and skipped (never fatal).
// Legacy v1 key "volume" is migrated to master_volume.
bool ParseSettings(const char* text, Settings& out, std::vector<std::string>* warnings = nullptr);
bool ParseBool(const std::string& v, bool& out);
std::string Trim(const std::string& s);

}  // namespace rd
