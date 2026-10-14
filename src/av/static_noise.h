// TV snow: what the set shows (and plays) when there is no signal.
#pragma once

#include <cstddef>
#include <cstdint>

#include "av/rng.h"
#include "gpu/gpu.h"

namespace rt90 {

// Animated random "snow" with the horizontal smear, line-to-line brightness
// wobble and slow rolling bar of a real untuned analog TV.
class SnowPass : public PicturePass {
public:
    const char* name() const override { return "snow"; }
    void apply(Framebuffer& fb, const FrameContext& ctx) override;

private:
    Rng rng_{0xC0FFEEu};
};

// Static hiss: band-limited white noise.
class HissGenerator {
public:
    // Default level: about -24 dBFS RMS - clearly "static", never harsh.
    static constexpr float kDefaultRms = 0.063f;

    // Adds (mixes) `n` samples of hiss with the given RMS level into `out`.
    void mix(float* out, size_t n, float rms = kDefaultRms);

private:
    Rng rng_{0xBADA55u};
    float lp_ = 0, hp_ = 0;
};

}  // namespace rt90
