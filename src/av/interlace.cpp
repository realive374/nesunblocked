#include "av/interlace.h"

#include <cstring>

namespace rt90 {

void InterlacePass::apply(Framebuffer& fb, const FrameContext& ctx) {
    if (!enabled) {
        prev_.resize(0, 0);
        return;
    }
    if (prev_.w != fb.w || prev_.h != fb.h) {
        prev_ = fb;
        return;
    }
    const int field = static_cast<int>(ctx.frame & 1);
    for (int y = 0; y < fb.h; ++y) {
        uint32_t* row = fb.row(y);
        if ((y & 1) != field) {
            // Old field: previous frame's line, slightly faded (phosphor decay).
            const uint32_t* old = prev_.row(y);
            for (int x = 0; x < fb.w; ++x) {
                const uint32_t c = old[x];
                row[x] = rgb(red(c) * 15 / 16, green(c) * 15 / 16, blue(c) * 15 / 16);
            }
        }
        std::memcpy(prev_.row(y), row, static_cast<size_t>(fb.w) * sizeof(uint32_t));
    }
}

}  // namespace rt90
