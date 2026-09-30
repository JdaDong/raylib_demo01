#include "testing.hpp"
#include "ui/ui_logic.hpp"

#include <cstring>

using namespace rd;

TEST(ui_logic, vstack_centered) {
    const auto r = VStack({0, 0, 200, 300}, 3, 40, 10);
    REQUIRE(r.size() == 3u);
    CHECK_NEAR(r[0].y, (300 - 140) / 2.0, 1e-4);
    CHECK_NEAR(r[1].y - r[0].y, 50.0, 1e-4);
    CHECK(VStack({0, 0, 1, 1}, 0, 10, 1).empty());
}

TEST(ui_logic, hgrid_covers_area) {
    const auto g = HGrid({10, 10, 310, 210}, 3, 2, 5);
    REQUIRE(g.size() == 6u);
    CHECK_NEAR(g[0].width, 100.0, 1e-4);
    CHECK_NEAR(g[5].x + g[5].width, 320.0, 1e-3);
    CHECK_NEAR(g[5].y + g[5].height, 220.0, 1e-3);
}

TEST(ui_logic, slider_mapping) {
    const Rectangle t{100, 0, 200, 10};
    CHECK_NEAR(SliderValueFromX(t, 100, 0, 1), 0.0, 1e-6);
    CHECK_NEAR(SliderValueFromX(t, 300, 0, 1), 1.0, 1e-6);
    CHECK_NEAR(SliderValueFromX(t, 200, 0, 1), 0.5, 1e-6);
    CHECK_NEAR(SliderValueFromX(t, -50, 0, 1), 0.0, 1e-6);  // clamped
    CHECK_NEAR(SliderValueFromX(t, 163, 0, 10, 1), 3.0, 1e-6);  // stepped
    CHECK_NEAR(SliderXFromValue(t, 0.25f, 0, 1), 150.0, 1e-4);
    CHECK_NEAR(SliderXFromValue(t, 5, 5, 5), 100.0, 1e-4);  // degenerate range
    CHECK_NEAR(SliderValueFromX({0, 0, 0, 0}, 5, 2, 3), 2.0, 1e-6);
}

TEST(ui_logic, focus_nav_wraps) {
    FocusNav n(3);
    n.prev();
    CHECK_EQ(n.index(), 2);
    n.next();
    CHECK_EQ(n.index(), 0);
    n.set(5);  // ignored
    CHECK_EQ(n.index(), 0);
    n.set(2);
    n.setCount(2);  // shrinks -> clamps
    CHECK_EQ(n.index(), 1);
    FocusNav empty;
    empty.next();
    CHECK_EQ(empty.index(), 0);
}

TEST(ui_logic, text_input_utf8) {
    TextInput t(4);
    CHECK(t.insertCodepoint('A'));
    CHECK(t.insertCodepoint(0x4E2D));  // 中 (3 bytes)
    CHECK(t.insertCodepoint(0x1F600)); // 😀 (4 bytes)
    CHECK_EQ(t.codepoints(), 3);
    CHECK_EQ(t.text().size(), 8u);
    CHECK(t.insertCodepoint('z'));
    CHECK(!t.insertCodepoint('!'));  // max 4 codepoints
    CHECK(!t.insertCodepoint(10));   // control chars rejected
    CHECK(t.backspace());
    CHECK(t.backspace());            // removes the whole 4-byte emoji
    CHECK_STREQ(t.text(), "A中");
    CHECK(t.backspace());
    CHECK(t.backspace());
    CHECK(!t.backspace());
    t.set("héllo!");
    CHECK_STREQ(t.text(), "héll");
}

TEST(ui_logic, wrap_text) {
    auto measure = [](const char* s) { return static_cast<float>(std::strlen(s)) * 10.f; };
    const auto lines = WrapText("the quick brown fox jumps", 100, measure);
    REQUIRE(lines.size() == 3u);
    CHECK_STREQ(lines[0], "the quick");
    CHECK_STREQ(lines[1], "brown fox");
    CHECK_STREQ(lines[2], "jumps");
    const auto nl = WrapText("a\nb", 1000, measure);
    CHECK_EQ(nl.size(), 2u);
    const auto longWord = WrapText("supercalifragilistic x", 50, measure);
    CHECK_STREQ(longWord[0], "supercalifragilistic");
    CHECK_EQ(WrapText("", 100, measure).size(), 1u);
}

TEST(ui_logic, text_insert_safe_fixes_raylib_bug) {
    CHECK_STREQ(TextInsertSafe("hello", "XX", 2), "heXXllo");
    CHECK_STREQ(TextInsertSafe("world", "hello ", 0), "hello world");
    CHECK_STREQ(TextInsertSafe("abc", "!", 99), "abc!");  // clamped
    CHECK_STREQ(TextInsertSafe("abc", "!", -3), "!abc");
    CHECK_STREQ(TextInsertSafe("", "", 0), "");
}

TEST(ui_logic, number_formatting) {
    CHECK_STREQ(FormatScore(0), "0");
    CHECK_STREQ(FormatScore(999), "999");
    CHECK_STREQ(FormatScore(1000), "1,000");
    CHECK_STREQ(FormatScore(1234567), "1,234,567");
    CHECK_STREQ(FormatScore(-4200), "-4,200");
    CHECK_STREQ(FormatClock(83.456f), "01:23.45");
    CHECK_STREQ(FormatClock(-3.f), "00:00.00");
}
