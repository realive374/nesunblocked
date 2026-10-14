#include "gpu/crt_filter.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace rt90 {

namespace {

// Runs fn(y_begin, y_end) over [0, rows) on a few threads.
template <typename Fn>
void parallel_rows(int rows, Fn fn) {
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const int n = static_cast<int>(std::min<unsigned>(hw, 8u));
    if (n <= 1 || rows < 64) {
        fn(0, rows);
        return;
    }
    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(n - 1));
    const int chunk = (rows + n - 1) / n;
    for (int i = 1; i < n; ++i) {
        const int b = i * chunk, e = std::min(rows, b + chunk);
        if (b < e) pool.emplace_back([=] { fn(b, e); });
    }
    fn(0, std::min(rows, chunk));
    for (std::thread& t : pool) t.join();
}

float smoothstep(float e0, float e1, float x) {
    float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

uint16_t fixed88(float v) { return static_cast<uint16_t>(std::clamp(v * 256.0f + 0.5f, 0.0f, 65535.0f)); }

}  // namespace

void CrtFilter::rebuild(int src_w, int src_h, int dst_w, int dst_h) {
    lut_.assign(static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h), Tap{});
    const CrtSettings& s = settings_;
    const bool on = s.enabled;
    const float k = on ? s.curvature : 0.0f;
    const float scan = on ? s.scanlines : 0.0f;
    const float vig_amt = on ? s.vignette : 0.0f;
    const float glow = on ? s.glow : 0.0f;
    const float sigma = 0.30f;
    const float gain = 1.0f + 0.30f * scan;  // compensate for the darker gaps
    const float edge_soft = 2.5f / static_cast<float>(std::min(dst_w, dst_h));

    for (int y = 0; y < dst_h; ++y) {
        for (int x = 0; x < dst_w; ++x) {
            Tap& tap = lut_[static_cast<size_t>(y) * static_cast<size_t>(dst_w) + static_cast<size_t>(x)];
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(dst_w) * 2.0f - 1.0f;
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(dst_h) * 2.0f - 1.0f;
            const float cu = u * (1.0f + k * v * v);
            const float cv = v * (1.0f + k * u * u);
            if (std::fabs(cu) >= 1.0f || std::fabs(cv) >= 1.0f) continue;  // bezel

            float sx = (cu + 1.0f) * 0.5f * static_cast<float>(src_w) - 0.5f;
            float sy = (cv + 1.0f) * 0.5f * static_cast<float>(src_h) - 0.5f;
            int x0 = static_cast<int>(std::floor(sx));
            float fx = sx - static_cast<float>(x0);
            if (x0 < 0) { x0 = 0; fx = 0; }
            if (x0 >= src_w - 1) { x0 = src_w - 2; fx = 1; }
            int y0 = static_cast<int>(std::floor(sy));
            float t = sy - static_cast<float>(y0);
            if (y0 < 0) { y0 = 0; t = 0; }
            if (y0 >= src_h - 1) { y0 = src_h - 2; t = 1; }

            const float b0 = std::exp(-(t * t) / (2 * sigma * sigma));
            const float b1 = std::exp(-((1 - t) * (1 - t)) / (2 * sigma * sigma));
            float w0 = (1 - scan) * (1 - t) + scan * b0;
            float w1 = (1 - scan) * t + scan * b1;

            const float r2 = cu * cu + cv * cv;
            float vig = std::max(0.0f, 1.0f - vig_amt * 0.5f * r2 * r2);
            if (on && k > 0) {
                const float edge = std::min(1.0f - std::fabs(cu), 1.0f - std::fabs(cv));
                vig *= smoothstep(0.0f, edge_soft, edge);
            }
            tap.x0 = static_cast<int16_t>(x0);
            tap.y0 = static_cast<int16_t>(y0);
            tap.fx = static_cast<uint8_t>(std::clamp(fx * 256.0f, 0.0f, 255.0f));
            tap.w0 = fixed88(w0 * gain * vig);
            tap.w1 = fixed88(w1 * gain * vig);
            tap.wg = fixed88(glow * vig);
        }
    }
    lut_settings_ = settings_;
    lut_src_w_ = src_w;
    lut_src_h_ = src_h;
    lut_dst_w_ = dst_w;
    lut_dst_h_ = dst_h;
}

