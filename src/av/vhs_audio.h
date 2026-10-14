// VHS linear-track audio: light tape hiss sitting about 30 dB below the
// programme at the default noise level, plus slight wow (slow pitch drift)
// and flutter (fast pitch wobble) from the capstan.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "av/rng.h"

namespace rt90 {

class VhsAudio {
public:
    explicit VhsAudio(int sample_rate);

    // Noise intensity 0-1 (shared with the picture; 0.15 by default).
    void set_intensity(float v);
    float intensity() const { return intensity_; }

    // Hiss level relative to the programme, in dB (about -30 dB at 15%,
    // never louder than -18 dB; -inf at 0%).
    float hiss_db() const;
    // Peak pitch deviation of wow + flutter, as a fraction (0.001 = 0.1%).
    float wow_depth() const;

    // Processes tape audio (int16) into `out` (float, -1..1), replacing it.
    void process(const int16_t* in, size_t n, float* out);
    void reset();

    float programme_rms() const { return prog_rms_; }

    // Measured levels since the last reset_stats() (for tests / debug overlay).
    double measured_hiss_rms() const { return stat_n_ ? std::sqrt(stat_hiss_ / static_cast<double>(stat_n_)) : 0.0; }
    double measured_input_rms() const { return stat_n_ ? std::sqrt(stat_in_ / static_cast<double>(stat_n_)) : 0.0; }
    void reset_stats() { stat_hiss_ = stat_in_ = 0; stat_n_ = 0; }

private:
    int rate_;
    float intensity_ = 0.15f;
    Rng rng_{0xFEEDu};
    std::vector<float> delay_;
    size_t write_ = 0;
    double phase_wow_ = 0, phase_flut_ = 0;
    float prog_rms_ = 0.1f;
    float lp_ = 0;
    double stat_hiss_ = 0, stat_in_ = 0;
    uint64_t stat_n_ = 0;
};

}  // namespace rt90
