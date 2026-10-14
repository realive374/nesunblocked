// Small 2D drawing helpers for the OSD / overlay layers.
#pragma once

#include <string>

#include "gpu/framebuffer.h"

namespace rt90 {

// Draws `text` with the 8x8 font at pixel (x, y). `scale` enlarges each font
// pixel into a scale x scale block. With `shadow`, a dark drop shadow is drawn
// one font pixel down-right so text stays readable on any picture.
void draw_text(Framebuffer& fb, int x, int y, const std::string& text, uint32_t color, int scale = 1,
               bool shadow = true);
int text_width(const std::string& text, int scale = 1);
void fill_rect(Framebuffer& fb, int x, int y, int w, int h, uint32_t color);
// Blends `color` over the rectangle with opacity alpha (0-255).
void blend_rect(Framebuffer& fb, int x, int y, int w, int h, uint32_t color, int alpha);

}  // namespace rt90
