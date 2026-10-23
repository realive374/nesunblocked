// Virtual GPU: composites the picture from the tape frame layer and the
// on-screen display layer at 320x240, then runs the post-processing pipeline
// (pluggable picture passes + the CRT filter) into the output canvas.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

#include "gpu/crt_filter.h"
#include "gpu/framebuffer.h"
#include "gpu/gpu_regs.h"

namespace rt90 {

struct FrameContext {
    uint64_t frame = 0;   // vsync count
    uint8_t screen = 0;   // io::Screen
};

// A picture-processing stage that runs at the internal 320x240 resolution.
class PicturePass {
public:
    virtual ~PicturePass() = default;
    virtual const char* name() const = 0;
    virtual void apply(Framebuffer& fb, const FrameContext& ctx) = 0;
    // How far the OSD text on scanline `y` is pushed sideways / up-down by
    // this pass's signal (only the tape-signal pass moves it).
    virtual int osd_shift(int /*y*/) const { return 0; }
    virtual int osd_hop() const { return 0; }
};

// Where a pass plugs into the pipeline:
//   Snow       - generates the picture while the screen shows static
//   TapeSignal - degrades the tape picture (VHS artifacts), before the OSD
//   Field      - whole-picture video-signal effects (interlacing), after the OSD
enum class PassSlot { Snow = 0, TapeSignal = 1, Field = 2 };
constexpr int kPassSlots = 3;

class Gpu {
public:
    static constexpr uint32_t kBlueScreen = rgb(16, 40, 200);
    static constexpr uint32_t kOsdGreen = rgb(60, 255, 80);
    static constexpr uint32_t kOsdWhite = rgb(240, 240, 240);
    static constexpr uint32_t kOsdRed = rgb(255, 60, 50);

    Gpu();

    void set_pass(PassSlot slot, PicturePass* pass) { passes_[static_cast<int>(slot)] = pass; }
    PicturePass* pass(PassSlot slot) const { return passes_[static_cast<int>(slot)]; }

    // Builds the 320x240 picture. `tape_frame` is the tape layer (320x240) or
    // nullptr when no tape picture is available. `overlay` (optional) draws
    // host UI such as the noise slider on top of the OSD.
    void compose(const GpuRegs& regs, const Framebuffer* tape_frame, uint64_t frame,
                 const std::function<void(Framebuffer&)>& overlay = {});

    // Runs the CRT stage into `canvas` (whatever size it has).
    void present(Framebuffer& canvas);

    const Framebuffer& picture() const { return picture_; }
    CrtFilter& crt() { return crt_; }

    static uint32_t osd_color(uint8_t c);

private:
    void draw_osd(const GpuRegs& regs, const PicturePass* jitter);
    void power_off_effect();

    Framebuffer picture_{kScreenW, kScreenH};
    Framebuffer last_on_{kScreenW, kScreenH};
    CrtFilter crt_;
    std::array<PicturePass*, kPassSlots> passes_{};
    uint8_t last_screen_ = 0;
    int off_frames_ = 1000;  // frames since the CRT was switched off
    uint32_t rng_ = 0x1234567u;
};

}  // namespace rt90
