// world: movement, shooting, collisions, scoring, splitting, determinism
#include "game/world.hpp"
#include "testing.hpp"

#include "raymath.h"

#include <algorithm>

using namespace rd;

namespace {
WorldConfig Quiet() {
    WorldConfig c;
    c.spawnWaves = false;
    c.defaultWalls = false;
    return c;
}
int CountEvents(const std::vector<GameEvent>& ev, EventType t) {
    return static_cast<int>(std::count_if(ev.begin(), ev.end(), [t](const GameEvent& e) { return e.type == t; }));
}
constexpr float kDt = 1.0f / 120.0f;
}  // namespace

TEST(world, player_starts_centered_and_alive) {
    World w(Quiet(), 1);
    CHECK_NEAR(w.player().pos.x, 960.0, 1e-3);
    CHECK_NEAR(w.player().pos.y, 540.0, 1e-3);
    CHECK(w.player().alive);
    CHECK(w.state() == WorldState::Playing);
    CHECK_EQ(w.score(), 0);
}

TEST(world, movement_accelerates_and_caps_speed) {
    World w(Quiet(), 1);
    InputFrame in;
    in.move = {1, 0};
    for (int i = 0; i < 240; ++i) w.step(in, kDt);
    CHECK(w.player().pos.x > 1100.f);
    CHECK(Vector2Length(w.player().vel) <= w.config().playerMaxSpeed + 1e-3f);
    // friction brings it to rest
    in.move = {0, 0};
    for (int i = 0; i < 600; ++i) w.step(in, kDt);
    CHECK(Vector2Length(w.player().vel) < 1.f);
}

TEST(world, diagonal_input_is_normalized) {
    World a(Quiet(), 1), b(Quiet(), 1);
    InputFrame d, s;
    d.move = {1, 1};     // |v| = 1.41 -> clamped to 1
    s.move = {0.70710678f, 0.70710678f};
    for (int i = 0; i < 30; ++i) { a.step(d, kDt); b.step(s, kDt); }
    CHECK_NEAR(a.player().pos.x, b.player().pos.x, 1e-3);
}

TEST(world, player_is_clamped_to_arena) {
    World w(Quiet(), 1);
    InputFrame in;
    in.move = {-1, -1};
    for (int i = 0; i < 2000; ++i) w.step(in, kDt);
    CHECK_NEAR(w.player().pos.x, w.player().radius, 1e-3);
    CHECK_NEAR(w.player().pos.y, w.player().radius, 1e-3);
}

TEST(world, walls_block_player) {
    World w(Quiet(), 1);
    w.addWall({1000, 0, 50, 1080});
    InputFrame in;
    in.move = {1, 0};
    for (int i = 0; i < 600; ++i) w.step(in, kDt);
    CHECK(w.player().pos.x <= 1000.f - w.player().radius + 0.01f);
}

TEST(world, fire_respects_cooldown) {
    World w(Quiet(), 1);
    InputFrame in;
    in.fire = true;
    in.hasAim = true;
    in.aimWorld = {2000, 540};
    int shots = 0;
    for (int i = 0; i < 120; ++i) {  // 1 second
        w.step(in, kDt);
        shots += CountEvents(w.drainEvents(), EventType::Shot);
    }
    const int expected = static_cast<int>(1.0f / w.player().fire.duration);
    CHECK(shots >= expected - 1 && shots <= expected + 1);
    CHECK(!w.bullets().empty());
    CHECK(w.bullets().front().vel.x > 0.f);  // aimed right
}

TEST(world, bullets_expire_and_hit_walls) {
    World w(Quiet(), 1);
    w.addWall({1100, 500, 30, 80});
    InputFrame in;
    in.fire = true;
    in.hasAim = true;
    in.aimWorld = {1500, 540};
    w.step(in, kDt);
    in.fire = false;
    int wallHits = 0;
    for (int i = 0; i < 400; ++i) {
        w.step(in, kDt);
        wallHits += CountEvents(w.drainEvents(), EventType::WallHit);
    }
    CHECK(wallHits >= 1);
    CHECK(w.bullets().empty());
}

