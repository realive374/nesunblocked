// Optional interlaced look: each video frame only refreshes one field (every
// other line); the other field still holds the previous frame, giving the
// combing on motion and slight line twitter of a 480i picture.
#pragma once

#include "gpu/gpu.h"

namespace rt90 {

class InterlacePass : public PicturePass {
public:
    bool enabled = false;
    const char* name() const override { return "interlace"; }
    void apply(Framebuffer& fb, const FrameContext& ctx) override;

private:
    Framebuffer prev_;
};

}  // namespace rt90
