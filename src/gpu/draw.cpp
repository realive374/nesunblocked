#include "gpu/draw.h"

#include "gpu/font.h"

namespace rt90 {

namespace {
void glyph(Framebuffer& fb, int x, int y, uint8_t ch, uint32_t color, int scale) {
    const uint8_t* rows = font_glyph(ch);
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            if (!(rows[r] & (1u << c))) continue;
            fill_rect(fb, x + c * scale, y + r * scale, scale, scale, color);
        }
    }
}
}  // namespace

int text_width(const std::string& text, int scale) { return static_cast<int>(text.size()) * 8 * scale; }

void draw_text(Framebuffer& fb, int x, int y, const std::string& text, uint32_t color, int scale, bool shadow) {
    int cx = x;
    for (unsigned char ch : text) {
        if (ch != ' ') {
            if (shadow) glyph(fb, cx + scale, y + scale, ch, 0xFF000000u, scale);
            glyph(fb, cx, y, ch, color, scale);
        }
        cx += 8 * scale;
    }
}

void fill_rect(Framebuffer& fb, int x, int y, int w, int h, uint32_t color) {
    const int x0 = std::max(0, x), y0 = std::max(0, y);
    const int x1 = std::min(fb.w, x + w), y1 = std::min(fb.h, y + h);
    for (int yy = y0; yy < y1; ++yy) std::fill(fb.row(yy) + x0, fb.row(yy) + std::max(x0, x1), color);
}

void blend_rect(Framebuffer& fb, int x, int y, int w, int h, uint32_t color, int alpha) {
    const int x0 = std::max(0, x), y0 = std::max(0, y);
    const int x1 = std::min(fb.w, x + w), y1 = std::min(fb.h, y + h);
    const int a = clamp255(alpha), ia = 255 - a;
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* p = fb.row(yy);
        for (int xx = x0; xx < x1; ++xx) {
            const uint32_t c = p[xx];
            p[xx] = rgb((red(c) * ia + red(color) * a) / 255, (green(c) * ia + green(color) * a) / 255,
                        (blue(c) * ia + blue(color) * a) / 255);
        }
    }
}

}  // namespace rt90
