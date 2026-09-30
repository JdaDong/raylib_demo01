// widgets.hpp - immediate-mode widgets drawn with NPatch panels + TTF text
#pragma once
#include "raylib.h"

namespace rd {

struct UiTheme {
    Font font{};
    Texture2D panel{};
    NPatchInfo patch{};
    Color text = RAYWHITE;
    Color accent = {90, 215, 255, 255};
    Color muted = {140, 150, 180, 255};
    Color warn = {255, 90, 110, 255};
};

// frame-level cursor arbitration: widgets request, app applies once per frame
void UiBeginFrame();
int UiRequestedCursor();

void UiPanel(const UiTheme& t, Rectangle r, Color tint = WHITE);
void UiText(const UiTheme& t, const char* text, Vector2 pos, float size, Color c);
void UiTextCentered(const UiTheme& t, const char* text, Vector2 center, float size, Color c);
Vector2 UiMeasure(const UiTheme& t, const char* text, float size);

bool UiButton(const UiTheme& t, Rectangle r, const char* label, bool focused);
// returns true while the value changes; keyboard left/right adjust when focused
bool UiSlider(const UiTheme& t, Rectangle r, const char* label, float* value, float minV, float maxV, bool focused);
bool UiToggle(const UiTheme& t, Rectangle r, const char* label, bool* value, bool focused);
bool UiCycle(const UiTheme& t, Rectangle r, const char* label, const char* valueText, bool focused, int* delta);

}  // namespace rd
