#include "game/world.hpp"

#include "core/math2d.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rd {

World::World(WorldConfig cfg, uint64_t seed) : cfg_(cfg), grid_(cfg.width, cfg.height, 96.f) { reset(seed); }

void World::reset(uint64_t seed) {
    seed_ = seed;
    rng_.reseed(seed);
    player_ = Player{};
    player_.pos = {cfg_.width * 0.5f, cfg_.height * 0.5f};
    enemies_.clear();
    pendingEnemies_.clear();
    bullets_.clear();
    pickups_.clear();
    events_.clear();
    walls_.clear();
    if (cfg_.defaultWalls) buildDefaultWalls();
    director_.reset();
    state_ = WorldState::Playing;
    nextId_ = 1;
    score_ = combo_ = kills_ = 0;
    comboTimer_ = 0.f;
    elapsed_ = 0.f;
}

void World::buildDefaultWalls() {
    const float w = cfg_.width, h = cfg_.height;
    // four pillars + two horizontal bars, symmetric, leaving the center free
    walls_.push_back({w * 0.22f, h * 0.22f, 70, 70});
    walls_.push_back({w * 0.78f - 70, h * 0.22f, 70, 70});
    walls_.push_back({w * 0.22f, h * 0.78f - 70, 70, 70});
    walls_.push_back({w * 0.78f - 70, h * 0.78f - 70, 70, 70});
    walls_.push_back({w * 0.5f - 160, h * 0.12f, 320, 24});
    walls_.push_back({w * 0.5f - 160, h * 0.88f - 24, 320, 24});
}

float World::multiplier() const { return 1.0f + static_cast<float>(std::min(combo_, 40)) * 0.1f; }

std::vector<GameEvent> World::drainEvents() {
    std::vector<GameEvent> out;
    out.swap(events_);
    return out;
}

void World::emit(EventType t, Vector2 p, float v, EnemyKind k) { events_.push_back({t, p, v, k}); }

void World::step(const InputFrame& in, float dt) {
    if (state_ == WorldState::Dead || dt <= 0.f) return;
    elapsed_ += dt;

    updatePlayer(in, dt);

    if (cfg_.spawnWaves) {
        spawnScratch_.clear();
        bool waveStarted = false;
        director_.update(dt, static_cast<int>(enemies_.size()), rng_, spawnScratch_, waveStarted);
        if (waveStarted) emit(EventType::WaveStarted, player_.pos, static_cast<float>(director_.wave()));
        for (EnemyKind k : spawnScratch_) spawnEnemy(k, pickSpawnPoint());
    }

    updateEnemies(dt);
    updateBullets(dt);
    collideBulletsEnemies();
    collideEnemiesPlayer();
    updatePickups(dt);
    flushPending();
    compact();

    if (combo_ > 0) {
        comboTimer_ -= dt;
        if (comboTimer_ <= 0.f) combo_ = 0;
    }
}

// ---------------------------------------------------------------------------
void World::updatePlayer(const InputFrame& in, float dt) {
    Player& p = player_;
    p.fire.update(dt);
    p.dash.update(dt);
    if (p.invuln > 0.f) p.invuln = std::max(0.f, p.invuln - dt);

    Vector2 move = in.move;
    if (Vector2Length(move) > 1.f) move = Vector2Normalize(move);

    if (in.hasAim) {
        const Vector2 d = Vector2Subtract(in.aimWorld, p.pos);
        if (Vector2Length(d) > 1e-3f) p.aimDir = Vector2Normalize(d);
    }

    if (in.dash && p.dash.tryTrigger()) {
        p.dashTime = cfg_.dashDuration;
        p.dashDir = Vector2Length(move) > 0.1f ? Vector2Normalize(move) : p.aimDir;
        p.invuln = std::max(p.invuln, cfg_.dashDuration + 0.05f);
        emit(EventType::Dash, p.pos);
    }

    if (p.dashTime > 0.f) {
        p.vel = Vector2Scale(p.dashDir, cfg_.dashSpeed);
        p.dashTime -= dt;
    } else {
        p.vel = Vector2Add(p.vel, Vector2Scale(move, cfg_.playerAccel * dt));
        p.vel = Vector2Scale(p.vel, std::exp(-cfg_.friction * dt));
        p.vel = Vector2ClampValue(p.vel, 0.f, cfg_.playerMaxSpeed);
    }
    p.pos = Vector2Add(p.pos, Vector2Scale(p.vel, dt));

    for (const Rectangle& w : walls_) {
        Vector2 n{0, 0};
        if (ResolveCircleRect(p.pos, p.radius, w, &n)) {
            const float vn = Vector2DotProduct(p.vel, n);
            if (vn < 0.f) p.vel = Vector2Subtract(p.vel, Vector2Scale(n, vn));
        }
    }
    p.pos.x = Clamp(p.pos.x, p.radius, cfg_.width - p.radius);
    p.pos.y = Clamp(p.pos.y, p.radius, cfg_.height - p.radius);

    if (in.fire && p.fire.tryTrigger()) {
        const float spread = rng_.range(-cfg_.bulletSpread, cfg_.bulletSpread);
        const Vector2 dir = Vector2Rotate(p.aimDir, spread);
        fireBullet(Vector2Add(p.pos, Vector2Scale(p.aimDir, p.radius + 4.f)), dir, false, cfg_.bulletSpeed, cfg_.bulletDamage);
        emit(EventType::Shot, p.pos);
    }
}

