// input.hpp - device independent input snapshot consumed by World::step()
#pragma once
#include "raylib.h"

namespace rd {

struct InputFrame {
    Vector2 move{0, 0};      // desired move direction, |move| <= 1
    Vector2 aimWorld{0, 0};  // aim target in world coordinates
    bool hasAim = false;
    bool fire = false;
    bool dash = false;       // edge-triggered
};

}  // namespace rd
