// Keeps audio and video locked together.
//
// Video is driven by the emulated 29.97 Hz clock and every video frame
// produces exactly its own slice of audio, so the two can only drift apart if
// the sound card's clock runs at a slightly different speed than the system
// clock (it always does, by a few hundred ppm). AvSync watches the audio
// output queue and nudges the emulation speed by at most +/-0.5% to hold the
// queue (and therefore the audio latency) at a constant target.
#pragma once

#include <cstddef>

namespace rt90 {

class AvSync {
public:
    static constexpr double kMaxAdjust = 0.005;

    explicit AvSync(int sample_rate, double target_latency_s = 0.10)
        : rate_(sample_rate), target_(target_latency_s * sample_rate) {}

    // Returns the emulation time step for `wall_dt` seconds of real time,
    // given how many samples are still queued for playback.
    double adjust(double wall_dt, size_t queued_samples);

    double target_samples() const { return target_; }
    double last_ratio() const { return ratio_; }
    // Audio latency in seconds implied by a queue level.
    double latency(size_t queued_samples) const { return static_cast<double>(queued_samples) / rate_; }

private:
    int rate_;
    double target_;
    double filtered_ = -1;
    double ratio_ = 1.0;
};

}  // namespace rt90