void World::fireBullet(Vector2 origin, Vector2 dir, bool hostile, float speed, float damage) {
    Bullet b;
    b.pos = b.prevPos = origin;
    b.vel = Vector2Scale(dir, speed);
    b.radius = hostile ? 6.f : 4.f;
    b.life = hostile ? 3.0f : cfg_.bulletLife;
    b.damage = damage;
    b.hostile = hostile;
    bullets_.push_back(b);
}

void World::updateBullets(float dt) {
    const Rectangle arena = bounds();
    for (Bullet& b : bullets_) {
        if (!b.alive) continue;
        b.prevPos = b.pos;
        b.pos = Vector2Add(b.pos, Vector2Scale(b.vel, dt));
        b.life -= dt;
        if (b.life <= 0.f || !CheckCollisionPointRec(b.pos, arena)) {
            b.alive = false;
            continue;
        }
        for (const Rectangle& w : walls_) {
            if (CheckCollisionCircleRec(b.pos, b.radius, w)) {
                b.alive = false;
                emit(EventType::WallHit, b.pos);
                break;
            }
        }
        if (b.alive && b.hostile && player_.alive && CheckCollisionCircles(b.pos, b.radius, player_.pos, player_.radius)) {
            b.alive = false;
            damagePlayer(b.damage, b.pos);
        }
    }
}

// ---------------------------------------------------------------------------
Enemy& World::spawnEnemy(EnemyKind kind, Vector2 pos, int generation) {
    const EnemyStats st = GetEnemyStats(kind);
    Enemy e;
    e.id = nextId_++;
    e.kind = kind;
    e.pos = pos;
    e.generation = generation;
    const float genScale = std::pow(0.7f, static_cast<float>(generation));
    e.radius = st.radius * genScale;
    e.maxHp = e.hp = st.hp * cfg_.difficulty * (generation ? 0.5f : 1.f);
    e.speed = st.speed * (0.85f + 0.15f * cfg_.difficulty) * (generation ? 1.25f : 1.f);
    e.score = st.score / (generation + 1);
    e.contactDamage = st.contactDamage;
    e.gun = Cooldown(2.2f / std::max(0.5f, cfg_.difficulty));
    e.gun.remaining = rng_.range(0.5f, e.gun.duration);
    if (kind == EnemyKind::Weaver) resetWeaverPath(e, true);
    enemies_.push_back(e);
    emit(EventType::EnemySpawned, pos, 0.f, kind);
    return enemies_.back();
}

void World::resetWeaverPath(Enemy& e, bool initial) {
    auto nearPlayer = [&]() {
        const Vector2 j = Vector2Scale(rng_.insideUnitCircle(), 160.f);
        Vector2 t = Vector2Add(player_.pos, j);
        t.x = Clamp(t.x, 20.f, cfg_.width - 20.f);
        t.y = Clamp(t.y, 20.f, cfg_.height - 20.f);
        return t;
    };
    if (initial) {
        e.path[0] = e.pos;
        e.path[1] = e.pos;
        e.path[2] = nearPlayer();
        e.path[3] = nearPlayer();
    } else {
        e.path[0] = e.path[1];
        e.path[1] = e.path[2];
        e.path[2] = e.path[3];
        e.path[3] = nearPlayer();
    }
    e.pathT = 0.f;
}

bool World::hasLineOfSight(Vector2 a, Vector2 b) const {
    for (const Rectangle& w : walls_) {
        const Vector2 c[4] = {{w.x, w.y}, {w.x + w.width, w.y}, {w.x + w.width, w.y + w.height}, {w.x, w.y + w.height}};
        for (int i = 0; i < 4; ++i) {
            Vector2 hit;
            if (CheckCollisionLines(a, b, c[i], c[(i + 1) % 4], &hit)) return false;
        }
    }
    return true;
}

