#include "game/highscore.hpp"

#include "game/hashing.hpp"
#include "raylib.h"

#include <algorithm>
#include <cstdlib>

namespace rd {

bool HighscoreTable::qualifies(int score) const {
    if (score <= 0) return false;
    if (static_cast<int>(entries_.size()) < kMax) return true;
    return score > entries_.back().score;
}

int HighscoreTable::insert(ScoreEntry e) {
    if (!qualifies(e.score)) return -1;
    e.name = SanitizeName(e.name);
    // stable: equal scores keep earlier entries first
    auto it = std::upper_bound(entries_.begin(), entries_.end(), e.score,
                               [](int s, const ScoreEntry& x) { return s > x.score; });
    const int rank = static_cast<int>(it - entries_.begin());
    entries_.insert(it, e);
    if (static_cast<int>(entries_.size()) > kMax) entries_.resize(kMax);
    return rank < kMax ? rank : -1;
}

std::string HighscoreTable::SanitizeName(const std::string& name) {
    std::string out;
    const char* p = name.c_str();
    int cps = 0;
    while (*p && cps < 12) {
        int size = 0;
        const int cp = GetCodepointNext(p, &size);
        if (size <= 0) break;
        const bool bad = cp == '|' || cp == ':' || cp < 32 || cp == 0x3F /* '?' = invalid marker */;
        if (!bad) {
            out.append(p, static_cast<size_t>(size));
            ++cps;
        }
        p += size;
    }
    // trim spaces
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out.empty() ? std::string("PLAYER") : out;
}

std::string HighscoreTable::serialize() const {
    std::string payload;
    for (const auto& e : entries_) {
        payload += TextFormat("%s|%d|%d|%lld\n", e.name.c_str(), e.score, e.wave, e.timestamp);
    }
    if (payload.empty()) return "NH1:" + Crc32Hex("", 0) + ":";
    const auto packed = Compress(payload.data(), static_cast<int>(payload.size()));
    return "NH1:" + Crc32Hex(payload.data(), static_cast<int>(payload.size())) + ":" +
           Base64Encode(packed.data(), static_cast<int>(packed.size()));
}

bool HighscoreTable::Deserialize(const std::string& blob, HighscoreTable& out, std::string* err) {
    auto fail = [&](const char* m) {
        if (err) *err = m;
        return false;
    };
    if (blob.rfind("NH1:", 0) != 0) return fail("bad magic");
    const size_t sep = blob.find(':', 4);
    if (sep == std::string::npos || sep != 12) return fail("bad header");
    const std::string crc = blob.substr(4, 8);
    std::string b64 = blob.substr(sep + 1);
    while (!b64.empty() && (b64.back() == '\n' || b64.back() == '\r' || b64.back() == ' ')) b64.pop_back();

    HighscoreTable t;
    if (b64.empty()) {  // empty table
        if (crc != Crc32Hex("", 0)) return fail("crc mismatch");
        out = t;
        return true;
    }
    std::vector<unsigned char> packed, raw;
    if (!Base64Decode(b64, packed)) return fail("bad base64");
    if (!Decompress(packed, raw)) return fail("bad deflate");
    if (Crc32Hex(raw.data(), static_cast<int>(raw.size())) != crc) return fail("crc mismatch");

    const std::string payload(raw.begin(), raw.end());
    int lineCount = 0;
    char** lines = LoadTextLines(payload.c_str(), &lineCount);
    for (int i = 0; i < lineCount; ++i) {
        if (TextLength(lines[i]) == 0) continue;
        int n = 0;
        char** parts = TextSplit(lines[i], '|', &n);  // static buffers: copy immediately
        if (n != 4) continue;
        ScoreEntry e;
        e.name = parts[0];
        e.score = TextToInteger(parts[1]);
        e.wave = TextToInteger(parts[2]);
        e.timestamp = std::atoll(parts[3]);
        t.entries_.push_back(e);
    }
    UnloadTextLines(lines, lineCount);
    std::stable_sort(t.entries_.begin(), t.entries_.end(), [](const ScoreEntry& a, const ScoreEntry& b) { return a.score > b.score; });
    if (static_cast<int>(t.entries_.size()) > kMax) t.entries_.resize(kMax);
    out = t;
    return true;
}

}  // namespace rd
