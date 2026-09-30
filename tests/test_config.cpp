#include "game/config.hpp"
#include "testing.hpp"

using namespace rd;

TEST(config, roundtrip) {
    Settings s;
    s.masterVolume = 0.25f;
    s.musicVolume = 0.1f;
    s.fullscreen = true;
    s.vsync = false;
    s.difficulty = 2;
    s.seed = 123456;
    s.targetFps = 144;
    s.playerName = "Da Dong";
    const std::string text = SerializeSettings(s);
    Settings r;
    std::vector<std::string> warnings;
    CHECK(ParseSettings(text.c_str(), r, &warnings));
    CHECK(warnings.empty());
    CHECK_NEAR(r.masterVolume, 0.25, 1e-3);
    CHECK_NEAR(r.musicVolume, 0.1, 1e-3);
    CHECK(r.fullscreen);
    CHECK(!r.vsync);
    CHECK_EQ(r.difficulty, 2);
    CHECK_EQ(r.seed, 123456u);
    CHECK_EQ(r.targetFps, 144);
    CHECK_STREQ(r.playerName, "Da Dong");  // inner spaces survive (only keys are space-stripped)
}

TEST(config, tolerant_parsing) {
    const char* text =
        "# comment\n"
        "; another comment\n"
        "\n"
        "  MASTER_VOLUME   =   2.5  \r\n"   // clamped, CRLF, case-insensitive key
        "show fps = off\n"                  // spaces inside key are removed -> show_fps? no: 'showfps'
        "vsync = maybe\n"                   // bad bool -> warning, keep default
        "difficulty = 9\n"                  // clamped
        "target_fps = 5\n"                  // out of range -> 60
        "garbage line\n"
        "unknown_key = 1\n";
    Settings s;
    std::vector<std::string> w;
    CHECK(ParseSettings(text, s, &w));
    CHECK_NEAR(s.masterVolume, 1.0, 1e-6);
    CHECK(s.vsync);
    CHECK_EQ(s.difficulty, 2);
    CHECK_EQ(s.targetFps, 60);
    CHECK(w.size() >= 4);  // showfps(unknown), vsync, garbage, unknown_key
}

TEST(config, legacy_volume_key_is_migrated) {
    Settings s;
    std::vector<std::string> w;
    CHECK(ParseSettings("version = 1\nvolume = 0.3\n", s, &w));
    CHECK_NEAR(s.masterVolume, 0.3, 1e-4);
    CHECK_EQ(s.version, 2);
    REQUIRE(!w.empty());
    CHECK(w[0].find("migrated") != std::string::npos);
}

TEST(config, null_text_fails) {
    Settings s;
    CHECK(!ParseSettings(nullptr, s));
}

TEST(config, parse_bool_variants) {
    bool b = false;
    CHECK(ParseBool("TRUE", b) && b);
    CHECK(ParseBool("on", b) && b);
    CHECK(ParseBool("1", b) && b);
    CHECK(ParseBool("No", b) && !b);
    CHECK(ParseBool("off", b) && !b);
    CHECK(!ParseBool("", b));
    CHECK(!ParseBool("2", b));
}

TEST(config, difficulty_helpers) {
    CHECK(DifficultyScale(0) < DifficultyScale(1));
    CHECK(DifficultyScale(1) < DifficultyScale(2));
    CHECK_STREQ(DifficultyName(2), "Hard");
    CHECK_STREQ(DifficultyName(42), "Normal");
}

TEST(config, trim) {
    CHECK_STREQ(Trim("  a b \t\r"), "a b");
    CHECK_STREQ(Trim(""), "");
    CHECK_STREQ(Trim("   "), "");
}
