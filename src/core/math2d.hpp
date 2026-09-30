// math2d.hpp - small 2D helpers on top of raymath (header-only, CPU-only)
#pragma once
#include "raylib.h"
#include "raymath.h"

#include <cmath>

namespace rd {

inline bool NearlyEqual(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

inline Vector2 NormalizeOrZero(Vector2 v) {
    const float len = Vector2Length(v);
    return (len > 1e-6f) ? Vector2Scale(v, 1.0f / len) : Vector2{0.0f, 0.0f};
}

// Radial deadzone with rescale: |v| < dz -> 0, |v| == 1 -> 1, keeps direction.
inline Vector2 ApplyDeadzone(Vector2 v, float dz) {
    const float len = Vector2Length(v);
    if (len < dz || len <= 1e-6f) return {0.0f, 0.0f};
    float scaled = (len - dz) / (1.0f - dz);
    if (scaled > 1.0f) scaled = 1.0f;
    return Vector2Scale(Vector2Scale(v, 1.0f / len), scaled);
}

inline Vector2 AngleToDir(float radians) { return {std::cos(radians), std::sin(radians)}; }
inline float DirToAngleDeg(Vector2 d) { return std::atan2(d.y, d.x) * RAD2DEG; }

// Frame-rate independent exponential smoothing: current -> target.
inline float ExpDecay(float current, float target, float lambda, float dt) {
    return target + (current - target) * std::exp(-lambda * dt);
}
inline Vector2 ExpDecayV(Vector2 current, Vector2 target, float lambda, float dt) {
    return {ExpDecay(current.x, target.x, lambda, dt), ExpDecay(current.y, target.y, lambda, dt)};
}

inline Vector2 RectCenter(Rectangle r) { return {r.x + r.width * 0.5f, r.y + r.height * 0.5f}; }
inline Rectangle RectExpand(Rectangle r, float by) {
    return {r.x - by, r.y - by, r.width + 2 * by, r.height + 2 * by};
}
inline Vector2 ClosestPointOnRect(Vector2 p, Rectangle r) {
    return {Clamp(p.x, r.x, r.x + r.width), Clamp(p.y, r.y, r.y + r.height)};
}

// Push a circle out of a rectangle. Returns true if they overlapped.
inline bool ResolveCircleRect(Vector2& center, float radius, Rectangle r, Vector2* normalOut = nullptr) {
    const Vector2 closest = ClosestPointOnRect(center, r);
    Vector2 delta = Vector2Subtract(center, closest);
    float dist = Vector2Length(delta);
    if (dist >= radius) return false;
    Vector2 n;
    if (dist > 1e-5f) {
        n = Vector2Scale(delta, 1.0f / dist);
    } else {
        // center inside rect: push out along the smallest penetration axis
        const float left = center.x - r.x, right = r.x + r.width - center.x;
        const float top = center.y - r.y, bottom = r.y + r.height - center.y;
        const float m = fminf(fminf(left, right), fminf(top, bottom));
        if (m == left) n = {-1, 0};
        else if (m == right) n = {1, 0};
        else if (m == top) n = {0, -1};
        else n = {0, 1};
        const Vector2 edge = (n.x < 0) ? Vector2{r.x, center.y}
                           : (n.x > 0) ? Vector2{r.x + r.width, center.y}
                           : (n.y < 0) ? Vector2{center.x, r.y}
                                       : Vector2{center.x, r.y + r.height};
        center = Vector2Add(edge, Vector2Scale(n, radius));
        if (normalOut) *normalOut = n;
        return true;
    }
    center = Vector2Add(closest, Vector2Scale(n, radius));
    if (normalOut) *normalOut = n;
    return true;
}

// Aspect-preserving fit of (srcW x srcH) into (dstW x dstH), centered (letterbox/pillarbox).
inline Rectangle Letterbox(float srcW, float srcH, float dstW, float dstH) {
    const float scale = fminf(dstW / srcW, dstH / srcH);
    const float w = srcW * scale, h = srcH * scale;
    return {(dstW - w) * 0.5f, (dstH - h) * 0.5f, w, h};
}

// Saturation color matrix for the post-processing shader (row-major semantic,
// raylib Matrix fields: m0 m4 m8 m12 is the first row).
inline Matrix SaturationMatrix(float s) {
    const float wr = 0.2126f, wg = 0.7152f, wb = 0.0722f;
    const float k = 1.0f - s;
    Matrix m = MatrixIdentity();
    m.m0 = k * wr + s; m.m4 = k * wg;     m.m8 = k * wb;
    m.m1 = k * wr;     m.m5 = k * wg + s; m.m9 = k * wb;
    m.m2 = k * wr;     m.m6 = k * wg;     m.m10 = k * wb + s;
    return m;
}

}  // namespace rd
