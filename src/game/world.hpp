// world.hpp - deterministic game simulation (no window, no GPU, no audio).
// Uses raylib's pure functions: CheckCollision*, GetSplinePointCatmullRom, raymath.
#pragma once
#include "core/rng.hpp"
#include "core/spatial_grid.hpp"
#include "game/input.hpp"
#include "game/types.hpp"
#include "game/wave_director.hpp"

#include <cstdint>
#include <vector>

namespace rd {

struct WorldConfig {
    float width = 1920.f, height = 1080.f;
    float playerAccel = 2600.f, playerMaxSpeed = 330.f, friction = 7.f;
    float dashSpeed = 950.f, dashDuration = 0.16f;
    float bulletSpeed = 820.f, bulletLife = 1.2f, bulletDamage = 10.f, bulletSpread = 0.035f;
    float enemyBulletSpeed = 330.f, enemyBulletDamage = 12.f;
    float magnetRadius = 130.f, pickupLife = 12.f, healChance = 0.07f;
    float comboWindow = 2.2f;
    float difficulty = 1.0f;  // scales enemy hp & speed
    float invulnAfterHit = 0.8f;
    bool spawnWaves = true;   // tests disable automatic waves
    bool defaultWalls = true;
};

enum class WorldState { Playing, Dead };

class World {
public:
    explicit World(WorldConfig cfg = {}, uint64_t seed = 1);

    void reset(uint64_t seed);
    void step(const InputFrame& in, float dt);

    // --- queries ---------------------------------------------------------
    const Player& player() const { return player_; }
    Player& player() { return player_; }
    const std::vector<Enemy>& enemies() const { return enemies_; }
    const std::vector<Bullet>& bullets() const { return bullets_; }
    const std::vector<Pickup>& pickups() const { return pickups_; }
    const std::vector<Rectangle>& walls() const { return walls_; }
    const std::vector<GameEvent>& events() const { return events_; }
    std::vector<GameEvent> drainEvents();

    int score() const { return score_; }
    int combo() const { return combo_; }
    int kills() const { return kills_; }
    int wave() const { return director_.wave(); }
    float elapsed() const { return elapsed_; }
    float comboTimer() const { return comboTimer_; }
    float multiplier() const;
    WorldState state() const { return state_; }
    uint64_t seed() const { return seed_; }
    Rectangle bounds() const { return {0, 0, cfg_.width, cfg_.height}; }
    const WorldConfig& config() const { return cfg_; }
    const WaveDirector& director() const { return director_; }
    uint64_t stateHash() const;

    // --- hooks (debug / tests) --------------------------------------------
    Enemy& spawnEnemy(EnemyKind kind, Vector2 pos, int generation = 0);
    void spawnPickup(PickupKind kind, Vector2 pos, float value);
    void addWall(Rectangle r) { walls_.push_back(r); }
    void clearWalls() { walls_.clear(); }
    bool hasLineOfSight(Vector2 a, Vector2 b) const;
    Vector2 pickSpawnPoint();
    void damagePlayer(float dmg, Vector2 from);

private:
    void buildDefaultWalls();
    void updatePlayer(const InputFrame& in, float dt);
    void fireBullet(Vector2 origin, Vector2 dir, bool hostile, float speed, float damage);
    void updateBullets(float dt);
    void updateEnemies(float dt);
    void updatePickups(float dt);
    void collideBulletsEnemies();
    void collideEnemiesPlayer();
    void killEnemy(Enemy& e);
    void resetWeaverPath(Enemy& e, bool initial);
    void flushPending();
    void compact();
    void emit(EventType t, Vector2 p, float v = 0.f, EnemyKind k = EnemyKind::Chaser);

    WorldConfig cfg_;
    Rng rng_;
    uint64_t seed_ = 1;
    Player player_;
    std::vector<Enemy> enemies_;
    std::vector<Enemy> pendingEnemies_;
    std::vector<Bullet> bullets_;
    std::vector<Pickup> pickups_;
    std::vector<Rectangle> walls_;
    std::vector<GameEvent> events_;
    std::vector<EnemyKind> spawnScratch_;
    std::vector<int> queryScratch_;
    SpatialGrid grid_;
    WaveDirector director_;
    WorldState state_ = WorldState::Playing;
    uint32_t nextId_ = 1;
    int score_ = 0, combo_ = 0, kills_ = 0;
    float comboTimer_ = 0.f;
    float elapsed_ = 0.f;
};

}  // namespace rd
