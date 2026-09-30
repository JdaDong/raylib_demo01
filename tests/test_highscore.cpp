#include "game/highscore.hpp"
#include "testing.hpp"

using namespace rd;

TEST(highscore, insert_sorted_and_capped) {
    HighscoreTable t;
    for (int i = 1; i <= 15; ++i) t.insert({"P" + std::to_string(i), i * 100, i, 0});
    CHECK_EQ(static_cast<int>(t.entries().size()), HighscoreTable::kMax);
    CHECK_EQ(t.entries().front().score, 1500);
    CHECK_EQ(t.entries().back().score, 600);
    CHECK_EQ(t.best(), 1500);
    CHECK(!t.qualifies(600));  // must beat last place
    CHECK(t.qualifies(601));
    CHECK_EQ(t.insert({"low", 10, 1, 0}), -1);
    CHECK_EQ(t.insert({"top", 9999, 1, 0}), 0);
}

TEST(highscore, ties_keep_insertion_order) {
    HighscoreTable t;
    t.insert({"first", 500, 1, 0});
    const int rank = t.insert({"second", 500, 1, 0});
    CHECK_EQ(rank, 1);
    CHECK_STREQ(t.entries()[0].name, "first");
}

TEST(highscore, zero_score_never_qualifies) {
    HighscoreTable t;
    CHECK(!t.qualifies(0));
    CHECK_EQ(t.insert({"z", 0, 0, 0}), -1);
}

TEST(highscore, serialize_roundtrip) {
    HighscoreTable t;
    t.insert({"ALICE", 1200, 5, 1700000000});
    t.insert({"大东", 3400, 9, 1700000123});
    const std::string blob = t.serialize();
    CHECK(blob.rfind("NH1:", 0) == 0);
    HighscoreTable r;
    std::string err;
    CHECK(HighscoreTable::Deserialize(blob, r, &err));
    REQUIRE(r.entries().size() == 2u);
    CHECK_STREQ(r.entries()[0].name, "大东");
    CHECK_EQ(r.entries()[0].score, 3400);
    CHECK_EQ(r.entries()[0].wave, 9);
    CHECK_EQ(r.entries()[0].timestamp, 1700000123LL);
    CHECK_STREQ(r.entries()[1].name, "ALICE");
}

TEST(highscore, empty_table_roundtrip) {
    HighscoreTable t, r;
    r.insert({"x", 5, 1, 0});
    CHECK(HighscoreTable::Deserialize(t.serialize(), r));
    CHECK(r.entries().empty());
}

TEST(highscore, tamper_detection) {
    HighscoreTable t;
    t.insert({"BOB", 777, 3, 0});
    std::string blob = t.serialize();
    HighscoreTable r;
    std::string err;
    CHECK(!HighscoreTable::Deserialize("garbage", r, &err));
    CHECK_STREQ(err, "bad magic");
    std::string badCrc = blob;
    badCrc[4] = badCrc[4] == '0' ? '1' : '0';
    CHECK(!HighscoreTable::Deserialize(badCrc, r, &err));
    CHECK_STREQ(err, "crc mismatch");
    std::string badB64 = blob.substr(0, 13) + "!!!!";
    CHECK(!HighscoreTable::Deserialize(badB64, r, &err));
    CHECK_STREQ(err, "bad base64");
    CHECK(!HighscoreTable::Deserialize("NH1:1234:", r, &err));
    CHECK_STREQ(err, "bad header");
}

TEST(highscore, sanitize_name) {
    CHECK_STREQ(HighscoreTable::SanitizeName("a|b:c\nd"), "abcd");
    CHECK_STREQ(HighscoreTable::SanitizeName("   "), "PLAYER");
    CHECK_STREQ(HighscoreTable::SanitizeName(""), "PLAYER");
    CHECK_STREQ(HighscoreTable::SanitizeName("ABCDEFGHIJKLMNOP"), "ABCDEFGHIJKL");  // 12 codepoints
    CHECK_STREQ(HighscoreTable::SanitizeName("霓虹收割者霓虹收割者霓虹收割者"), "霓虹收割者霓虹收割者霓虹");
    CHECK_STREQ(HighscoreTable::SanitizeName("  pad  "), "pad");
}
