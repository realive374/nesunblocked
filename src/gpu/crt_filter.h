// CRT post-processing: upscales the 320x240 picture to the output canvas with
// slight screen curvature, scanlines, a vignette and phosphor glow.
#pragma once

#include <cstdint>
#include <vector>

#include "gpu/framebuffer.h"

namespace rt90 {

struct CrtSettings {
    bool enabled = true;
    float curvature = 0.035f;  // barrel distortion amount (0 = flat)
    float scanlines = 0.55f;   // 0 = smooth, 1 = strong dark gaps between lines
    float vignette = 0.30f;    // corner darkening
    float glow = 0.20f;        // phosphor bloom added from a blurred copy
    bool operator==(const CrtSettings& o) const {
        return enabled == o.enabled && curvature == o.curvature && scanlines == o.scanlines &&
               vignette == o.vignette && glow == o.glow;
    }
};

class CrtFilter {
public:
    CrtSettings& settings() { return settings_; }
    const CrtSettings& settings() const { return settings_; }

    // Renders `src` (any size, normally 320x240) into `dst` (its size is the
    // output canvas size).
    void process(const Framebuffer& src, Framebuffer& dst);

private:
    // Precomputed per-output-pixel sampling: source column/row and weights.
    struct Tap {
        int16_t x0 = -1;   // -1 = outside the tube (bezel)
        int16_t y0 = 0;
        uint8_t fx = 0;    // horizontal blend 0-255
        uint16_t w0 = 0;   // row y0 weight, 8.8 fixed point (incl. vignette)
        uint16_t w1 = 0;   // row y0+1 weight
        uint16_t wg = 0;   // glow weight
    };
    void rebuild(int src_w, int src_h, int dst_w, int dst_h);
    void blur(const Framebuffer& src);

    CrtSettings settings_;
    CrtSettings lut_settings_;
    int lut_src_w_ = 0, lut_src_h_ = 0, lut_dst_w_ = 0, lut_dst_h_ = 0;
    std::vector<Tap> lut_;
    Framebuffer glow_, tmp_;
};

}  // namespace rt90
