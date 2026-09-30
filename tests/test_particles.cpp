#include "game/particles.hpp"
#include "testing.hpp"

#include "raymath.h"

using namespace rd;

TEST(particles, burst_and_expire) {
    ParticleSystem ps(1000);
    Rng r(1);
    BurstParams bp;
    bp.count = 50;
    bp.lifeMin = 0.1f;
    bp.lifeMax = 0.2f;
    ps.burst({100, 100}, bp, r);
    CHECK_EQ(ps.size(), static_cast<size_t>(50));
    ps.update(0.05f);
    CHECK_EQ(ps.size(), static_cast<size_t>(50));
    for (const auto& p : ps.items()) CHECK(Vector2Distance(p.pos, {100, 100}) > 0.f);
    ps.update(0.2f);
    CHECK_EQ(ps.size(), static_cast<size_t>(0));
}

TEST(particles, capacity_limit) {
    ParticleSystem ps(10);
    Rng r(1);
    BurstParams bp;
    bp.count = 100;
    ps.burst({0, 0}, bp, r);
    CHECK_EQ(ps.size(), static_cast<size_t>(10));
}

TEST(particles, drag_slows_down) {
    ParticleSystem ps;
    Particle p{};
    p.vel = {100, 0};
    p.life = p.maxLife = 10;
    p.drag = 2;
    ps.emit(p);
    ps.update(1.0f);
    CHECK_NEAR(ps.items()[0].vel.x, 100.0 * std::exp(-2.0), 1e-2);
}

TEST(particles, color_and_size_interpolate) {
    Particle p{};
    p.maxLife = 1;
    p.life = 1;
    p.color = {255, 0, 0, 255};
    p.colorEnd = {0, 0, 255, 0};
    p.size = 10;
    p.sizeEnd = 0;
    CHECK_EQ(ParticleSystem::CurrentColor(p).r, 255);
    p.life = 0.5f;
    const Color mid = ParticleSystem::CurrentColor(p);  // raylib ColorLerp
    CHECK(mid.r > 100 && mid.r < 155);
    CHECK(mid.b > 100 && mid.b < 155);
    CHECK_NEAR(ParticleSystem::CurrentSize(p), 5.0, 1e-4);
    p.life = 0.f;
    CHECK_EQ(ParticleSystem::CurrentColor(p).a, 0);
}

TEST(particles, directional_spread) {
    ParticleSystem ps;
    Rng r(2);
    BurstParams bp;
    bp.count = 200;
    bp.direction = 0.f;  // +X
    bp.spread = 0.2f;
    bp.drag = 0;
    ps.burst({0, 0}, bp, r);
    for (const auto& p : ps.items()) CHECK(p.vel.x > 0.f);
}
