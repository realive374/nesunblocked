// 32-bit XRGB pixel buffers and colour helpers shared by the GPU stages.
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace rt90 {

constexpr int kScreenW = 320;  // internal resolution (4:3)
constexpr int kScreenH = 240;

struct Framebuffer {
    int w = 0, h = 0;
    std::vector<uint32_t> px;  // 0xFFRRGGBB

    Framebuffer() = default;
    Framebuffer(int width, int height, uint32_t fill = 0xFF000000u) { resize(width, height, fill); }
    void resize(int width, int height, uint32_t fill = 0xFF000000u) {
        w = width;
        h = height;
        px.assign(static_cast<size_t>(w) * static_cast<size_t>(h), fill);
    }
    void fill(uint32_t c) { std::fill(px.begin(), px.end(), c); }
    uint32_t* row(int y) { return px.data() + static_cast<size_t>(y) * static_cast<size_t>(w); }
    const uint32_t* row(int y) const { return px.data() + static_cast<size_t>(y) * static_cast<size_t>(w); }
    uint32_t& at(int x, int y) { return px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]; }
    uint32_t at(int x, int y) const { return px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]; }
};

constexpr uint32_t rgb(int r, int g, int b) {
    return 0xFF000000u | (static_cast<uint32_t>(r & 0xFF) << 16) | (static_cast<uint32_t>(g & 0xFF) << 8) |
           static_cast<uint32_t>(b & 0xFF);
}
constexpr int red(uint32_t c) { return static_cast<int>((c >> 16) & 0xFF); }
constexpr int green(uint32_t c) { return static_cast<int>((c >> 8) & 0xFF); }
constexpr int blue(uint32_t c) { return static_cast<int>(c & 0xFF); }
inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
inline uint32_t rgb_clamped(int r, int g, int b) { return rgb(clamp255(r), clamp255(g), clamp255(b)); }
// Rec.601 luma, 0-255.
constexpr int luma(uint32_t c) { return (red(c) * 77 + green(c) * 150 + blue(c) * 29) >> 8; }

}  // namespace rt90
