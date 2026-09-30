#include "ui/widgets.hpp"

#include "core/math2d.hpp"
#include "ui/ui_logic.hpp"

namespace rd {

namespace {
int g_cursor = MOUSE_CURSOR_DEFAULT;
constexpr float kFontSize = 20.f;
}  // namespace

void UiBeginFrame() { g_cursor = MOUSE_CURSOR_DEFAULT; }
int UiRequestedCursor() { return g_cursor; }

void UiPanel(const UiTheme& t, Rectangle r, Color tint) {
    DrawTextureNPatch(t.panel, t.patch, r, {0, 0}, 0.f, tint);
}

Vector2 UiMeasure(const UiTheme& t, const char* text, float size) { return MeasureTextEx(t.font, text, size, 1.f); }

void UiText(const UiTheme& t, const char* text, Vector2 pos, float size, Color c) {
    DrawTextEx(t.font, text, pos, size, 1.f, c);
}

void UiTextCentered(const UiTheme& t, const char* text, Vector2 center, float size, Color c) {
    const Vector2 m = UiMeasure(t, text, size);
    DrawTextEx(t.font, text, {center.x - m.x * 0.5f, center.y - m.y * 0.5f}, size, 1.f, c);
}

bool UiButton(const UiTheme& t, Rectangle r, const char* label, bool focused) {
    const bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    if (hover) g_cursor = MOUSE_CURSOR_POINTING_HAND;
    const bool pressed = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const Color tint = pressed ? ColorBrightness(t.accent, -0.3f) : (hover || focused) ? t.accent : WHITE;
    UiPanel(t, r, tint);
    if (focused) DrawRectangleRoundedLinesEx(RectExpand(r, 3.f), 0.25f, 6, 2.f, Fade(t.accent, 0.8f));
    UiTextCentered(t, label, {r.x + r.width * 0.5f, r.y + r.height * 0.5f + (pressed ? 1.f : 0.f)}, kFontSize, t.text);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

bool UiSlider(const UiTheme& t, Rectangle r, const char* label, float* value, float minV, float maxV, bool focused) {
    const float old = *value;
    UiPanel(t, r, focused ? t.accent : WHITE);
    UiText(t, label, {r.x + 12, r.y + r.height * 0.5f - 10}, kFontSize, t.text);
    const Rectangle track{r.x + r.width * 0.45f, r.y + r.height * 0.5f - 4, r.width * 0.42f, 8};
    const Rectangle hit{track.x - 8, r.y, track.width + 16, r.height};
    const bool hover = CheckCollisionPointRec(GetMousePosition(), hit);
    if (hover) g_cursor = MOUSE_CURSOR_RESIZE_EW;
    if (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) *value = SliderValueFromX(track, GetMouseX(), minV, maxV, 0.01f);
    if (focused) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) *value = Clamp(*value - 0.05f * (maxV - minV), minV, maxV);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) *value = Clamp(*value + 0.05f * (maxV - minV), minV, maxV);
    }
    DrawRectangleRounded(track, 1.f, 8, Fade(BLACK, 0.5f));
    const float kx = SliderXFromValue(track, *value, minV, maxV);
    DrawRectangleRounded({track.x, track.y, kx - track.x, track.height}, 1.f, 8, t.accent);
    DrawCircleV({kx, track.y + track.height * 0.5f}, 9.f, RAYWHITE);
    DrawCircleLinesV({kx, track.y + track.height * 0.5f}, 9.f, t.accent);
    const bool unit = minV == 0.f && maxV == 1.f;
    const char* pct = unit ? TextFormat("%3d%%", static_cast<int>(*value * 100.f + 0.5f)) : TextFormat("%.2f", *value);
    UiText(t, pct, {r.x + r.width - 52, r.y + r.height * 0.5f - 10}, kFontSize, t.muted);
    return old != *value;
}

bool UiToggle(const UiTheme& t, Rectangle r, const char* label, bool* value, bool focused) {
    const bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    if (hover) g_cursor = MOUSE_CURSOR_POINTING_HAND;
    bool changed = false;
    if ((hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) || (focused && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
        *value = !*value;
        changed = true;
    }
    UiPanel(t, r, (hover || focused) ? t.accent : WHITE);
    UiText(t, label, {r.x + 12, r.y + r.height * 0.5f - 10}, kFontSize, t.text);
    const Rectangle sw{r.x + r.width - 70, r.y + r.height * 0.5f - 11, 52, 22};
    DrawRectangleRounded(sw, 1.f, 12, *value ? t.accent : Fade(GRAY, 0.6f));
    DrawCircleV({*value ? sw.x + sw.width - 11 : sw.x + 11, sw.y + 11}, 9.f, RAYWHITE);
    return changed;
}

bool UiCycle(const UiTheme& t, Rectangle r, const char* label, const char* valueText, bool focused, int* delta) {
    *delta = 0;
    const bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    if (hover) g_cursor = MOUSE_CURSOR_POINTING_HAND;
    if (hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) *delta = 1;
    if (hover && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) *delta = -1;
    if (focused && IsKeyPressed(KEY_RIGHT)) *delta = 1;
    if (focused && IsKeyPressed(KEY_LEFT)) *delta = -1;
    UiPanel(t, r, (hover || focused) ? t.accent : WHITE);
    UiText(t, label, {r.x + 12, r.y + r.height * 0.5f - 10}, kFontSize, t.text);
    const char* v = TextFormat("< %s >", valueText);
    const Vector2 m = UiMeasure(t, v, kFontSize);
    UiText(t, v, {r.x + r.width - m.x - 16, r.y + r.height * 0.5f - 10}, kFontSize, t.accent);
    return *delta != 0;
}

}  // namespace rd
