#include "game/particles.hpp"

#include "raymath.h"

#include <cmath>

namespace rd {

void ParticleSystem::burst(Vector2 pos, const BurstParams& bp, Rng& rng) {
    for (int i = 0; i < bp.count; ++i) {
        const float a = bp.direction + rng.range(-bp.spread * 0.5f, bp.spread * 0.5f);
        const float s = rng.range(bp.speedMin, bp.speedMax);
        Particle p;
        p.pos = pos;
        p.vel = {std::cos(a) * s, std::sin(a) * s};
        p.maxLife = p.life = rng.range(bp.lifeMin, bp.lifeMax);
        p.size = bp.size * rng.range(0.7f, 1.3f);
        p.sizeEnd = bp.sizeEnd;
        p.drag = bp.drag;
        p.angle = rng.range(0.f, 360.f);
        p.spin = rng.range(-360.f, 360.f);
        p.color = bp.color;
        p.colorEnd = bp.colorEnd;
        emit(p);
    }
}

void ParticleSystem::update(float dt) {
    for (size_t i = 0; i < items_.size();) {
        Particle& p = items_[i];
        p.life -= dt;
        if (p.life <= 0.f) {
            items_.removeSwap(i);  // do not advance i: slot now holds the last element
            continue;
        }
        p.vel = Vector2Scale(p.vel, std::exp(-p.drag * dt));
        p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));
        p.angle += p.spin * dt;
        ++i;
    }
}

Color ParticleSystem::CurrentColor(const Particle& p) {
    const float t = p.maxLife > 0.f ? 1.f - p.life / p.maxLife : 1.f;
    return ColorLerp(p.color, p.colorEnd, Clamp(t, 0.f, 1.f));
}

float ParticleSystem::CurrentSize(const Particle& p) {
    const float t = p.maxLife > 0.f ? 1.f - p.life / p.maxLife : 1.f;
    return Lerp(p.size, p.sizeEnd, Clamp(t, 0.f, 1.f));
}

}  // namespace rd
