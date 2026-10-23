#include "gpu/gpu.h"

#include <cmath>

#include "gpu/font.h"
#include "vcr/io_ports.h"

namespace rt90 {

Gpu::Gpu() { passes_.fill(nullptr); }

uint32_t Gpu::osd_color(uint8_t c) {
    switch (c) {
        case io::OSD_WHITE: return kOsdWhite;
        case io::OSD_RED:   return kOsdRed;
        default:            return kOsdGreen;
    }
}

void Gpu::compose(const GpuRegs& regs, const Framebuffer* tape_frame, uint64_t frame,
                  const std::function<void(Framebuffer&)>& overlay) {
    const uint8_t screen = regs.screen();
    const FrameContext ctx{frame, screen};

    if (screen == io::SCREEN_OFF) {
        if (last_screen_ != io::SCREEN_OFF) off_frames_ = 0;
        last_screen_ = screen;
        power_off_effect();
        ++off_frames_;
        return;
    }
    last_screen_ = screen;

    // Layer 1: background / tape picture.
    switch (screen) {
        case io::SCREEN_STATIC:
            if (PicturePass* snow = pass(PassSlot::Snow)) {
                snow->apply(picture_, ctx);
            } else {
                for (uint32_t& p : picture_.px) {  // fallback snow
                    rng_ = rng_ * 1664525u + 1013904223u;
                    const int v = static_cast<int>(rng_ >> 24);
                    p = rgb(v, v, v);
                }
            }
            break;
        case io::SCREEN_BLUE:
            picture_.fill(kBlueScreen);
            break;
        case io::SCREEN_TAPE:
            if (tape_frame && tape_frame->w == kScreenW && tape_frame->h == kScreenH) {
                picture_.px = tape_frame->px;
                if (PicturePass* p = pass(PassSlot::TapeSignal)) p->apply(picture_, ctx);
            } else {
                picture_.fill(0xFF000000u);
            }
            break;
        default:
            picture_.fill(0xFF000000u);
            break;
    }

    // Layer 2: on-screen display, then host overlay.
    // Over tape video the OSD rides the same unstable signal and jitters with it.
    draw_osd(regs, screen == io::SCREEN_TAPE && tape_frame ? pass(PassSlot::TapeSignal) : nullptr);
    if (overlay) overlay(picture_);
    if (PicturePass* p = pass(PassSlot::Field)) p->apply(picture_, ctx);
    last_on_.px = picture_.px;
}

void Gpu::draw_osd(const GpuRegs& regs, const PicturePass* jitter) {
    const int hop = jitter ? jitter->osd_hop() : 0;
    auto plot = [&](int x, int y, uint32_t c) {
        y += hop;
        if (y < 0 || y >= kScreenH) return;
        x += jitter ? jitter->osd_shift(y) : 0;
        if (x >= 0 && x < kScreenW) picture_.at(x, y) = c;
    };
    for (int row = 0; row < GpuRegs::kRows; ++row) {
        for (int col = 0; col < GpuRegs::kCols; ++col) {
            const GpuRegs::Cell& cell = regs.cell(col, row);
            if (cell.ch == ' ' || cell.ch == 0) continue;
            const uint8_t* g = font_glyph(cell.ch);
            const uint32_t color = osd_color(cell.color);
            const int x0 = col * 8, y0 = row * 8;
            // Blocky text with a 1-pixel dark outline (drawn first).
            for (int r = 0; r < 8; ++r)
                for (int c = 0; c < 8; ++c)
                    if (g[r] & (1u << c))
                        for (int dy = 0; dy <= 1; ++dy)
                            for (int dx = 0; dx <= 1; ++dx) {
                                if (dx || dy) plot(x0 + c + dx, y0 + r + dy, rgb(0, 24, 0));
                            }
            for (int r = 0; r < 8; ++r)
                for (int c = 0; c < 8; ++c)
                    if (g[r] & (1u << c)) plot(x0 + c, y0 + r, color);
        }
    }
}

// Classic CRT switch-off: the picture collapses to a bright line, then a dot.
void Gpu::power_off_effect() {
    picture_.fill(0xFF000000u);
    constexpr int kCollapse = 6, kDot = 14;
    if (off_frames_ >= kDot) return;
    const int cy = kScreenH / 2, cx = kScreenW / 2;
    if (off_frames_ < kCollapse) {
        const float t = static_cast<float>(off_frames_ + 1) / kCollapse;
        const int half = std::max(1, static_cast<int>((1.0f - t) * kScreenH / 2));
        for (int y = cy - half; y < cy + half; ++y) {
            const int sy = cy + (y - cy) * (kScreenH / 2) / half;
            if (y < 0 || y >= kScreenH || sy < 0 || sy >= kScreenH) continue;
            for (int x = 0; x < kScreenW; ++x) {
                const uint32_t c = last_on_.at(x, sy);
                picture_.at(x, y) = rgb_clamped(red(c) + 120, green(c) + 120, blue(c) + 120);
            }
        }
    } else {
        const float t = static_cast<float>(off_frames_ - kCollapse) / (kDot - kCollapse);
        const int half_w = static_cast<int>((1.0f - t) * (1.0f - t) * kScreenW / 2);
        const int v = static_cast<int>(255 * (1.0f - t * 0.5f));
        for (int x = cx - half_w; x <= cx + half_w; ++x)
            if (x >= 0 && x < kScreenW) picture_.at(x, cy) = rgb(v, v, v);
    }
}

void Gpu::present(Framebuffer& canvas) { crt_.process(picture_, canvas); }

}  // namespace rt90
