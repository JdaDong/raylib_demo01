// easing.hpp - easing curves (t in [0,1] -> [0,1] at endpoints)
#pragma once
#include <cmath>

namespace rd::ease {

inline float clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
inline float linear(float t) { return clamp01(t); }
inline float quadIn(float t) { t = clamp01(t); return t * t; }
inline float quadOut(float t) { t = clamp01(t); return t * (2.f - t); }
inline float quadInOut(float t) {
    t = clamp01(t);
    return t < 0.5f ? 2.f * t * t : -1.f + (4.f - 2.f * t) * t;
}
inline float cubicOut(float t) { t = clamp01(t) - 1.f; return t * t * t + 1.f; }
inline float backOut(float t) {
    t = clamp01(t);
    const float c1 = 1.70158f, c3 = c1 + 1.f;
    return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
}
inline float elasticOut(float t) {
    t = clamp01(t);
    if (t == 0.f || t == 1.f) return t;
    const float c4 = (2.f * 3.14159265f) / 3.f;
    return std::pow(2.f, -10.f * t) * std::sin((t * 10.f - 0.75f) * c4) + 1.f;
}
inline float bounceOut(float t) {
    t = clamp01(t);
    const float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.f / d1) return n1 * t * t;
    if (t < 2.f / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
    if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}
// 0 -> 1 -> 0 hump, used for pulses
inline float pingPong(float t) { t = clamp01(t); return 1.f - std::fabs(2.f * t - 1.f); }

}  // namespace rd::ease
