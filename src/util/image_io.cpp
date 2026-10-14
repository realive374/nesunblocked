#include "util/image_io.h"

#include <vector>

#include "stb_image_write.h"

namespace rt90 {

bool save_png(const Framebuffer& fb, const std::string& path) {
    std::vector<uint8_t> rgb_bytes(static_cast<size_t>(fb.w) * static_cast<size_t>(fb.h) * 3);
    for (size_t i = 0; i < fb.px.size(); ++i) {
        rgb_bytes[i * 3 + 0] = static_cast<uint8_t>(red(fb.px[i]));
        rgb_bytes[i * 3 + 1] = static_cast<uint8_t>(green(fb.px[i]));
        rgb_bytes[i * 3 + 2] = static_cast<uint8_t>(blue(fb.px[i]));
    }
    return stbi_write_png(path.c_str(), fb.w, fb.h, 3, rgb_bytes.data(), fb.w * 3) != 0;
}

}  // namespace rt90
