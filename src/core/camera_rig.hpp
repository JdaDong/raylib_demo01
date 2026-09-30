// camera_rig.hpp - Camera2D follow / clamp / shake (pure math on Camera2D)
#pragma once
#include "core/math2d.hpp"
#include "core/rng.hpp"

#include <cmath>

namespace rd {

// "Trauma" based screen shake: offset ~ trauma^2
struct Shake {
    float trauma = 0.0f;
    float decay = 1.6f;
    float maxOffset = 14.0f;
    float maxAngle = 2.5f;  // degrees

    void add(float amount) { trauma = fminf(1.0f, trauma + amount); }
    void update(float dt) { trauma = fmaxf(0.0f, trauma - decay * dt); }
    float intensity() const { return trauma * trauma; }
    Vector2 offset(Rng& rng) const {
        const float k = intensity();
        return {rng.range(-1.f, 1.f) * maxOffset * k, rng.range(-1.f, 1.f) * maxOffset * k};
    }
    float angle(Rng& rng) const { return rng.range(-1.f, 1.f) * maxAngle * intensity(); }
};

struct CameraRig {
    Camera2D cam{{0, 0}, {0, 0}, 0.0f, 1.0f};
    float zoomTarget = 1.0f;
    float minZoom = 0.6f, maxZoom = 1.8f;

    // visible world rect for a given view size
    Rectangle visibleWorld(Vector2 viewSize) const {
        const float w = viewSize.x / cam.zoom, h = viewSize.y / cam.zoom;
        return {cam.target.x - w * 0.5f, cam.target.y - h * 0.5f, w, h};
    }

    static Vector2 ClampTarget(Vector2 target, Rectangle world, Vector2 viewSize, float zoom) {
        const float hw = viewSize.x * 0.5f / zoom, hh = viewSize.y * 0.5f / zoom;
        Vector2 t = target;
        if (world.width <= hw * 2) t.x = world.x + world.width * 0.5f;
        else t.x = Clamp(t.x, world.x + hw, world.x + world.width - hw);
        if (world.height <= hh * 2) t.y = world.y + world.height * 0.5f;
        else t.y = Clamp(t.y, world.y + hh, world.y + world.height - hh);
        return t;
    }

    void snap(Vector2 target, Rectangle world, Vector2 viewSize) {
        cam.offset = {viewSize.x * 0.5f, viewSize.y * 0.5f};
        cam.zoom = zoomTarget;
        cam.target = ClampTarget(target, world, viewSize, cam.zoom);
    }

    void update(Vector2 target, float dt, Rectangle world, Vector2 viewSize, float stiffness = 6.0f) {
        zoomTarget = Clamp(zoomTarget, minZoom, maxZoom);
        cam.zoom = ExpDecay(cam.zoom, zoomTarget, 10.0f, dt);
        cam.offset = {viewSize.x * 0.5f, viewSize.y * 0.5f};
        const Vector2 desired = ClampTarget(target, world, viewSize, cam.zoom);
        cam.target = ExpDecayV(cam.target, desired, stiffness, dt);
    }
};

}  // namespace rd