TEST(world, bullet_kills_enemy_and_scores) {
    World w(Quiet(), 1);
    w.spawnEnemy(EnemyKind::Chaser, {1060, 540});
    InputFrame in;
    in.fire = true;
    in.hasAim = true;
    in.aimWorld = {1060, 540};
    int kills = 0;
    for (int i = 0; i < 240 && kills == 0; ++i) {
        w.step(in, kDt);
        kills += CountEvents(w.drainEvents(), EventType::EnemyKilled);
    }
    CHECK_EQ(kills, 1);
    CHECK_EQ(w.kills(), 1);
    CHECK(w.score() >= GetEnemyStats(EnemyKind::Chaser).score);
    CHECK_EQ(w.combo(), 1);
    CHECK(!w.pickups().empty());  // drops at least one gem
}

TEST(world, fast_bullet_does_not_tunnel) {
    WorldConfig c = Quiet();
    c.bulletSpeed = 20000.f;  // 166 px per step at 120 Hz
    World w(c, 1);
    w.spawnEnemy(EnemyKind::Tank, {1200, 540});
    InputFrame in;
    in.fire = true;
    in.hasAim = true;
    in.aimWorld = {1500, 540};
    int hits = 0;
    for (int i = 0; i < 3; ++i) {
        w.step(in, kDt);
        hits += CountEvents(w.drainEvents(), EventType::EnemyHit);
    }
    CHECK(hits >= 1);  // swept CheckCollisionCircleLine catches it
}

TEST(world, splitter_splits_twice) {
    World w(Quiet(), 5);
    Enemy& e = w.spawnEnemy(EnemyKind::Splitter, {1100, 540});
    e.hp = 1.f;
    InputFrame in;
    in.fire = true;
    in.hasAim = true;
    int maxGen = 0, kills = 0;
    for (int i = 0; i < 1200; ++i) {
        // always aim at the closest enemy
        const auto& es = w.enemies();
        if (!es.empty()) {
            in.aimWorld = es.front().pos;
            for (const auto& x : es) maxGen = std::max(maxGen, x.generation);
        }
        w.player().hp = 100.f;  // keep the player alive
        w.step(in, kDt);
        kills += CountEvents(w.drainEvents(), EventType::EnemyKilled);
        if (w.enemies().empty() && i > 10) break;
    }
    CHECK_EQ(maxGen, 2);
    CHECK_EQ(kills, 7);  // 1 + 2 + 4
}

TEST(world, contact_damage_and_invulnerability) {
    World w(Quiet(), 1);
    w.spawnEnemy(EnemyKind::Chaser, w.player().pos);
    InputFrame in;
    w.step(in, kDt);
    const float hpAfterFirst = w.player().hp;
    CHECK(hpAfterFirst < 100.f);
    CHECK(w.player().invuln > 0.f);
    w.damagePlayer(50.f, {0, 0});  // ignored while invulnerable
    CHECK_NEAR(w.player().hp, hpAfterFirst, 1e-4);
}

TEST(world, player_death_ends_simulation) {
    World w(Quiet(), 1);
    w.damagePlayer(1000.f, {0, 0});
    CHECK(!w.player().alive);
    CHECK(w.state() == WorldState::Dead);
    CHECK_EQ(CountEvents(w.events(), EventType::PlayerDied), 1);
    const uint64_t h = w.stateHash();
    InputFrame in;
    in.move = {1, 0};
    w.step(in, kDt);
    CHECK_EQ(w.stateHash(), h);  // frozen
}

TEST(world, dash_grants_iframes_and_moves_fast) {
    World w(Quiet(), 1);
    InputFrame in;
    in.dash = true;
    in.move = {1, 0};
    const float x0 = w.player().pos.x;
    w.step(in, kDt);
    CHECK(w.player().invuln > 0.f);
    in.dash = false;
    for (int i = 0; i < 10; ++i) w.step(in, kDt);
    CHECK(w.player().pos.x - x0 > 70.f);
    in.dash = true;
    w.step(in, kDt);  // still on cooldown
    CHECK_EQ(CountEvents(w.drainEvents(), EventType::Dash), 1);
}

