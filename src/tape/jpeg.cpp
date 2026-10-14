#include "tape/jpeg.h"

#include "stb_image_write.h"

namespace rt90 {

bool encode_jpeg(const Framebuffer& fb, int quality, std::vector<uint8_t>& out) {
    out.clear();
    std::vector<uint8_t> rgb_bytes(fb.px.size() * 3);
    for (size_t i = 0; i < fb.px.size(); ++i) {
        rgb_bytes[i * 3 + 0] = static_cast<uint8_t>(red(fb.px[i]));
        rgb_bytes[i * 3 + 1] = static_cast<uint8_t>(green(fb.px[i]));
        rgb_bytes[i * 3 + 2] = static_cast<uint8_t>(blue(fb.px[i]));
    }
    auto sink = [](void* ctx, void* data, int size) {
        auto* v = static_cast<std::vector<uint8_t>*>(ctx);
        const auto* p = static_cast<const uint8_t*>(data);
        v->insert(v->end(), p, p + size);
    };
    return stbi_write_jpg_to_func(sink, &out, fb.w, fb.h, 3, rgb_bytes.data(), quality) != 0 && !out.empty();
}

}  // namespace rt90