void CrtFilter::blur(const Framebuffer& src) {
    // Highlights glow more: square the signal before blurring.
    const int w = src.w, h = src.h, r = 3;
    tmp_.resize(w, h);
    glow_.resize(w, h);
    auto sq = [](int c) { return c * c / 255; };
    for (int y = 0; y < h; ++y) {  // horizontal box blur
        const uint32_t* in = src.row(y);
        uint32_t* out = tmp_.row(y);
        int sr = 0, sg = 0, sb = 0;
        for (int x = -r; x <= r; ++x) {
            const uint32_t c = in[std::clamp(x, 0, w - 1)];
            sr += sq(red(c)); sg += sq(green(c)); sb += sq(blue(c));
        }
        for (int x = 0; x < w; ++x) {
            out[x] = rgb(sr / (2 * r + 1), sg / (2 * r + 1), sb / (2 * r + 1));
            const uint32_t a = in[std::clamp(x + r + 1, 0, w - 1)], d = in[std::clamp(x - r, 0, w - 1)];
            sr += sq(red(a)) - sq(red(d));
            sg += sq(green(a)) - sq(green(d));
            sb += sq(blue(a)) - sq(blue(d));
        }
    }
    for (int x = 0; x < w; ++x) {  // vertical box blur
        int sr = 0, sg = 0, sb = 0;
        for (int y = -r; y <= r; ++y) {
            const uint32_t c = tmp_.at(x, std::clamp(y, 0, h - 1));
            sr += red(c); sg += green(c); sb += blue(c);
        }
        for (int y = 0; y < h; ++y) {
            glow_.at(x, y) = rgb(sr / (2 * r + 1), sg / (2 * r + 1), sb / (2 * r + 1));
            const uint32_t a = tmp_.at(x, std::clamp(y + r + 1, 0, h - 1));
            const uint32_t d = tmp_.at(x, std::clamp(y - r, 0, h - 1));
            sr += red(a) - red(d);
            sg += green(a) - green(d);
            sb += blue(a) - blue(d);
        }
    }
}

void CrtFilter::process(const Framebuffer& src, Framebuffer& dst) {
    if (src.w < 2 || src.h < 2 || dst.w <= 0 || dst.h <= 0) return;
    if (!(lut_settings_ == settings_) || lut_src_w_ != src.w || lut_src_h_ != src.h || lut_dst_w_ != dst.w ||
        lut_dst_h_ != dst.h || lut_.empty())
        rebuild(src.w, src.h, dst.w, dst.h);
    const bool glow_on = settings_.enabled && settings_.glow > 0;
    if (glow_on) blur(src);

    const int dw = dst.w;
    parallel_rows(dst.h, [&](int yb, int ye) {
        for (int y = yb; y < ye; ++y) {
            const Tap* tap = &lut_[static_cast<size_t>(y) * static_cast<size_t>(dw)];
            uint32_t* out = dst.row(y);
            for (int x = 0; x < dw; ++x, ++tap) {
                if (tap->x0 < 0) {
                    out[x] = 0xFF000000u;
                    continue;
                }
                const uint32_t* r0 = src.row(tap->y0) + tap->x0;
                const uint32_t* r1 = r0 + src.w;
                const int fx = tap->fx, ifx = 256 - fx;
                auto lerp_ch = [&](const uint32_t* p, int shift) {
                    return ((static_cast<int>((p[0] >> shift) & 0xFF) * ifx) +
                            (static_cast<int>((p[1] >> shift) & 0xFF) * fx)) >> 8;
                };
                int c[3];
                for (int ch = 0; ch < 3; ++ch) {
                    const int shift = 16 - ch * 8;
                    c[ch] = (lerp_ch(r0, shift) * tap->w0 + lerp_ch(r1, shift) * tap->w1) >> 8;
                }
                if (glow_on) {
                    const uint32_t g = glow_.at(tap->x0, tap->y0);
                    c[0] += (red(g) * tap->wg) >> 8;
                    c[1] += (green(g) * tap->wg) >> 8;
                    c[2] += (blue(g) * tap->wg) >> 8;
                }
                out[x] = rgb_clamped(c[0], c[1], c[2]);
            }
        }
    });
}

}  // namespace rt90
