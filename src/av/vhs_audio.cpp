#include "av/vhs_audio.h"

#include <algorithm>
#include <cmath>

namespace rt90 {

namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr float kRmsFloor = 0.05f;  // ~-26 dBFS: quiet scenes still have a little hiss
constexpr int kDelay = 2048;
}  // namespace

VhsAudio::VhsAudio(int sample_rate) : rate_(sample_rate), delay_(kDelay, 0.0f) {}

void VhsAudio::set_intensity(float v) { intensity_ = std::clamp(v, 0.0f, 1.0f); }

float VhsAudio::hiss_db() const {
    if (intensity_ <= 0) return -INFINITY;
    return std::min(-18.0f, -30.0f + 20.0f * std::log10(intensity_ / 0.15f));
}

float VhsAudio::wow_depth() const { return std::min(0.004f, 0.0012f * intensity_ / 0.15f); }

void VhsAudio::reset() {
    std::fill(delay_.begin(), delay_.end(), 0.0f);
    write_ = 0;
    prog_rms_ = 0.1f;
}

void VhsAudio::process(const int16_t* in, size_t n, float* out) {
    const float depth = wow_depth();
    // Delay modulation amplitude (samples) that gives the wanted pitch deviation:
    // d(delay)/dt = A * 2*pi*f / rate  ->  A = depth * rate / (2*pi*f).
    const double wow_f = 0.55, flut_f = 6.5;
    const double a_wow = depth * 0.8 * rate_ / (kTwoPi * wow_f);
    const double a_flut = depth * 0.2 * rate_ / (kTwoPi * flut_f);
    const double base = a_wow + a_flut + 4.0;
    const float hiss_gain = intensity_ > 0 ? std::pow(10.0f, hiss_db() / 20.0f) : 0.0f;
    const size_t mask = delay_.size() - 1;

    for (size_t i = 0; i < n; ++i) {
        const float x = in[i] / 32768.0f;
        delay_[write_ & mask] = x;
        // Programme level, tracked slowly (~2 s) so the hiss stays steady.
        prog_rms_ = std::sqrt(prog_rms_ * prog_rms_ + (x * x - prog_rms_ * prog_rms_) * 0.00003f);

        phase_wow_ += kTwoPi * wow_f / rate_;
        phase_flut_ += kTwoPi * flut_f / rate_;
        if (phase_wow_ > kTwoPi) phase_wow_ -= kTwoPi;
        if (phase_flut_ > kTwoPi) phase_flut_ -= kTwoPi;
        const double d = base + a_wow * std::sin(phase_wow_) + a_flut * std::sin(phase_flut_);
        const double rp = static_cast<double>(write_) - d;
        const double fl = std::floor(rp);
        const float f = static_cast<float>(rp - fl);
        const size_t i0 = static_cast<size_t>(static_cast<int64_t>(fl)) & mask;
        const float y = delay_[i0] * (1 - f) + delay_[(i0 + 1) & mask] * f;
        ++write_;

        // Hiss: white noise, softened, relative to the programme level.
        const float wn = rng_.signed_unit() * 1.7320508f;  // unit RMS
        lp_ += 0.5f * (wn - lp_);
        const float hiss = lp_ * 1.6f * hiss_gain * std::max(prog_rms_, kRmsFloor);
        out[i] = y + hiss;
        stat_hiss_ += double(hiss) * hiss;
        stat_in_ += double(x) * x;
        ++stat_n_;
    }
}

}  // namespace rt90
