#include "platform/input_mapper.hpp"

#include "core/math2d.hpp"

#include <cstring>

namespace rd {

namespace {
// An SDL_GameControllerDB style line (raylib/GLFW accepts the same format).
const char* kExtraMappings =
    "03000000c82d00000631000000000000,8BitDo Ultimate Wireless (raylib_demo01),"
    "a:b0,b:b1,x:b3,y:b4,back:b10,start:b11,leftshoulder:b6,rightshoulder:b7,"
    "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a5,righttrigger:a4,platform:Mac OS X,";
constexpr float kStickDeadzone = 0.18f;
}  // namespace

void InputMapper::init() {
    SetGesturesEnabled(GESTURE_TAP | GESTURE_DOUBLETAP | GESTURE_HOLD | GESTURE_DRAG | GESTURE_SWIPE_LEFT |
                       GESTURE_SWIPE_RIGHT | GESTURE_SWIPE_UP | GESTURE_SWIPE_DOWN | GESTURE_PINCH_IN | GESTURE_PINCH_OUT);
    const int r = SetGamepadMappings(kExtraMappings);
    TraceLog(LOG_INFO, "INPUT: SetGamepadMappings -> %d", r);
}

int InputMapper::activeGamepad() const {
    for (int i = 0; i < 4; ++i)
        if (IsGamepadAvailable(i)) return i;
    return -1;
}

InputFrame InputMapper::poll(const Camera2D& cam, Vector2 playerWorldPos, int virtualW, int virtualH) {
    InputFrame in;
    // --- keyboard -------------------------------------------------------------
    Vector2 move{0, 0};
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) move.y -= 1;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) move.y += 1;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) move.x -= 1;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) move.x += 1;
    in.fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsKeyDown(KEY_J);
    in.dash = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT_SHIFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    // --- mouse aim ----------------------------------------------------------------
    Vector2 aimScreen = GetMousePosition();  // already in virtual coords (SetMouseOffset/Scale)
    if (relativeAim) {
        relativeAimOffset = Vector2Add(relativeAimOffset, Vector2Scale(GetMouseDelta(), 0.6f));
        relativeAimOffset = Vector2ClampValue(relativeAimOffset, 40.f, 320.f);
        aimScreen = Vector2Add(GetWorldToScreen2D(playerWorldPos, cam), relativeAimOffset);
    }
    in.aimWorld = GetScreenToWorld2D(aimScreen, cam);
    in.hasAim = true;

    // --- gamepad ----------------------------------------------------------------------
    const int gp = activeGamepad();
    if (gp >= 0) {
        const Vector2 ls = ApplyDeadzone({GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_X), GetGamepadAxisMovement(gp, GAMEPAD_AXIS_LEFT_Y)}, kStickDeadzone);
        move = Vector2Add(move, ls);
        const Vector2 rs = ApplyDeadzone({GetGamepadAxisMovement(gp, GAMEPAD_AXIS_RIGHT_X), GetGamepadAxisMovement(gp, GAMEPAD_AXIS_RIGHT_Y)}, kStickDeadzone);
        if (Vector2Length(rs) > 0.2f) {
            in.aimWorld = Vector2Add(playerWorldPos, Vector2Scale(Vector2Normalize(rs), 220.f));
            aimScreen = GetWorldToScreen2D(in.aimWorld, cam);
        }
        const float rt = GetGamepadAxisMovement(gp, GAMEPAD_AXIS_RIGHT_TRIGGER);
        in.fire = in.fire || rt > 0.2f || IsGamepadButtonDown(gp, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) || IsGamepadButtonDown(gp, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        in.dash = in.dash || IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    }

    // --- touch virtual joystick + gestures ----------------------------------------------
    if (touchMode) {
        touchActive = false;
        const int n = GetTouchPointCount();
        for (int i = 0; i < n; ++i) {
            const Vector2 p = GetTouchPosition(i);
            if (p.x < virtualW * 0.5f) {
                if (!touchActive && GetTouchPointId(i) >= 0) {
                    if (Vector2Length(touchOrigin) < 1.f || IsGestureDetected(GESTURE_TAP)) touchOrigin = p;
                    touchCurrent = p;
                    touchActive = true;
                }
            } else {
                in.fire = true;  // right half: hold to fire, aim at finger
                aimScreen = p;
                in.aimWorld = GetScreenToWorld2D(p, cam);
            }
        }
        if (touchActive) {
            move = Vector2Add(move, Vector2ClampValue(Vector2Scale(Vector2Subtract(touchCurrent, touchOrigin), 1.f / 60.f), 0.f, 1.f));
        } else {
            touchOrigin = {0, 0};
        }
        if (IsGestureDetected(GESTURE_DOUBLETAP) || IsGestureDetected(GESTURE_SWIPE_UP)) in.dash = true;
        (void)virtualH;
    }
    lastGesture = GetGestureDetected();
    lastAimScreen = aimScreen;
    in.move = Vector2ClampValue(move, 0.f, 1.f);
    return in;
}

bool InputMapper::menuUp() const {
    const int gp = activeGamepad();
    return IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || IsKeyPressedRepeat(KEY_UP) || GetMouseWheelMove() > 0.5f ||
           (gp >= 0 && IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_LEFT_FACE_UP));
}
bool InputMapper::menuDown() const {
    const int gp = activeGamepad();
    return IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || IsKeyPressedRepeat(KEY_DOWN) || GetMouseWheelMove() < -0.5f ||
           (gp >= 0 && IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_LEFT_FACE_DOWN));
}
bool InputMapper::menuLeft() const {
    const int gp = activeGamepad();
    return IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_A) ||
           (gp >= 0 && IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_LEFT_FACE_LEFT));
}
bool InputMapper::menuRight() const {
    const int gp = activeGamepad();
    return IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_D) ||
           (gp >= 0 && IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_LEFT_FACE_RIGHT));
}
bool InputMapper::menuAccept() const {
    const int gp = activeGamepad();
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || (gp >= 0 && IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
}
bool InputMapper::menuBack() const {
    const int gp = activeGamepad();
    return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || (gp >= 0 && IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
}
bool InputMapper::pausePressed() const {
    const int gp = activeGamepad();
    return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || (gp >= 0 && IsGamepadButtonPressed(gp, GAMEPAD_BUTTON_MIDDLE_RIGHT));
}

void InputMapper::rumble(float strength, float seconds) {
    const int gp = activeGamepad();
    if (gp >= 0) SetGamepadVibration(gp, strength, strength * 0.6f, seconds);
}

std::string InputMapper::describeGamepad() const {
    const int gp = activeGamepad();
    if (gp < 0) return "no gamepad";
    const char* name = GetGamepadName(gp);
    return TextFormat("#%d %s (%d axes, last button %d)", gp, name ? name : "?", GetGamepadAxisCount(gp), GetGamepadButtonPressed());
}

std::string InputMapper::describeBindings() const {
    // GetKeyName returns the layout-specific label for printable keys (may be NULL)
    auto k = [](int key, const char* fallback) {
        const char* n = GetKeyName(key);
        return std::string((n && *n) ? n : fallback);
    };
    return "Move " + k(KEY_W, "W") + k(KEY_A, "A") + k(KEY_S, "S") + k(KEY_D, "D") + "  Fire LMB/" + k(KEY_J, "J") +
           "  Dash SPACE/RMB  Pause ESC/" + k(KEY_P, "P");
}

}  // namespace rd
