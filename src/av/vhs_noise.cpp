#include "av/vhs_noise.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rt90 {

namespace {
constexpr float kPi = 3.14159265358979f;
}

void VhsNoisePass::set_intensity(float v) { intensity_ = std::clamp(v, 0.0f, 1.0f); }

float VhsNoisePass::effective() const {
    if (!tracking_active()) return intensity_;
    // 0-0.4 s: the heads hunt for the track (a little worse), then clean
    // for a few seconds, then ease back over the last 2 s.
    const int t = tracking_age_;
    if (t < 12) return std::min(1.0f, intensity_ * 1.4f + 0.05f);
    const float clean = 0.15f;
    const int fade_start = kTrackingFrames - 60;
    if (t < fade_start) return intensity_ * clean;
    const float k = static_cast<float>(t - fade_start) / 60.0f;
    return intensity_ * (clean + (1.0f - clean) * k);
}

void VhsNoisePass::update_band() {
    const float e = effective();
    if (band_len_ > 0) {
        ++band_age_;
        const float phase = static_cast<float>(band_age_) / static_cast<float>(band_len_);
        band_strength_ = std::sin(phase * kPi);  // fade in and out
        band_y_ -= 0.0015f;                      // rolls slowly upwards
        if (band_y_ < 0.72f) band_y_ = 0.97f;
        if (band_age_ >= band_len_) {
            band_len_ = 0;
            band_strength_ = 0;
            // Idle 4-12 s at low noise, much less when the tape is bad.
            const float idle = (120.0f + static_cast<float>(rng_.range(240))) * (1.2f - e);
            band_timer_ = std::max(15, static_cast<int>(idle));
        }
    } else if (--band_timer_ <= 0 && e > 0.01f) {
        band_len_ = 45 + rng_.range(75);  // 1.5-4 s episode
        band_age_ = 0;
        band_y_ = 0.80f + rng_.unit() * 0.15f;
    }
}

void VhsNoisePass::process_row(const uint32_t* src, uint32_t* dst, int w, float shift, float soften,
                               float chroma_r, float chroma_delay, float grain) {
    y_.resize(static_cast<size_t>(w));
    i_.resize(static_cast<size_t>(w));
    q_.resize(static_cast<size_t>(w));
    tmp_.resize(static_cast<size_t>(w));
    // Fetch with a sub-pixel horizontal shift (time-base error).
    for (int x = 0; x < w; ++x) {
        const float sx = static_cast<float>(x) - shift;
        int x0 = static_cast<int>(std::floor(sx));
        const float f = sx - static_cast<float>(x0);
        const int xa = std::clamp(x0, 0, w - 1), xb = std::clamp(x0 + 1, 0, w - 1);
        const uint32_t a = src[xa], b = src[xb];
        const float r = static_cast<float>(red(a)) * (1 - f) + static_cast<float>(red(b)) * f;
        const float g = static_cast<float>(green(a)) * (1 - f) + static_cast<float>(green(b)) * f;
        const float bl = static_cast<float>(blue(a)) * (1 - f) + static_cast<float>(blue(b)) * f;
        y_[static_cast<size_t>(x)] = 0.299f * r + 0.587f * g + 0.114f * bl;
        i_[static_cast<size_t>(x)] = 0.596f * r - 0.274f * g - 0.322f * bl;
        q_[static_cast<size_t>(x)] = 0.211f * r - 0.523f * g + 0.312f * bl;
    }
    // Softened detail: luma low-pass.
    for (int x = 0; x < w; ++x) {
        const float l = y_[static_cast<size_t>(std::max(0, x - 1))], c = y_[static_cast<size_t>(x)];
        const float rr = y_[static_cast<size_t>(std::min(w - 1, x + 1))];
        tmp_[static_cast<size_t>(x)] = c + soften * ((l + 2 * c + rr) * 0.25f - c);
    }
    y_.swap(tmp_);
    // Chroma bleed + smear: wide box blur, delayed to the right of the luma.
    const int r = std::max(1, static_cast<int>(std::lround(chroma_r)));
    const int delay = static_cast<int>(std::lround(chroma_delay));
    for (std::vector<float>* ch : {&i_, &q_}) {
        std::vector<float>& v = *ch;
        float acc = 0;
        for (int x = -r; x <= r; ++x) acc += v[static_cast<size_t>(std::clamp(x, 0, w - 1))];
        const float norm = 1.0f / static_cast<float>(2 * r + 1);
        for (int x = 0; x < w; ++x) {
            tmp_[static_cast<size_t>(x)] = acc * norm;
            acc += v[static_cast<size_t>(std::min(w - 1, x + r + 1))] - v[static_cast<size_t>(std::max(0, x - r))];
        }
        for (int x = w - 1; x >= 0; --x) v[static_cast<size_t>(x)] = tmp_[static_cast<size_t>(std::max(0, x - delay))];
    }
    for (int x = 0; x < w; ++x) {
        float Y = y_[static_cast<size_t>(x)];
        if (grain > 0) Y += rng_.signed_unit() * grain;
        const float I = i_[static_cast<size_t>(x)] * 0.92f, Q = q_[static_cast<size_t>(x)] * 0.92f;
        dst[x] = rgb_clamped(static_cast<int>(Y + 0.956f * I + 0.621f * Q), static_cast<int>(Y - 0.272f * I - 0.647f * Q),
                             static_cast<int>(Y - 1.106f * I + 1.703f * Q));
    }
}

