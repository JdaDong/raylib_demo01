// highscore.hpp - top-10 table with tamper-evident, compressed save blob
//   blob = "NH1:" + crc32(payload) + ":" + base64(deflate(payload))
#pragma once
#include <string>
#include <vector>

namespace rd {

struct ScoreEntry {
    std::string name;
    int score = 0;
    int wave = 0;
    long long timestamp = 0;
};

class HighscoreTable {
public:
    static constexpr int kMax = 10;

    bool qualifies(int score) const;
    // returns rank (0-based) or -1 if it did not make the table
    int insert(ScoreEntry e);
    const std::vector<ScoreEntry>& entries() const { return entries_; }
    int best() const { return entries_.empty() ? 0 : entries_.front().score; }
    void clear() { entries_.clear(); }

    std::string serialize() const;
    static bool Deserialize(const std::string& blob, HighscoreTable& out, std::string* err = nullptr);
    // strips separators/control chars, limits to 12 UTF-8 codepoints, falls back to "PLAYER"
    static std::string SanitizeName(const std::string& name);

private:
    std::vector<ScoreEntry> entries_;
};

}  // namespace rd
