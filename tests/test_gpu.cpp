// GPU tests: layer compositing, OSD rendering, pass slots and the CRT filter.
#include <chrono>
#include <set>

#include "av/vhs_noise.h"

#include "gpu/draw.h"
#include "gpu/font.h"
#include "gpu/gpu.h"
#include "gpu/test_pattern.h"
#include "test.h"
#include "vcr/io_ports.h"

using namespace rt90;
using namespace rt90::io;

namespace {

void set_screen(GpuRegs& regs, uint8_t screen) {
    regs.io_write(GPU_SCREEN, screen);
    regs.latch();
}

void write_text(GpuRegs& regs, int col, int row, const std::string& s, uint8_t color = OSD_GREEN) {
    regs.io_write(GPU_OSD_X, static_cast<uint8_t>(col));
    regs.io_write(GPU_OSD_Y, static_cast<uint8_t>(row));
    regs.io_write(GPU_OSD_COLOR, color);
    for (unsigned char c : s) regs.io_write(GPU_OSD_CHAR, c);
    regs.latch();
}

int count_color(const Framebuffer& fb, uint32_t c, int x0 = 0, int y0 = 0, int w = -1, int h = -1) {
    if (w < 0) w = fb.w;
    if (h < 0) h = fb.h;
    int n = 0;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) n += fb.at(x, y) == c;
    return n;
}

struct CountingPass : PicturePass {
    int calls = 0;
    uint32_t paint;
    explicit CountingPass(uint32_t c) : paint(c) {}
    const char* name() const override { return "counting"; }
    void apply(Framebuffer& fb, const FrameContext&) override {
        ++calls;
        fb.at(0, 0) = paint;
    }
};

}  // namespace

TEST(gpu_internal_resolution_is_4_3) {
    Gpu gpu;
    CHECK_EQ(gpu.picture().w, 320);
    CHECK_EQ(gpu.picture().h, 240);
    CHECK_EQ(gpu.picture().w * 3, gpu.picture().h * 4);
}

TEST(gpu_font_has_vcr_glyphs) {
    for (uint8_t g : {GLYPH_PLAY, GLYPH_REV, GLYPH_STOP, GLYPH_PAUSE, GLYPH_EJECT}) {
        const uint8_t* rows = font_glyph(g);
        int bits = 0;
        for (int r = 0; r < 8; ++r) bits += rows[r] != 0;
        CHECK(bits >= 5);
    }
    CHECK_EQ(font_glyph('A')[0], 0x0C);  // VGA font 'A' top row
}

TEST(gpu_blue_screen_with_osd_text) {
    GpuRegs regs;
    Gpu gpu;
    set_screen(regs, SCREEN_BLUE);
    write_text(regs, 15, 14, "NO SIGNAL", OSD_WHITE);
    gpu.compose(regs, nullptr, 1);
    const Framebuffer& p = gpu.picture();
    CHECK(count_color(p, Gpu::kBlueScreen) > 320 * 240 * 9 / 10);
    // White text pixels appear inside the text cells and nowhere else.
    CHECK(count_color(p, Gpu::kOsdWhite, 15 * 8, 14 * 8, 9 * 8, 8) > 50);
    CHECK_EQ(count_color(p, Gpu::kOsdWhite, 0, 0, 320, 100), 0);
}

TEST(gpu_osd_is_green_over_tape_picture) {
    GpuRegs regs;
    Gpu gpu;
    Framebuffer tape(kScreenW, kScreenH, rgb(10, 20, 30));
    set_screen(regs, SCREEN_TAPE);
    write_text(regs, 2, 2, "PLAY \x10");
    gpu.compose(regs, &tape, 1);
    const Framebuffer& p = gpu.picture();
    CHECK(count_color(p, Gpu::kOsdGreen, 16, 16, 6 * 8, 8) > 60);
    CHECK_EQ(p.at(300, 200), rgb(10, 20, 30));  // tape layer shows through elsewhere
}

