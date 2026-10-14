// Host-side TV screens drawn into the GPU overlay layer, in the same blocky
// OSD style as the firmware's text.
#pragma once

#include <string>

#include "gpu/framebuffer.h"

namespace rt90 {

// "NOISE [#####.....] 15%" slider near the bottom of the screen.
void draw_noise_slider(Framebuffer& fb, float level);

// "RECORDING..." screen with a progress bar (progress 0-1).
void draw_recording_screen(Framebuffer& fb, const std::string& title, float progress, uint64_t frame);

// Friendly "TAPE NOT RECOGNIZED" screen.
void draw_not_recognized(Framebuffer& fb, const std::string& file_name, const std::string& reason);

// Hint shown on the empty-deck snow: how to insert a tape.
void draw_insert_hint(Framebuffer& fb, uint64_t frame);

// Shortens `s` to at most `max_chars`, adding "..." when cut.
std::string ellipsize(const std::string& s, size_t max_chars);

}  // namespace rt90
