// JPEG encoding of tape frames.
#pragma once

#include <cstdint>
#include <vector>

#include "gpu/framebuffer.h"

namespace rt90 {

// Encodes `fb` as a baseline JPEG (quality 1-100). Returns false on failure.
bool encode_jpeg(const Framebuffer& fb, int quality, std::vector<uint8_t>& out);

}  // namespace rt90
