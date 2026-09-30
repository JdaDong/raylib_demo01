// rng.hpp - deterministic RNG (splitmix64). Game logic uses this instead of
// raylib's global GetRandomValue() so that World is reproducible per seed
// (replays, determinism tests). Cosmetic randomness may still use raylib.
#pragma once
#include "raylib.h"

#include <cmath>
#include <cstdint>

namespace rd {

class Rng {
public:
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) { reseed(seed); }

    void reseed(uint64_t seed) { state_ = seed ? seed : 0x9E3779B97F4A7C15ull; }
    uint64_t state() const { return state_; }

    uint64_t nextU64() {
        uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    uint32_t nextU32() { return static_cast<uint32_t>(nextU64() >> 32); }
    // [0, 1)
    float next01() { return static_cast<float>(nextU32() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * next01(); }
    // inclusive [lo, hi]
    int rangeInt(int lo, int hi) {
        if (hi < lo) { int t = lo; lo = hi; hi = t; }
        const uint64_t span = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo) + 1u;
        return lo + static_cast<int>(nextU64() % span);
    }
    bool chance(float p) { return next01() < p; }
    Vector2 onUnitCircle() {
        const float a = range(0.0f, 2.0f * PI);
        return {std::cos(a), std::sin(a)};
    }
    Vector2 insideUnitCircle() {
        const Vector2 d = onUnitCircle();
        const float r = std::sqrt(next01());
        return {d.x * r, d.y * r};
    }

private:
    uint64_t state_;
};

}  // namespace rd
