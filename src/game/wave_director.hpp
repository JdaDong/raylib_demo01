// wave_director.hpp - budget based wave composition and spawn pacing
#pragma once
#include "core/rng.hpp"
#include "game/types.hpp"

#include <deque>
#include <vector>

namespace rd {

class WaveDirector {
public:
    static int BudgetForWave(int wave);
    static int CostOf(EnemyKind k);
    static bool IsUnlocked(EnemyKind k, int wave);
    // Spends the wave budget on enemies; always returns >= 1 enemy.
    static std::vector<EnemyKind> Compose(int wave, Rng& rng);

    void reset(float firstDelay = 1.5f);
    // Appends kinds to spawn this tick to outSpawns; sets waveStarted on a new wave.
    void update(float dt, int aliveEnemies, Rng& rng, std::vector<EnemyKind>& outSpawns, bool& waveStarted);

    int wave() const { return wave_; }
    int pending() const { return static_cast<int>(queue_.size()); }
    float intermission() const { return intermission_; }
    float spawnInterval() const { return spawnInterval_; }

private:
    int wave_ = 0;
    std::deque<EnemyKind> queue_;
    float spawnTimer_ = 0.0f;
    float intermission_ = 1.5f;
    float spawnInterval_ = 0.45f;
};

}  // namespace rd
