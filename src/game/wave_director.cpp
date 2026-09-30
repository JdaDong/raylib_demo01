#include "game/wave_director.hpp"

#include <algorithm>

namespace rd {

int WaveDirector::BudgetForWave(int wave) {
    if (wave < 1) wave = 1;
    return 4 + wave * 3 + (wave * wave) / 4;
}

int WaveDirector::CostOf(EnemyKind k) {
    switch (k) {
        case EnemyKind::Chaser: return 1;
        case EnemyKind::Weaver: return 2;
        case EnemyKind::Splitter: return 3;
        case EnemyKind::Tank: return 6;
        default: return 1;
    }
}

bool WaveDirector::IsUnlocked(EnemyKind k, int wave) {
    switch (k) {
        case EnemyKind::Chaser: return true;
        case EnemyKind::Weaver: return wave >= 2;
        case EnemyKind::Splitter: return wave >= 3;
        case EnemyKind::Tank: return wave >= 4;
        default: return false;
    }
}

std::vector<EnemyKind> WaveDirector::Compose(int wave, Rng& rng) {
    int budget = BudgetForWave(wave);
    std::vector<EnemyKind> unlocked;
    for (int i = 0; i < kEnemyKindCount; ++i) {
        const auto k = static_cast<EnemyKind>(i);
        if (IsUnlocked(k, wave)) unlocked.push_back(k);
    }
    std::vector<EnemyKind> out;
    while (budget > 0) {
        // pick an affordable kind, bias towards newer (more expensive) kinds as waves grow
        std::vector<EnemyKind> affordable;
        for (auto k : unlocked)
            if (CostOf(k) <= budget) affordable.push_back(k);
        if (affordable.empty()) break;
        EnemyKind pick = affordable[static_cast<size_t>(rng.rangeInt(0, static_cast<int>(affordable.size()) - 1))];
        if (rng.chance(0.45f)) pick = EnemyKind::Chaser;  // chasers are the backbone
        if (CostOf(pick) > budget) pick = EnemyKind::Chaser;
        budget -= CostOf(pick);
        out.push_back(pick);
    }
    if (out.empty()) out.push_back(EnemyKind::Chaser);
    return out;
}

void WaveDirector::reset(float firstDelay) {
    wave_ = 0;
    queue_.clear();
    spawnTimer_ = 0.0f;
    intermission_ = firstDelay;
}

void WaveDirector::update(float dt, int aliveEnemies, Rng& rng, std::vector<EnemyKind>& outSpawns, bool& waveStarted) {
    waveStarted = false;
    if (queue_.empty() && aliveEnemies == 0) {
        intermission_ -= dt;
        if (intermission_ <= 0.0f) {
            ++wave_;
            auto kinds = Compose(wave_, rng);
            queue_.assign(kinds.begin(), kinds.end());
            spawnInterval_ = std::max(0.12f, 0.5f - 0.03f * static_cast<float>(wave_));
            spawnTimer_ = 0.0f;
            intermission_ = 2.5f;
            waveStarted = true;
        }
        return;
    }
    if (!queue_.empty()) {
        spawnTimer_ -= dt;
        while (spawnTimer_ <= 0.0f && !queue_.empty()) {
            outSpawns.push_back(queue_.front());
            queue_.pop_front();
            spawnTimer_ += spawnInterval_;
        }
    }
}

}  // namespace rd
