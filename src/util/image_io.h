// Image file helpers (PNG screenshots).
#pragma once

#include <string>

#include "gpu/framebuffer.h"

namespace rt90 {

bool save_png(const Framebuffer& fb, const std::string& path);

}  // namespace rt90
