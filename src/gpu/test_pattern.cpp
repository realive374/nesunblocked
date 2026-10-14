#include "gpu/test_pattern.h"

#include "gpu/draw.h"

namespace rt90 {

void draw_test_pattern(Framebuffer& fb, uint32_t tape_frame) {
    static constexpr uint32_t bars[7] = {rgb(192, 192, 192), rgb(192, 192, 0), rgb(0, 192, 192), rgb(0, 192, 0),
                                         rgb(192, 0, 192),   rgb(192, 0, 0),   rgb(0, 0, 192)};
    const int split = fb.h * 2 / 3;
    for (int y = 0; y < fb.h; ++y) {
        uint32_t* row = fb.row(y);
        for (int x = 0; x < fb.w; ++x) {
            if (y < split) {
                row[x] = bars[x * 7 / fb.w];
            } else {
                const int v = x * 255 / (fb.w - 1);  // grey ramp
                row[x] = rgb(v, v, v);
            }
        }
    }
    const int travel = fb.w - 40;
    const int pos = static_cast<int>(tape_frame % static_cast<uint32_t>(2 * travel));
    const int bx = 20 + (pos < travel ? pos : 2 * travel - pos);
    fill_rect(fb, bx - 10, split + 14, 20, 20, rgb(255, 255, 255));
    draw_text(fb, fb.w / 2 - text_width("RETROTUBE 90") / 2, split - 24, "RETROTUBE 90", rgb(255, 255, 255));
}

}  // namespace rt90