Vector2 World::pickSpawnPoint() {
    const float margin = 40.f;
    for (int attempt = 0; attempt < 24; ++attempt) {
        Vector2 p;
        switch (rng_.rangeInt(0, 3)) {
            case 0: p = {rng_.range(margin, cfg_.width - margin), margin}; break;
            case 1: p = {rng_.range(margin, cfg_.width - margin), cfg_.height - margin}; break;
            case 2: p = {margin, rng_.range(margin, cfg_.height - margin)}; break;
            default: p = {cfg_.width - margin, rng_.range(margin, cfg_.height - margin)}; break;
        }
        if (Vector2Distance(p, player_.pos) < 320.f) continue;
        bool blocked = false;
        for (const Rectangle& w : walls_)
            if (CheckCollisionCircleRec(p, 30.f, w)) { blocked = true; break; }
        if (!blocked) return p;
    }
    // fallback: farthest corner
    const Vector2 corners[4] = {{margin, margin}, {cfg_.width - margin, margin}, {margin, cfg_.height - margin}, {cfg_.width - margin, cfg_.height - margin}};
    Vector2 best = corners[0];
    for (const Vector2& c : corners)
        if (Vector2Distance(c, player_.pos) > Vector2Distance(best, player_.pos)) best = c;
    return best;
}

void World::updateEnemies(float dt) {
    for (Enemy& e : enemies_) {
        if (!e.alive) continue;
        e.age += dt;
        e.hitFlash = std::max(0.f, e.hitFlash - dt * 5.f);
        e.gun.update(dt);
        const Vector2 toPlayer = Vector2Subtract(player_.pos, e.pos);
        const float dist = Vector2Length(toPlayer);
        const Vector2 dir = NormalizeOrZero(toPlayer);

        switch (e.kind) {
            case EnemyKind::Chaser: {
                e.vel = ExpDecayV(e.vel, Vector2Scale(dir, e.speed), 4.f, dt);
                break;
            }
            case EnemyKind::Weaver: {
                const float segLen = std::max(40.f, Vector2Distance(e.path[1], e.path[2]));
                e.pathT += dt * e.speed / segLen;
                if (e.pathT >= 1.f) resetWeaverPath(e, false);
                const Vector2 next = GetSplinePointCatmullRom(e.path[0], e.path[1], e.path[2], e.path[3], e.pathT);
                e.vel = Vector2Scale(Vector2Subtract(next, e.pos), 1.f / dt);
                break;
            }
            case EnemyKind::Splitter: {
                const Vector2 perp = {-dir.y, dir.x};
                const Vector2 wobble = Vector2Scale(perp, std::sin(e.age * 5.f) * 0.6f);
                e.vel = ExpDecayV(e.vel, Vector2Scale(NormalizeOrZero(Vector2Add(dir, wobble)), e.speed), 3.f, dt);
                break;
            }
            case EnemyKind::Tank: {
                const float want = dist > 260.f ? 1.f : (dist < 200.f ? -0.6f : 0.f);
                e.vel = ExpDecayV(e.vel, Vector2Scale(dir, e.speed * want), 2.f, dt);
                if (player_.alive && dist < 620.f && e.gun.ready() && hasLineOfSight(e.pos, player_.pos)) {
                    e.gun.tryTrigger();
                    fireBullet(Vector2Add(e.pos, Vector2Scale(dir, e.radius + 6.f)), dir, true, cfg_.enemyBulletSpeed, cfg_.enemyBulletDamage);
                    emit(EventType::EnemyShot, e.pos, 0.f, e.kind);
                }
                break;
            }
            default: break;
        }
        e.pos = Vector2Add(e.pos, Vector2Scale(e.vel, dt));
        if (e.kind != EnemyKind::Weaver) {
            for (const Rectangle& w : walls_) ResolveCircleRect(e.pos, e.radius, w);
        }
        e.pos.x = Clamp(e.pos.x, e.radius, cfg_.width - e.radius);
        e.pos.y = Clamp(e.pos.y, e.radius, cfg_.height - e.radius);
    }

    // soft separation between enemies (grid broadphase)
    grid_.clear();
    for (size_t i = 0; i < enemies_.size(); ++i) {
        const Enemy& e = enemies_[i];
        grid_.insert(static_cast<int>(i), {e.pos.x - e.radius, e.pos.y - e.radius, e.radius * 2, e.radius * 2});
    }
    for (size_t i = 0; i < enemies_.size(); ++i) {
        Enemy& a = enemies_[i];
        grid_.query({a.pos.x - a.radius, a.pos.y - a.radius, a.radius * 2, a.radius * 2}, queryScratch_);
        for (int j : queryScratch_) {
            if (static_cast<size_t>(j) <= i) continue;
            Enemy& b = enemies_[static_cast<size_t>(j)];
            const float minD = a.radius + b.radius;
            const Vector2 d = Vector2Subtract(b.pos, a.pos);
            const float len = Vector2Length(d);
            if (len > 1e-4f && len < minD) {
                const Vector2 push = Vector2Scale(d, (minD - len) * 0.5f / len);
                a.pos = Vector2Subtract(a.pos, push);
                b.pos = Vector2Add(b.pos, push);
            }
        }
    }
}

