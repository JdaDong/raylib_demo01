// ui_logic.hpp - GPU-free UI math: layout, focus navigation, sliders,
// UTF-8 aware text input, word wrap and number formatting.
#pragma once
#include "raylib.h"

#include <functional>
#include <string>
#include <vector>

namespace rd {

// `count` rows of height itemH centered in `area`
std::vector<Rectangle> VStack(Rectangle area, int count, float itemH, float gap);
std::vector<Rectangle> HGrid(Rectangle area, int cols, int rows, float gap);

float SliderValueFromX(Rectangle track, float x, float minV, float maxV, float step = 0.f);
float SliderXFromValue(Rectangle track, float v, float minV, float maxV);

class FocusNav {
public:
    explicit FocusNav(int count = 0) : count_(count) {}
    void setCount(int c) { count_ = c; if (index_ >= c) index_ = c > 0 ? c - 1 : 0; }
    void next() { if (count_ > 0) index_ = (index_ + 1) % count_; }
    void prev() { if (count_ > 0) index_ = (index_ - 1 + count_) % count_; }
    void set(int i) { if (i >= 0 && i < count_) index_ = i; }
    int index() const { return index_; }
    int count() const { return count_; }

private:
    int count_ = 0;
    int index_ = 0;
};

// UTF-8 text field; length limited in codepoints (not bytes)
class TextInput {
public:
    explicit TextInput(int maxCodepoints = 12) : max_(maxCodepoints) {}
    bool insertCodepoint(int cp);  // uses CodepointToUTF8
    bool backspace();              // uses GetCodepointPrevious
    void clear() { text_.clear(); }
    void set(const std::string& s);
    const std::string& text() const { return text_; }
    int codepoints() const;        // GetCodepointCount
    int maxCodepoints() const { return max_; }

private:
    std::string text_;
    int max_;
};

using MeasureFn = std::function<float(const char*)>;
// greedy word wrap; words longer than maxWidth are placed on their own line
std::vector<std::string> WrapText(const std::string& text, float maxWidth, const MeasureFn& measure);

// Correct replacement for raylib 6.0 TextInsert()/TextInsertAlloc() (tail-offset bug).
// `position` is a byte offset, clamped to [0, len].
std::string TextInsertSafe(const std::string& text, const std::string& insert, int position);

std::string FormatScore(long long v);       // 1234567 -> "1,234,567"
std::string FormatClock(float seconds);     // 83.456 -> "01:23.45"

}  // namespace rd
