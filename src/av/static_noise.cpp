#include "av/static_noise.h"

#include <cmath>

namespace rt90 {

void SnowPass::apply(Framebuffer& fb, const FrameContext& ctx) {
    // A dark band rolls slowly up the screen, like a TV searching for sync.
    const int roll_h = fb.h / 5;
    const int roll_y = fb.h - static_cast<int>((ctx.frame * 3) % static_cast<uint64_t>(fb.h + roll_h));
    for (int y = 0; y < fb.h; ++y) {
        uint32_t* row = fb.row(y);
        const int line_bias = static_cast<int>(rng_.range(41)) - 20;  // per-line brightness wobble
        const bool in_roll = y >= roll_y && y < roll_y + roll_h;
        int prev = 128;
        for (int x = 0; x < fb.w; ++x) {
            int v = static_cast<int>(rng_.next() >> 24);
            v = (v * 5 + prev * 3) >> 3;  // horizontal smear (limited bandwidth)
            prev = v;
            v = v + line_bias;
            if (in_roll) v = v * 3 / 5;
            // Faint colour speckle.
            const int tint = static_cast<int>(rng_.next() & 15) - 8;
            row[x] = rgb_clamped(v + tint, v, v - tint);
        }
    }
}

void HissGenerator::mix(float* out, size_t n, float rms) {
    // Uniform noise has RMS 1/sqrt(3); the filters below lose ~20%.
    const float gain = rms * 1.7320508f * 1.25f;
    for (size_t i = 0; i < n; ++i) {
        const float w = rng_.signed_unit();
        lp_ += 0.6f * (w - lp_);   // gentle low-pass: tame the top end
        hp_ += 0.02f * (lp_ - hp_);  // remove rumble
        out[i] += (lp_ - hp_) * gain;
    }
}

}  // namespace rt90