void World::collideBulletsEnemies() {
    grid_.clear();
    for (size_t i = 0; i < enemies_.size(); ++i) {
        const Enemy& e = enemies_[i];
        if (e.alive) grid_.insert(static_cast<int>(i), {e.pos.x - e.radius, e.pos.y - e.radius, e.radius * 2, e.radius * 2});
    }
    for (Bullet& b : bullets_) {
        if (!b.alive || b.hostile) continue;
        const float minX = std::min(b.prevPos.x, b.pos.x) - b.radius, maxX = std::max(b.prevPos.x, b.pos.x) + b.radius;
        const float minY = std::min(b.prevPos.y, b.pos.y) - b.radius, maxY = std::max(b.prevPos.y, b.pos.y) + b.radius;
        grid_.query({minX, minY, maxX - minX, maxY - minY}, queryScratch_);
        for (int idx : queryScratch_) {
            Enemy& e = enemies_[static_cast<size_t>(idx)];
            if (!e.alive) continue;
            // swept test: segment prevPos->pos against inflated circle, avoids tunneling
            const bool moved = Vector2DistanceSqr(b.prevPos, b.pos) > 1e-6f;
            const bool hit = moved ? CheckCollisionCircleLine(e.pos, e.radius + b.radius, b.prevPos, b.pos)
                                   : CheckCollisionCircles(e.pos, e.radius, b.pos, b.radius);
            if (!hit) continue;
            b.alive = false;
            e.hp -= b.damage;
            e.hitFlash = 1.f;
            e.pos = Vector2Add(e.pos, Vector2Scale(NormalizeOrZero(b.vel), 6.f));
            emit(EventType::EnemyHit, b.pos, b.damage, e.kind);
            if (e.hp <= 0.f) killEnemy(e);
            break;
        }
    }
}

void World::collideEnemiesPlayer() {
    if (!player_.alive) return;
    for (Enemy& e : enemies_) {
        if (!e.alive) continue;
        if (CheckCollisionCircles(e.pos, e.radius, player_.pos, player_.radius)) {
            if (player_.dashTime > 0.f) {
                // dashing through enemies damages them instead
                e.hp -= 25.f;
                e.hitFlash = 1.f;
                emit(EventType::EnemyHit, e.pos, 25.f, e.kind);
                if (e.hp <= 0.f) killEnemy(e);
            } else {
                damagePlayer(e.contactDamage, e.pos);
            }
            const Vector2 away = NormalizeOrZero(Vector2Subtract(e.pos, player_.pos));
            e.pos = Vector2Add(e.pos, Vector2Scale(away, 18.f));
        }
    }
}

void World::damagePlayer(float dmg, Vector2 from) {
    Player& p = player_;
    if (!p.alive || p.invuln > 0.f || dmg <= 0.f) return;
    p.hp -= dmg;
    p.invuln = cfg_.invulnAfterHit;
    combo_ = 0;
    const Vector2 away = NormalizeOrZero(Vector2Subtract(p.pos, from));
    p.vel = Vector2Add(p.vel, Vector2Scale(away, 380.f));
    emit(EventType::PlayerHit, p.pos, dmg);
    if (p.hp <= 0.f) {
        p.hp = 0.f;
        p.alive = false;
        state_ = WorldState::Dead;
        emit(EventType::PlayerDied, p.pos, static_cast<float>(score_));
    }
}