TEST(gpu_tape_screen_without_frame_is_black) {
    GpuRegs regs;
    Gpu gpu;
    set_screen(regs, SCREEN_TAPE);
    gpu.compose(regs, nullptr, 1);
    CHECK_EQ(count_color(gpu.picture(), 0xFF000000u), 320 * 240);
}

TEST(gpu_static_screen_animates) {
    GpuRegs regs;
    Gpu gpu;
    set_screen(regs, SCREEN_STATIC);
    gpu.compose(regs, nullptr, 1);
    const std::vector<uint32_t> a = gpu.picture().px;
    gpu.compose(regs, nullptr, 2);
    CHECK(a != gpu.picture().px);
}

TEST(gpu_power_off_collapses_to_black) {
    GpuRegs regs;
    Gpu gpu;
    Framebuffer tape(kScreenW, kScreenH, rgb(200, 200, 200));
    set_screen(regs, SCREEN_TAPE);
    gpu.compose(regs, &tape, 1);
    set_screen(regs, SCREEN_OFF);
    gpu.compose(regs, nullptr, 2);
    const int lit_first = 320 * 240 - count_color(gpu.picture(), 0xFF000000u);
    CHECK(lit_first > 0);
    for (int i = 0; i < 30; ++i) gpu.compose(regs, nullptr, static_cast<uint64_t>(3 + i));
    CHECK_EQ(count_color(gpu.picture(), 0xFF000000u), 320 * 240);
}

TEST(gpu_pass_slots_run_in_the_right_screens) {
    GpuRegs regs;
    Gpu gpu;
    CountingPass snow(rgb(1, 1, 1)), signal(rgb(2, 2, 2)), field(rgb(3, 3, 3));
    gpu.set_pass(PassSlot::Snow, &snow);
    gpu.set_pass(PassSlot::TapeSignal, &signal);
    gpu.set_pass(PassSlot::Field, &field);
    Framebuffer tape(kScreenW, kScreenH);

    set_screen(regs, SCREEN_STATIC);
    gpu.compose(regs, &tape, 1);
    CHECK_EQ(snow.calls, 1);
    CHECK_EQ(signal.calls, 0);
    CHECK_EQ(field.calls, 1);

    set_screen(regs, SCREEN_TAPE);
    gpu.compose(regs, &tape, 2);
    CHECK_EQ(snow.calls, 1);
    CHECK_EQ(signal.calls, 1);
    CHECK_EQ(field.calls, 2);
    CHECK_EQ(gpu.picture().at(0, 0), rgb(3, 3, 3));  // field pass runs last

    set_screen(regs, SCREEN_BLUE);
    gpu.compose(regs, &tape, 3);
    CHECK_EQ(signal.calls, 1);  // tape-signal effects only touch tape video
}

TEST(gpu_overlay_draws_on_top) {
    GpuRegs regs;
    Gpu gpu;
    set_screen(regs, SCREEN_BLUE);
    gpu.compose(regs, nullptr, 1, [](Framebuffer& fb) { fill_rect(fb, 10, 10, 5, 5, rgb(255, 0, 0)); });
    CHECK_EQ(count_color(gpu.picture(), rgb(255, 0, 0)), 25);
}

TEST(crt_output_fills_canvas_with_curved_corners) {
    CrtFilter crt;
    Framebuffer src(kScreenW, kScreenH, rgb(128, 128, 128));
    Framebuffer dst(960, 720);
    crt.process(src, dst);
    CHECK_EQ(dst.at(0, 0), 0xFF000000u);        // curvature leaves the corners black
    CHECK_EQ(dst.at(959, 719), 0xFF000000u);
    const uint32_t c = dst.at(480, 360);        // centre keeps roughly its brightness
    CHECK(green(c) > 100 && green(c) < 200);
    CHECK(green(dst.at(480, 360)) > green(dst.at(40, 360)));  // vignette darkens edges
}

