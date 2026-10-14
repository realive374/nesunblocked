// Double-buffered video RAM for the tape frame layer. The video pipeline
// decodes the next frame into the back buffer; swap() at vsync makes it the
// front buffer the GPU composites from.
#pragma once

#include <cstdint>
#include <utility>

#include "gpu/framebuffer.h"

namespace rt90 {

class Vram {
public:
    static constexpr size_t kBytes = 2u * kScreenW * kScreenH * sizeof(uint32_t);

    Framebuffer& back() { return buf_[back_]; }
    const Framebuffer& front() const { return buf_[back_ ^ 1]; }
    // Tape frame number shown in the front buffer (or kNone).
    static constexpr uint32_t kNone = 0xFFFFFFFFu;
    uint32_t front_frame() const { return front_frame_; }

    // Publishes the back buffer, tagged with the tape frame it holds.
    void swap(uint32_t frame) {
        back_ ^= 1;
        front_frame_ = frame;
        ++swaps_;
    }
    void invalidate() { front_frame_ = kNone; }
    uint64_t swaps() const { return swaps_; }

private:
    Framebuffer buf_[2] = {Framebuffer(kScreenW, kScreenH), Framebuffer(kScreenW, kScreenH)};
    int back_ = 0;
    uint32_t front_frame_ = kNone;
    uint64_t swaps_ = 0;
};

}  // namespace rt90
