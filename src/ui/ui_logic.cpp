#include "ui/ui_logic.hpp"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rd {

std::vector<Rectangle> VStack(Rectangle area, int count, float itemH, float gap) {
    std::vector<Rectangle> out;
    if (count <= 0) return out;
    const float total = static_cast<float>(count) * itemH + static_cast<float>(count - 1) * gap;
    float y = area.y + (area.height - total) * 0.5f;
    for (int i = 0; i < count; ++i) {
        out.push_back({area.x, y, area.width, itemH});
        y += itemH + gap;
    }
    return out;
}

std::vector<Rectangle> HGrid(Rectangle area, int cols, int rows, float gap) {
    std::vector<Rectangle> out;
    if (cols <= 0 || rows <= 0) return out;
    const float cw = (area.width - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    const float ch = (area.height - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            out.push_back({area.x + static_cast<float>(c) * (cw + gap), area.y + static_cast<float>(r) * (ch + gap), cw, ch});
    return out;
}

float SliderValueFromX(Rectangle track, float x, float minV, float maxV, float step) {
    if (track.width <= 0.f) return minV;
    const float t = Clamp((x - track.x) / track.width, 0.f, 1.f);
    float v = minV + t * (maxV - minV);
    if (step > 0.f) v = minV + std::round((v - minV) / step) * step;
    return Clamp(v, fminf(minV, maxV), fmaxf(minV, maxV));
}

float SliderXFromValue(Rectangle track, float v, float minV, float maxV) {
    if (maxV == minV) return track.x;
    return track.x + Clamp((v - minV) / (maxV - minV), 0.f, 1.f) * track.width;
}

bool TextInput::insertCodepoint(int cp) {
    if (cp < 32 || cp == 127) return false;
    if (codepoints() >= max_) return false;
    int size = 0;
    const char* utf8 = CodepointToUTF8(cp, &size);
    if (size <= 0) return false;
    text_.append(utf8, static_cast<size_t>(size));
    return true;
}

bool TextInput::backspace() {
    if (text_.empty()) return false;
    int size = 0;
    GetCodepointPrevious(text_.c_str() + text_.size(), &size);
    if (size <= 0 || static_cast<size_t>(size) > text_.size()) size = 1;
    text_.erase(text_.size() - static_cast<size_t>(size));
    return true;
}

void TextInput::set(const std::string& s) {
    text_.clear();
    const char* p = s.c_str();
    while (*p) {
        int size = 0;
        const int cp = GetCodepointNext(p, &size);
        if (size <= 0) break;
        if (!insertCodepoint(cp)) break;
        p += size;
    }
}

int TextInput::codepoints() const { return GetCodepointCount(text_.c_str()); }

std::vector<std::string> WrapText(const std::string& text, float maxWidth, const MeasureFn& measure) {
    std::vector<std::string> lines;
    std::string current, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() && measure(candidate.c_str()) > maxWidth) {
            lines.push_back(current);
            current = word;
        } else {
            current = candidate;
        }
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') {
            flushWord();
        } else if (c == '\n') {
            flushWord();
            lines.push_back(current);
            current.clear();
        } else {
            word.push_back(c);
        }
    }
    flushWord();
    if (!current.empty() || lines.empty()) lines.push_back(current);
    return lines;
}

std::string TextInsertSafe(const std::string& text, const std::string& insert, int position) {
    const size_t pos = position < 0 ? 0 : std::min(static_cast<size_t>(position), text.size());
    std::string out;
    out.reserve(text.size() + insert.size());
    out.append(text, 0, pos).append(insert).append(text, pos, std::string::npos);
    return out;
}

std::string FormatScore(long long v) {
    const bool neg = v < 0;
    unsigned long long u = neg ? static_cast<unsigned long long>(-v) : static_cast<unsigned long long>(v);
    std::string digits = std::to_string(u), out;
    int n = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (n && n % 3 == 0) out.insert(out.begin(), ',');
        out.insert(out.begin(), *it);
        ++n;
    }
    return neg ? "-" + out : out;
}

std::string FormatClock(float seconds) {
    if (seconds < 0.f) seconds = 0.f;
    const int total = static_cast<int>(seconds * 100.f);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d", total / 6000, (total / 100) % 60, total % 100);
    return buf;
}

}  // namespace rd
