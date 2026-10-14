// Small fast PRNG for noise generation (xorshift32).
#pragma once

#include <cstdint>

namespace rt90 {

class Rng {
public:
    explicit Rng(uint32_t seed = 0x9E3779B9u) : s_(seed ? seed : 1u) {}
    uint32_t next() {
        s_ ^= s_ << 13;
        s_ ^= s_ >> 17;
        s_ ^= s_ << 5;
        return s_;
    }
    // Uniform in [0, 1).
    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    // Uniform in [-1, 1).
    float signed_unit() { return unit() * 2.0f - 1.0f; }
    int range(int n) { return n > 0 ? static_cast<int>(next() % static_cast<uint32_t>(n)) : 0; }

private:
    uint32_t s_;
};

}  // namespace rt90
