#include "game/wave_director.hpp"
#include "testing.hpp"

using namespace rd;

TEST(wave_director, budget_grows_monotonically) {
    int prev = 0;
    for (int w = 1; w <= 30; ++w) {
        const int b = WaveDirector::BudgetForWave(w);
        CHECK(b > prev);
        prev = b;
    }
    CHECK_EQ(WaveDirector::BudgetForWave(0), WaveDirector::BudgetForWave(1));
}

TEST(wave_director, unlock_schedule) {
    CHECK(WaveDirector::IsUnlocked(EnemyKind::Chaser, 1));
    CHECK(!WaveDirector::IsUnlocked(EnemyKind::Weaver, 1));
    CHECK(WaveDirector::IsUnlocked(EnemyKind::Weaver, 2));
    CHECK(!WaveDirector::IsUnlocked(EnemyKind::Tank, 3));
    CHECK(WaveDirector::IsUnlocked(EnemyKind::Tank, 4));
}

TEST(wave_director, compose_spends_budget_with_unlocked_kinds_only) {
    Rng r(1);
    for (int w = 1; w <= 12; ++w) {
        const auto kinds = WaveDirector::Compose(w, r);
        CHECK(!kinds.empty());
        int cost = 0;
        for (auto k : kinds) {
            CHECK(WaveDirector::IsUnlocked(k, w));
            cost += WaveDirector::CostOf(k);
        }
        CHECK(cost <= WaveDirector::BudgetForWave(w));
        CHECK(cost >= WaveDirector::BudgetForWave(w) - 0);  // chasers (cost 1) always fill the rest
    }
}

TEST(wave_director, compose_is_deterministic) {
    Rng a(9), b(9);
    CHECK(WaveDirector::Compose(6, a) == WaveDirector::Compose(6, b));
}

TEST(wave_director, pacing_intermission_then_trickle) {
    WaveDirector d;
    d.reset(1.0f);
    Rng r(3);
    std::vector<EnemyKind> out;
    bool started = false;
    d.update(0.5f, 0, r, out, started);
    CHECK(!started);
    CHECK_EQ(d.wave(), 0);
    d.update(0.6f, 0, r, out, started);
    CHECK(started);
    CHECK_EQ(d.wave(), 1);
    const int total = d.pending();
    CHECK(total > 0);
    // first spawn arrives immediately, the rest trickle in at spawnInterval
    d.update(0.0001f, 0, r, out, started);
    CHECK_EQ(out.size(), static_cast<size_t>(1));
    for (int i = 0; i < 200; ++i) d.update(0.05f, static_cast<int>(out.size()), r, out, started);
    CHECK_EQ(static_cast<int>(out.size()), total);
    CHECK_EQ(d.pending(), 0);
    // alive enemies block the next wave
    d.update(10.f, 3, r, out, started);
    CHECK(!started);
    CHECK_EQ(d.wave(), 1);
    d.update(10.f, 0, r, out, started);
    CHECK(started);
    CHECK_EQ(d.wave(), 2);
}
