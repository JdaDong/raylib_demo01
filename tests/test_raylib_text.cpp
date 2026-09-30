// raylib_text: contract tests for raylib's text/UTF-8 utilities as used by
// the game (documents raylib 6.0 behaviour, static-buffer semantics etc.)
#include "testing.hpp"

#include "raylib.h"

#include <cstring>
#include <string>

TEST(raylib_text, format_and_compare) {
    CHECK_STREQ(TextFormat("%d-%s-%.1f", 3, "x", 1.25f), "3-x-1.2");
    CHECK(TextIsEqual("abc", "abc"));
    CHECK(!TextIsEqual("abc", "abd"));
    CHECK_EQ(TextLength("héllo"), 6u);  // bytes, not codepoints
    char dst[16];
    CHECK_EQ(TextCopy(dst, "copy"), 4);
    CHECK_STREQ(dst, "copy");
}

TEST(raylib_text, textformat_rotating_buffers) {
    // TextFormat() keeps a small ring of static buffers: two nested calls are safe
    const char* a = TextFormat("A%d", 1);
    const char* b = TextFormat("B%d", 2);
    CHECK_STREQ(a, "A1");
    CHECK_STREQ(b, "B2");
}

TEST(raylib_text, subtext_find_replace_insert) {
    CHECK_STREQ(TextSubtext("abcdef", 1, 3), "bcd");
    CHECK_STREQ(TextSubtext("abc", 1, 99), "bc");
    CHECK_EQ(TextFindIndex("hello", "ll"), 2);
    CHECK_EQ(TextFindIndex("hello", "zz"), -1);
    CHECK_STREQ(TextReplace("a-b-c", "-", "+"), "a+b+c");  // static buffer
    char* alloc = TextReplaceAlloc("a-b-c", "-", "::");
    CHECK_STREQ(alloc, "a::b::c");
    MemFree(alloc);
    // raylib 6.0 BUG (pinned here so an upgrade that fixes it is noticed):
    // TextInsert/TextInsertAlloc copy the tail from text + position + strlen(insert)
    // instead of text + position -> "hello" + "XX"@2 == "heXXo" (should be "heXXllo").
    // Out-of-bounds when position + strlen(insert) > strlen(text)! Use rd::TextInsertSafe().
    CHECK_STREQ(TextInsert("hello", "XX", 2), "heXXo");
    char* ins = TextInsertAlloc("hello", "XX", 2);
    CHECK_STREQ(ins, "heXXo");
    MemFree(ins);
    char* atEnd = TextInsertAlloc("abcdef", "-", 6);  // appending is unaffected
    CHECK_STREQ(atEnd, "abcdef-");
    MemFree(atEnd);
}

TEST(raylib_text, between_and_replace_between) {
    CHECK_STREQ(GetTextBetween("x<abc>y", "<", ">"), "abc");
    CHECK_STREQ(TextReplaceBetween("x<abc>y", "<", ">", "Q"), "x<Q>y");
    char* a = TextReplaceBetweenAlloc("[v1] tail", "[", "]", "v2");
    CHECK_STREQ(a, "[v2] tail");
    MemFree(a);
}

TEST(raylib_text, split_join_append) {
    int n = 0;
    char** parts = TextSplit("a|b||c", '|', &n);
    REQUIRE(n == 4);
    CHECK_STREQ(parts[0], "a");
    CHECK_STREQ(parts[2], "");  // empty fields preserved
    CHECK_STREQ(parts[3], "c");
    const char* words[] = {"x", "y", "z"};
    CHECK_STREQ(TextJoin(const_cast<char**>(words), 3, ", "), "x, y, z");
    char buf[32] = {0};
    int pos = 0;
    TextAppend(buf, "ab", &pos);
    TextAppend(buf, "cd", &pos);
    CHECK_STREQ(buf, "abcd");
    CHECK_EQ(pos, 4);
}

TEST(raylib_text, case_conversions) {
    CHECK_STREQ(TextToUpper("abC1"), "ABC1");
    CHECK_STREQ(TextToLower("ABc1"), "abc1");
    CHECK_STREQ(TextToPascal("hello_world"), "HelloWorld");
    CHECK_STREQ(TextToCamel("hello_world"), "helloWorld");
    CHECK_STREQ(TextToSnake("HelloWorld"), "hello_world");
    CHECK_STREQ(TextRemoveSpaces(" a b  c "), "abc");
}

TEST(raylib_text, number_parsing_quirks) {
    CHECK_EQ(TextToInteger("42"), 42);
    CHECK_EQ(TextToInteger("-7"), -7);
    CHECK_EQ(TextToInteger("  42"), 0);  // raylib 6.0: leading spaces NOT skipped -> trim first!
    CHECK_NEAR(TextToFloat("3.5"), 3.5, 1e-6);
    CHECK_NEAR(TextToFloat("-0.25"), -0.25, 1e-6);
}

TEST(raylib_text, lines) {
    int n = 0;
    char** lines = LoadTextLines("a\nb\n\nc", &n);
    REQUIRE(n == 4);
    CHECK_STREQ(lines[2], "");
    CHECK_STREQ(lines[3], "c");
    UnloadTextLines(lines, n);
}

TEST(raylib_text, utf8_codepoints) {
    const char* s = "a中😀";
    CHECK_EQ(GetCodepointCount(s), 3);
    int size = 0;
    CHECK_EQ(GetCodepoint(s + 1, &size), 0x4E2D);
    CHECK_EQ(size, 3);
    CHECK_EQ(GetCodepointNext(s + 4, &size), 0x1F600);
    CHECK_EQ(size, 4);
    CHECK_EQ(GetCodepointPrevious(s + std::strlen(s), &size), 0x1F600);
    CHECK_STREQ(CodepointToUTF8(0x4E2D, &size), "中");
    int count = 0;
    int* cps = LoadCodepoints(s, &count);
    REQUIRE(count == 3);
    char* back = LoadUTF8(cps, count);
    CHECK_STREQ(back, s);
    UnloadUTF8(back);
    UnloadCodepoints(cps);
}

TEST(raylib_text, random_sequence_is_permutation) {
    SetRandomSeed(1234);
    int* seq = LoadRandomSequence(10, 0, 9);
    REQUIRE(seq != nullptr);
    bool seen[10] = {false};
    for (int i = 0; i < 10; ++i) {
        CHECK(seq[i] >= 0 && seq[i] <= 9);
        seen[seq[i]] = true;
    }
    for (bool b : seen) CHECK(b);  // no duplicates
    UnloadRandomSequence(seq);
    SetRandomSeed(99);
    const int a = GetRandomValue(0, 1000);
    SetRandomSeed(99);
    CHECK_EQ(GetRandomValue(0, 1000), a);  // seeded = reproducible
}
