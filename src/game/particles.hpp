// particles.hpp - CPU particle simulation (rendering lives in gfx/renderer)
#pragma once
#include "core/dyn_array.hpp"
#include "core/rng.hpp"
#include "raylib.h"

namespace rd {

struct Particle {
    Vector2 pos;
    Vector2 vel;
    float life;
    float maxLife;
    float size;
    float sizeEnd;
    float drag;
    float angle;
    float spin;
    Color color;
    Color colorEnd;
};

struct BurstParams {
    int count = 16;
    float speedMin = 60.f, speedMax = 240.f;
    float lifeMin = 0.3f, lifeMax = 0.8f;
    float size = 4.f, sizeEnd = 0.f;
    float drag = 3.f;
    float spread = 2.f * PI;   // radians around `direction`
    float direction = 0.f;     // radians
    Color color = WHITE;
    Color colorEnd = {255, 255, 255, 0};
};

class ParticleSystem {
public:
    explicit ParticleSystem(size_t maxParticles = 6000) : max_(maxParticles), items_(1024) {}

    void emit(const Particle& p) {
        if (items_.size() < max_) items_.push_back(p);
    }
    void burst(Vector2 pos, const BurstParams& bp, Rng& rng);
    void update(float dt);
    void clear() { items_.clear(); }

    size_t size() const { return items_.size(); }
    size_t maxParticles() const { return max_; }
    const DynArray<Particle>& items() const { return items_; }

    // life fraction 1 -> 0; interpolated with raylib ColorLerp
    static Color CurrentColor(const Particle& p);
    static float CurrentSize(const Particle& p);

private:
    size_t max_;
    DynArray<Particle> items_;
};

}  // namespace rd
