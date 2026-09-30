// timer.hpp - cooldowns and fixed-timestep accumulator
#pragma once
#include <cmath>

namespace rd {

struct Cooldown {
    float duration = 1.0f;
    float remaining = 0.0f;

    explicit Cooldown(float d = 1.0f) : duration(d) {}
    void update(float dt) { remaining = (remaining > dt) ? remaining - dt : 0.0f; }
    bool ready() const { return remaining <= 0.0f; }
    bool tryTrigger() {
        if (!ready()) return false;
        remaining = duration;
        return true;
    }
    void reset() { remaining = 0.0f; }
    // 0 right after trigger -> 1 when ready
    float progress() const { return duration <= 0.0f ? 1.0f : 1.0f - remaining / duration; }
};

// Classic "fix your timestep" accumulator with a spiral-of-death guard.
class FixedStep {
public:
    explicit FixedStep(float step = 1.0f / 120.0f, int maxSteps = 8) : step_(step), maxSteps_(maxSteps) {}

    int advance(float frameDt) {
        if (!(frameDt > 0.0f)) frameDt = 0.0f;  // also rejects NaN
        acc_ += frameDt;
        int n = 0;
        while (acc_ >= step_ && n < maxSteps_) {
            acc_ -= step_;
            ++n;
        }
        if (n == maxSteps_ && acc_ >= step_) {
            dropped_ += static_cast<int>(acc_ / step_);
            acc_ = std::fmod(acc_, step_);
        }
        return n;
    }
    float alpha() const { return acc_ / step_; }
    float step() const { return step_; }
    int dropped() const { return dropped_; }
    void reset() { acc_ = 0.0f; dropped_ = 0; }

private:
    float step_;
    int maxSteps_;
    float acc_ = 0.0f;
    int dropped_ = 0;
};

}  // namespace rd