void World::killEnemy(Enemy& e) {
    if (!e.alive) return;
    e.alive = false;
    ++kills_;
    ++combo_;
    comboTimer_ = cfg_.comboWindow;
    const int gained = static_cast<int>(std::lround(static_cast<float>(e.score) * multiplier()));
    score_ += gained;
    emit(EventType::EnemyKilled, e.pos, static_cast<float>(gained), e.kind);

    const int gems = 1 + e.score / 150;
    for (int i = 0; i < gems; ++i) spawnPickup(PickupKind::Gem, Vector2Add(e.pos, Vector2Scale(rng_.insideUnitCircle(), 10.f)), 25.f);
    if (rng_.chance(cfg_.healChance)) spawnPickup(PickupKind::Heal, e.pos, 20.f);

    if (e.kind == EnemyKind::Splitter && e.generation < 2) {
        for (int s = 0; s < 2; ++s) {
            const EnemyStats st = GetEnemyStats(EnemyKind::Splitter);
            Enemy c;
            c.id = nextId_++;
            c.kind = EnemyKind::Splitter;
            c.generation = e.generation + 1;
            const float gs = std::pow(0.7f, static_cast<float>(c.generation));
            c.radius = st.radius * gs;
            c.maxHp = c.hp = st.hp * cfg_.difficulty * 0.5f;
            c.speed = st.speed * 1.25f;
            c.score = st.score / (c.generation + 1);
            c.contactDamage = st.contactDamage * 0.7f;
            const Vector2 off = {s == 0 ? -e.radius : e.radius, 0.f};
            c.pos = Vector2Add(e.pos, off);
            c.vel = Vector2Scale(off, 8.f);
            pendingEnemies_.push_back(c);  // deferred: enemies_ may be iterated right now
        }
    }
}

void World::spawnPickup(PickupKind kind, Vector2 pos, float value) {
    Pickup p;
    p.kind = kind;
    p.pos = pos;
    p.vel = Vector2Scale(rng_.onUnitCircle(), rng_.range(40.f, 120.f));
    p.value = value;
    p.life = cfg_.pickupLife;
    p.radius = kind == PickupKind::Gem ? 7.f : 10.f;
    pickups_.push_back(p);
}

void World::updatePickups(float dt) {
    for (Pickup& pk : pickups_) {
        if (!pk.alive) continue;
        pk.life -= dt;
        if (pk.life <= 0.f) { pk.alive = false; continue; }
        const Vector2 to = Vector2Subtract(player_.pos, pk.pos);
        const float d = Vector2Length(to);
        if (player_.alive && d < cfg_.magnetRadius) {
            const float pull = 900.f * (1.f - d / cfg_.magnetRadius) + 200.f;
            pk.vel = Vector2Add(pk.vel, Vector2Scale(NormalizeOrZero(to), pull * dt));
        }
        pk.vel = Vector2Scale(pk.vel, std::exp(-3.f * dt));
        pk.pos = Vector2Add(pk.pos, Vector2Scale(pk.vel, dt));
        if (player_.alive && CheckCollisionCircles(pk.pos, pk.radius, player_.pos, player_.radius + 4.f)) {
            pk.alive = false;
            if (pk.kind == PickupKind::Gem) {
                score_ += static_cast<int>(pk.value);
                emit(EventType::PickupGem, pk.pos, pk.value);
            } else {
                player_.hp = std::min(player_.maxHp, player_.hp + pk.value);
                emit(EventType::PickupHeal, pk.pos, pk.value);
            }
        }
    }
}

void World::flushPending() {
    for (Enemy& e : pendingEnemies_) {
        enemies_.push_back(e);
        emit(EventType::EnemySpawned, e.pos, 0.f, e.kind);
    }
    pendingEnemies_.clear();
}

void World::compact() {
    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [](const Enemy& e) { return !e.alive; }), enemies_.end());
    bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet& b) { return !b.alive; }), bullets_.end());
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(), [](const Pickup& p) { return !p.alive; }), pickups_.end());
}

// FNV-1a over quantized state -> equal hash == equal simulation
uint64_t World::stateHash() const {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](int64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xFF);
            h *= 1099511628211ull;
        }
    };
    auto q = [](float f) { return static_cast<int64_t>(std::lround(f * 100.0f)); };
    mix(q(player_.pos.x)); mix(q(player_.pos.y)); mix(q(player_.hp));
    mix(score_); mix(kills_); mix(combo_); mix(director_.wave());
    mix(static_cast<int64_t>(enemies_.size()));
    for (const Enemy& e : enemies_) { mix(e.id); mix(q(e.pos.x)); mix(q(e.pos.y)); mix(q(e.hp)); }
    mix(static_cast<int64_t>(bullets_.size()));
    for (const Bullet& b : bullets_) { mix(q(b.pos.x)); mix(q(b.pos.y)); }
    mix(static_cast<int64_t>(pickups_.size()));
    mix(static_cast<int64_t>(rng_.state()));
    return h;
}

}  // namespace rd
