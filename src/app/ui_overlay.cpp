#include "app/ui_overlay.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "gpu/draw.h"
#include "gpu/font.h"
#include "gpu/gpu.h"

namespace rt90 {

namespace {

std::string upper_ascii(const std::string& s) {
    std::string out;
    for (unsigned char c : s) out += (c >= 32 && c < 127) ? static_cast<char>(std::toupper(c)) : '?';
    return out;
}

void centered(Framebuffer& fb, int y, const std::string& text, uint32_t color, int scale = 1) {
    draw_text(fb, (fb.w - text_width(text, scale)) / 2, y, text, color, scale);
}

}  // namespace

std::string ellipsize(const std::string& s, size_t max_chars) {
    if (s.size() <= max_chars) return s;
    if (max_chars <= 3) return s.substr(0, max_chars);
    return s.substr(0, max_chars - 3) + "...";
}

void draw_noise_slider(Framebuffer& fb, float level) {
    level = std::clamp(level, 0.0f, 1.0f);
    const int pct = static_cast<int>(std::lround(level * 100));
    const int cells = 20, filled = static_cast<int>(std::lround(level * cells));
    std::string bar = "NOISE ";
    for (int i = 0; i < cells; ++i) bar += i < filled ? static_cast<char>(GLYPH_BLOCK) : '-';
    bar += " " + std::to_string(pct) + "%";
    const int y = fb.h - 48;
    blend_rect(fb, 8, y - 4, fb.w - 16, 16, 0xFF000000u, 120);
    centered(fb, y, bar, Gpu::kOsdGreen);
}

void draw_insert_hint(Framebuffer& fb, uint64_t frame) {
    if ((frame / 45) % 4 == 3) return;  // blink slowly, like a VCR prompt
    blend_rect(fb, 0, fb.h - 64, fb.w, 40, 0xFF000000u, 150);
    centered(fb, fb.h - 58, "INSERT A TAPE:", Gpu::kOsdWhite);
    centered(fb, fb.h - 44, "DRAG A MOVIE HERE OR PRESS SPACE", Gpu::kOsdGreen);
}

void draw_recording_screen(Framebuffer& fb, const std::string& title, float progress, uint64_t frame) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    fb.fill(rgb(8, 8, 12));
    // Blinking red REC dot.
    if ((frame / 15) % 2 == 0) {
        draw_text(fb, 16, 16, std::string(1, static_cast<char>(GLYPH_DOT)), Gpu::kOsdRed);
    }
    draw_text(fb, 28, 16, "REC", Gpu::kOsdRed);
    centered(fb, 80, "RECORDING...", Gpu::kOsdGreen, 2);
    centered(fb, 108, ellipsize(upper_ascii(title), 36), Gpu::kOsdWhite);
    const int bw = 240, bx = (fb.w - bw) / 2, by = 132;
    fill_rect(fb, bx - 2, by - 2, bw + 4, 14, Gpu::kOsdGreen);
    fill_rect(fb, bx, by, bw, 10, rgb(8, 8, 12));
    fill_rect(fb, bx + 1, by + 1, static_cast<int>(static_cast<float>(bw - 2) * progress), 8, Gpu::kOsdGreen);
    centered(fb, 152, std::to_string(static_cast<int>(progress * 100)) + "%", Gpu::kOsdGreen);
    centered(fb, 200, "ESC: CANCEL", rgb(120, 120, 120));
}

void draw_not_recognized(Framebuffer& fb, const std::string& file_name, const std::string& reason) {
    fb.fill(Gpu::kBlueScreen);
    centered(fb, 72, "TAPE NOT", Gpu::kOsdWhite, 2);
    centered(fb, 92, "RECOGNIZED", Gpu::kOsdWhite, 2);
    centered(fb, 128, ellipsize(upper_ascii(file_name), 36), Gpu::kOsdGreen);
    centered(fb, 144, ellipsize(upper_ascii(reason), 38), Gpu::kOsdGreen);
    centered(fb, 184, "PLEASE INSERT AN .MP4 OR .VHS", Gpu::kOsdWhite);
    centered(fb, 196, "FILE (DRAG IT ONTO THE TV)", Gpu::kOsdWhite);
}

}  // namespace rt90