TEST(world, pickups_are_magnetized_and_collected) {
    World w(Quiet(), 1);
    w.spawnPickup(PickupKind::Gem, Vector2Add(w.player().pos, {90, 0}), 25);
    w.player().hp = 50;
    w.spawnPickup(PickupKind::Heal, Vector2Add(w.player().pos, {-60, 0}), 20);
    InputFrame in;
    for (int i = 0; i < 240; ++i) w.step(in, kDt);
    CHECK(w.pickups().empty());
    CHECK_EQ(w.score(), 25);
    CHECK_NEAR(w.player().hp, 70.0, 1e-3);
}

TEST(world, pickups_expire) {
    World w(Quiet(), 1);
    w.spawnPickup(PickupKind::Gem, {100, 100}, 25);
    InputFrame in;
    for (int i = 0; i < static_cast<int>(w.config().pickupLife / kDt) + 20; ++i) w.step(in, kDt);
    CHECK(w.pickups().empty());
    CHECK_EQ(w.score(), 0);
}

TEST(world, combo_multiplier_and_decay) {
    World w(Quiet(), 1);
    CHECK_NEAR(w.multiplier(), 1.0, 1e-6);
    for (int i = 0; i < 3; ++i) {
        Enemy& e = w.spawnEnemy(EnemyKind::Chaser, {1010.f + i * 0.f, 540});
        e.hp = 0.1f;
        InputFrame in;
        in.fire = true;
        in.hasAim = true;
        in.aimWorld = e.pos;
        for (int k = 0; k < 60 && w.kills() <= i; ++k) {
            w.step(in, kDt);
            w.player().fire.reset();
        }
    }
    CHECK_EQ(w.combo(), 3);
    CHECK_NEAR(w.multiplier(), 1.3, 1e-5);
    InputFrame idle;
    for (int i = 0; i < static_cast<int>(w.config().comboWindow / kDt) + 5; ++i) w.step(idle, kDt);
    CHECK_EQ(w.combo(), 0);
}

TEST(world, tank_needs_line_of_sight) {
    World w(Quiet(), 1);
    CHECK(w.hasLineOfSight({0, 0}, {100, 0}));
    w.addWall({40, -10, 20, 20});
    CHECK(!w.hasLineOfSight({0, 0}, {100, 0}));
    CHECK(w.hasLineOfSight({0, 50}, {100, 50}));

    World t(Quiet(), 1);
    t.addWall({1100, 400, 40, 280});  // between tank and player
    t.spawnEnemy(EnemyKind::Tank, {1300, 540});
    InputFrame in;
    int shots = 0;
    for (int i = 0; i < 600; ++i) {
        t.step(in, kDt);
        shots += CountEvents(t.drainEvents(), EventType::EnemyShot);
        t.player().pos = {960, 540};
        t.player().vel = {0, 0};
    }
    CHECK_EQ(shots, 0);
}

TEST(world, weaver_follows_catmull_rom_path) {
    World w(Quiet(), 3);
    Enemy& e = w.spawnEnemy(EnemyKind::Weaver, {100, 100});
    const uint32_t id = e.id;
    InputFrame in;
    float traveled = 0.f;
    Vector2 last{100, 100};
    for (int i = 0; i < 240; ++i) {
        w.step(in, kDt);
        w.player().hp = 100.f;
        for (const auto& x : w.enemies())
            if (x.id == id) {
                traveled += Vector2Distance(last, x.pos);
                last = x.pos;
            }
    }
    CHECK(traveled > 100.f);
}