void VhsNoisePass::apply(Framebuffer& fb, const FrameContext& ctx) {
    (void)ctx;
    if (tracking_age_ < (1 << 20)) ++tracking_age_;
    update_band();
    const float e = effective();
    const int w = fb.w, h = fb.h;
    src_row_.resize(static_cast<size_t>(w));

    const float soften = std::min(1.0f, 0.45f + e * 0.8f);
    const float chroma_r = 1.5f + e * 5.0f;
    const float chroma_delay = 1.0f + e * 2.5f;
    const float grain = e * 14.0f;
    const float jitter = e * 1.6f;
    const int band_c = static_cast<int>(band_y_ * static_cast<float>(h));
    const int band_half = static_cast<int>(3 + e * 8);
    const int hs_lines = 3 + static_cast<int>(e * 5);  // head-switching region
    const float line_phase = rng_.unit() * 100.0f;

    for (int y = 0; y < h; ++y) {
        uint32_t* row = fb.row(y);
        std::memcpy(src_row_.data(), row, static_cast<size_t>(w) * sizeof(uint32_t));
        // Time-base jitter: smooth wander plus a little per-line noise.
        float shift = jitter * (0.6f * std::sin(static_cast<float>(y) * 0.05f + line_phase) + 0.4f * rng_.signed_unit());
        float band = 0;
        if (band_strength_ > 0 && std::abs(y - band_c) <= band_half) {
            band = band_strength_ * (1.0f - static_cast<float>(std::abs(y - band_c)) / static_cast<float>(band_half + 1));
            shift += band * (2.0f + e * 14.0f) * rng_.signed_unit();
        }
        const int from_bottom = h - 1 - y;
        if (from_bottom < hs_lines) {  // head switching: skewed, wobbly bottom lines
            const float k = 1.0f - static_cast<float>(from_bottom) / static_cast<float>(hs_lines);
            shift += k * (2.0f + e * 10.0f) + rng_.signed_unit() * (0.5f + e * 2.0f);
        }
        process_row(src_row_.data(), row, w, shift, soften, chroma_r, chroma_delay, grain);
        if (band > 0) {  // tracking band: bright speckled streaks
            const float p = band * (0.05f + e * 0.35f);
            for (int x = 0; x < w; ++x) {
                if (rng_.unit() < p) {
                    const int len = 1 + rng_.range(6);
                    const int v = 150 + rng_.range(106);
                    for (int k = 0; k < len && x + k < w; ++k) row[x + k] = rgb(v, v, v);
                    x += len;
                }
            }
        }
    }

    // Picture search bars (FF / REW).
    if (search_) {
        const int bars = 2;
        for (int b = 0; b < bars; ++b) {
            const int cy = static_cast<int>((ctx.frame * 7 + static_cast<uint64_t>(b) * static_cast<uint64_t>(h) / bars) %
                                            static_cast<uint64_t>(h));
            for (int y = cy - 5; y <= cy + 5; ++y) {
                if (y < 0 || y >= h) continue;
                uint32_t* row = fb.row(y);
                for (int x = 0; x < w; ++x)
                    if (rng_.unit() < 0.5f) {
                        const int v = rng_.range(256);
                        row[x] = rgb(v, v, v);
                    }
            }
        }
    }

    // Dropouts: rare short white flecks where oxide is missing.
    const float expected = e * 0.35f;  // ~1.5 per second at the default level
    int flecks = 0;
    if (rng_.unit() < expected) flecks = 1 + (rng_.unit() < expected ? 1 : 0);
    for (int i = 0; i < flecks; ++i) {
        const int y = rng_.range(h), x = rng_.range(w);
        const int len = 4 + rng_.range(18);
        uint32_t* row = fb.row(y);
        for (int k = 0; k < len && x + k < w; ++k) {
            const int v = 200 + rng_.range(56);
            row[x + k] = rgb(v, v, v);
        }
    }
}

}  // namespace rt90
