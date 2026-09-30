// input_mapper.hpp - keyboard / mouse / gamepad / touch / gestures -> InputFrame
#pragma once
#include "game/input.hpp"
#include "raylib.h"

#include <string>

namespace rd {

class InputMapper {
public:
    void init();  // gestures + gamepad mappings

    // aimScreenToWorld: camera used to convert the (virtual) mouse position
    InputFrame poll(const Camera2D& cam, Vector2 playerWorldPos, int virtualW, int virtualH);

    // menu navigation (keyboard + gamepad + wheel)
    bool menuUp() const;
    bool menuDown() const;
    bool menuLeft() const;
    bool menuRight() const;
    bool menuAccept() const;
    bool menuBack() const;
    bool pausePressed() const;

    int activeGamepad() const;
    void rumble(float strength, float seconds);
    std::string describeGamepad() const;
    std::string describeBindings() const;  // uses GetKeyName (layout aware)

    bool relativeAim = false;   // F4: DisableCursor + GetMouseDelta driven aim
    bool touchMode = false;     // F8: left half of the screen is a virtual joystick
    Vector2 relativeAimOffset{200, 0};
    Vector2 touchOrigin{0, 0}, touchCurrent{0, 0};
    bool touchActive = false;
    Vector2 lastAimScreen{0, 0};
    int lastGesture = 0;
};

}  // namespace rd
