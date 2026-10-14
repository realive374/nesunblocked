// Colour-bar test signal, used as the picture of blank test tapes.
#pragma once

#include <cstdint>

#include "gpu/framebuffer.h"

namespace rt90 {

// Draws SMPTE-style colour bars with a box that moves with `tape_frame`, so
// play / rewind / fast-forward are visible.
void draw_test_pattern(Framebuffer& fb, uint32_t tape_frame);

}  // namespace rt90
