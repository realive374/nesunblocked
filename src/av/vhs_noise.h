// VHS tape artifacts layered on the tape picture (GPU TapeSignal slot):
// a rolling tracking-noise band near the bottom, chroma bleed and smear,
// horizontal line jitter, softened detail, rare dropout flecks and the
// head-switching wobble at the bottom edge. Everything scales with one
// intensity value (default 15%) and stays subtle at that level.
#pragma once

#include <vector>

#include "av/rng.h"
#include "gpu/gpu.h"

namespace rt90 {

class VhsNoisePass : public PicturePass {
public:
    static constexpr float kDefaultIntensity = 0.15f;
    static constexpr int kTrackingFrames = 180;  // the clean-up lasts ~6 s

    const char* name() const override { return "vhs"; }
    void apply(Framebuffer& fb, const FrameContext& ctx) override;

    void set_intensity(float v);
    float intensity() const { return intensity_; }
    // Intensity actually applied this frame (lowered while tracking).
    float effective() const;

    // Tracking button: briefly cleans up the picture.
    void start_tracking() { tracking_age_ = 0; }
    bool tracking_active() const { return tracking_age_ < kTrackingFrames; }

    // Picture search (FF / REW) adds the classic horizontal noise bars.
    void set_search(bool on) { search_ = on; }

    // Tracking band state, exposed for the debug overlay and tests.
    float band_strength() const { return band_strength_; }

private:
    void update_band();
    void process_row(const uint32_t* src, uint32_t* dst, int w, float shift, float soften, float chroma_r,
                     float chroma_delay, float grain);

    float intensity_ = kDefaultIntensity;
    int tracking_age_ = 1 << 20;
    bool search_ = false;
    Rng rng_{0x5EED1234u};
    // Tracking band: an episode fades in and out every few seconds.
    float band_y_ = 0.85f;  // centre, as a fraction of the picture height
    float band_strength_ = 0;
    int band_timer_ = 90;
    int band_len_ = 0, band_age_ = 0;
    std::vector<float> y_, i_, q_, tmp_;
    std::vector<uint32_t> src_row_;
};

}  // namespace rt90