TEST(world, waves_spawn_away_from_player) {
    WorldConfig c;
    World w(c, 11);
    InputFrame in;
    int waves = 0, spawns = 0;
    for (int i = 0; i < 120 * 4; ++i) {
        w.step(in, kDt);
        for (const auto& ev : w.drainEvents()) {
            if (ev.type == EventType::WaveStarted) ++waves;
            if (ev.type == EventType::EnemySpawned) {
                ++spawns;
                CHECK(Vector2Distance(ev.pos, {960, 540}) >= 300.f);
            }
        }
        w.player().hp = 100.f;
    }
    CHECK(waves >= 1);
    CHECK(spawns >= WaveDirector::BudgetForWave(1) / WaveDirector::CostOf(EnemyKind::Tank));
    CHECK_EQ(w.wave(), 1);
}

TEST(world, spawn_points_avoid_walls) {
    World w(WorldConfig{}, 21);
    for (int i = 0; i < 200; ++i) {
        const Vector2 p = w.pickSpawnPoint();
        for (const auto& r : w.walls()) CHECK(!CheckCollisionCircleRec(p, 29.f, r));
        CHECK(CheckCollisionPointRec(p, w.bounds()));
    }
}

TEST(world, same_seed_same_inputs_same_state) {
    World a(WorldConfig{}, 1234), b(WorldConfig{}, 1234), c(WorldConfig{}, 999);
    Rng inputRng(5);
    for (int i = 0; i < 120 * 10; ++i) {
        InputFrame in;
        in.move = {inputRng.range(-1, 1), inputRng.range(-1, 1)};
        in.fire = inputRng.chance(0.7f);
        in.dash = inputRng.chance(0.01f);
        in.hasAim = true;
        in.aimWorld = {inputRng.range(0, 1920), inputRng.range(0, 1080)};
        a.step(in, kDt);
        b.step(in, kDt);
        c.step(in, kDt);
    }
    CHECK_EQ(a.stateHash(), b.stateHash());
    CHECK(a.stateHash() != c.stateHash());
    CHECK_EQ(a.score(), b.score());
}

TEST(world, reset_restores_initial_state) {
    World w(WorldConfig{}, 77);
    const uint64_t h0 = w.stateHash();
    InputFrame in;
    in.move = {1, 0};
    in.fire = true;
    for (int i = 0; i < 500; ++i) w.step(in, kDt);
    CHECK(w.stateHash() != h0);
    w.reset(77);
    CHECK_EQ(w.stateHash(), h0);
}

TEST(world, stress_many_seeds_stays_finite) {
    // fuzz: 150 seeds x 8 s of chaotic input; every position must stay finite and inside the arena
    for (uint64_t seed = 1; seed <= 150; ++seed) {
        World w(WorldConfig{}, seed);
        Rng in(seed * 31);
        for (int i = 0; i < 120 * 8; ++i) {
            InputFrame f;
            f.move = {in.range(-1, 1), in.range(-1, 1)};
            f.fire = true;
            f.dash = in.chance(0.02f);
            f.hasAim = true;
            f.aimWorld = w.enemies().empty() ? Vector2{in.range(0, 1920), in.range(0, 1080)} : w.enemies().front().pos;
            w.step(f, kDt);
            if (w.state() == WorldState::Dead) break;
        }
        bool finite = std::isfinite(w.player().pos.x) && std::isfinite(w.player().pos.y);
        for (const auto& e : w.enemies()) finite = finite && std::isfinite(e.pos.x) && std::isfinite(e.pos.y) && CheckCollisionPointRec(e.pos, w.bounds());
        for (const auto& b : w.bullets()) finite = finite && std::isfinite(b.pos.x);
        CHECK(finite);
        if (!finite) std::fprintf(stderr, "    non-finite state with seed %llu\n", static_cast<unsigned long long>(seed));
    }
}

TEST(world, zero_or_negative_dt_is_noop) {
    World w(WorldConfig{}, 1);
    const uint64_t h = w.stateHash();
    InputFrame in;
    in.move = {1, 1};
    w.step(in, 0.f);
    w.step(in, -1.f);
    CHECK_EQ(w.stateHash(), h);
}