TEST(crt_scanlines_modulate_vertically) {
    CrtFilter crt;
    crt.settings().curvature = 0;
    crt.settings().vignette = 0;
    crt.settings().glow = 0;
    Framebuffer src(kScreenW, kScreenH, rgb(200, 200, 200));
    Framebuffer dst(960, 720);
    crt.process(src, dst);
    int lo = 255, hi = 0;
    for (int y = 300; y < 312; ++y) {
        lo = std::min(lo, green(dst.at(480, y)));
        hi = std::max(hi, green(dst.at(480, y)));
    }
    CHECK(hi - lo > 30);  // visible dark gaps between lines

    crt.settings().scanlines = 0;
    crt.process(src, dst);
    lo = 255, hi = 0;
    for (int y = 300; y < 312; ++y) {
        lo = std::min(lo, green(dst.at(480, y)));
        hi = std::max(hi, green(dst.at(480, y)));
    }
    CHECK(hi - lo <= 2);
}

TEST(crt_disabled_is_plain_upscale) {
    CrtFilter crt;
    crt.settings().enabled = false;
    Framebuffer src(kScreenW, kScreenH);
    draw_test_pattern(src, 0);
    Framebuffer dst(960, 720);
    crt.process(src, dst);
    CHECK_EQ(dst.at(0, 0), src.at(0, 0));
    CHECK_EQ(dst.at(481, 100), src.at(160, 33));
}

TEST(crt_glow_brightens_around_highlights) {
    CrtFilter crt;
    Framebuffer src(kScreenW, kScreenH, rgb(0, 0, 0));
    fill_rect(src, 150, 110, 20, 20, rgb(255, 255, 255));
    Framebuffer dst(960, 720);
    crt.process(src, dst);
    CHECK(green(dst.at(3 * 148, 3 * 120)) > 0);  // halo just outside the white square
    crt.settings().glow = 0;
    crt.process(src, dst);
    CHECK_EQ(green(dst.at(3 * 146, 3 * 120)), 0);
}

TEST(crt_is_fast_enough_for_30fps) {
    CrtFilter crt;
    Framebuffer src(kScreenW, kScreenH);
    draw_test_pattern(src, 0);
    Framebuffer dst(960, 720);
    crt.process(src, dst);  // builds the lookup table
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 10; ++i) crt.process(src, dst);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 10;
    std::printf("    crt: %.2f ms/frame at 960x720\n", ms);
    CHECK(ms < 33.0);
}

namespace {
// Leftmost x of the green OSD text in rows [y0, y1), or -1.
int osd_left_edge(const Framebuffer& p, int y0, int y1) {
    int best = -1;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < 100; ++x)
            if (p.at(x, y) == Gpu::kOsdGreen) {
                if (best < 0 || x < best) best = x;
                break;
            }
    return best;
}
}  // namespace

TEST(gpu_osd_words_jitter_with_the_tape) {
    GpuRegs regs;
    Gpu gpu;
    VhsNoisePass vhs;
    gpu.set_pass(PassSlot::TapeSignal, &vhs);
    Framebuffer tape(kScreenW, kScreenH, rgb(20, 20, 20));
    set_screen(regs, SCREEN_TAPE);
    write_text(regs, 2, 2, "PLAY \x10");
    std::set<int> lefts;
    int hops = 0;
    for (int f = 0; f < 90; ++f) {
        gpu.compose(regs, &tape, static_cast<uint64_t>(f));
        const int left = osd_left_edge(gpu.picture(), 8, 34);
        CHECK(left >= 16 - 12 && left <= 16 + 12);  // still readable where it belongs
        lefts.insert(left);
        hops += vhs.osd_hop() != 0;
    }
    CHECK(lefts.size() >= 3);  // the words visibly move from frame to frame
    CHECK(hops > 0);

    // The VCR's own blue screen is rock steady.
    set_screen(regs, SCREEN_BLUE);
    lefts.clear();
    for (int f = 0; f < 30; ++f) {
        gpu.compose(regs, &tape, static_cast<uint64_t>(100 + f));
        lefts.insert(osd_left_edge(gpu.picture(), 8, 34));
    }
    CHECK_EQ(lefts.size(), 1u);
}
