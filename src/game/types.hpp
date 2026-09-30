// types.hpp - plain game entity data (no rendering state)
#pragma once
#include "core/timer.hpp"
#include "raylib.h"

#include <cstdint>

namespace rd {

enum class EnemyKind : uint8_t { Chaser = 0, Weaver, Splitter, Tank, Count };
constexpr int kEnemyKindCount = static_cast<int>(EnemyKind::Count);

struct EnemyStats {
    float radius;
    float hp;
    float speed;
    int score;
    float contactDamage;
};

inline const char* EnemyKindName(EnemyKind k) {
    switch (k) {
        case EnemyKind::Chaser: return "Chaser";
        case EnemyKind::Weaver: return "Weaver";
        case EnemyKind::Splitter: return "Splitter";
        case EnemyKind::Tank: return "Tank";
        default: return "?";
    }
}

inline EnemyStats GetEnemyStats(EnemyKind k) {
    switch (k) {
        case EnemyKind::Chaser: return {13.f, 20.f, 165.f, 100, 12.f};
        case EnemyKind::Weaver: return {12.f, 16.f, 240.f, 150, 10.f};
        case EnemyKind::Splitter: return {20.f, 40.f, 110.f, 200, 15.f};
        case EnemyKind::Tank: return {26.f, 120.f, 60.f, 500, 25.f};
        default: return {10.f, 10.f, 100.f, 10, 5.f};
    }
}

struct Player {
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    Vector2 aimDir{1, 0};
    Vector2 dashDir{0, 0};
    float radius = 14.0f;
    float hp = 100.0f;
    float maxHp = 100.0f;
    float invuln = 0.0f;
    float dashTime = 0.0f;
    Cooldown fire{0.11f};
    Cooldown dash{1.1f};
    bool alive = true;
};

struct Bullet {
    Vector2 pos{0, 0};
    Vector2 prevPos{0, 0};
    Vector2 vel{0, 0};
    float radius = 4.0f;
    float life = 1.0f;
    float damage = 10.0f;
    bool hostile = false;
    bool alive = true;
};

struct Enemy {
    uint32_t id = 0;
    EnemyKind kind = EnemyKind::Chaser;
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    float radius = 12.0f;
    float hp = 10.0f;
    float maxHp = 10.0f;
    float speed = 100.0f;
    float contactDamage = 10.0f;
    float hitFlash = 0.0f;
    float age = 0.0f;
    int generation = 0;
    int score = 100;
    Vector2 path[4]{};  // Weaver: Catmull-Rom control points, travels path[1] -> path[2]
    float pathT = 0.0f;
    Cooldown gun{2.2f};
    bool alive = true;
};

enum class PickupKind : uint8_t { Gem, Heal };

struct Pickup {
    PickupKind kind = PickupKind::Gem;
    Vector2 pos{0, 0};
    Vector2 vel{0, 0};
    float life = 10.0f;
    float value = 10.0f;
    float radius = 7.0f;
    bool alive = true;
};

enum class EventType : uint8_t {
    Shot, Dash, EnemyHit, EnemyKilled, EnemySpawned, EnemyShot,
    PlayerHit, PlayerDied, PickupGem, PickupHeal, WaveStarted, WallHit
};

struct GameEvent {
    EventType type;
    Vector2 pos;
    float value;
    EnemyKind kind;
};

}  // namespace rd
