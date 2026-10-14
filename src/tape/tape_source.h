// Where tape frames come from: a .vhs file (Phase 5) or a built-in test tape.
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "gpu/framebuffer.h"

namespace rt90 {

enum class VideoCodec : uint8_t { None = 0, Raw = 1, Jpeg = 2 };

// One frame of tape: compressed (or raw) picture plus its slice of audio.
struct TapeFrame {
    uint32_t index = 0;
    VideoCodec codec = VideoCodec::None;
    std::vector<uint8_t> video;   // Raw: 320x240 XRGB little-endian; Jpeg: JFIF bytes
    std::vector<int16_t> audio;   // mono PCM at the tape's audio rate
};

class TapeSource {
public:
    virtual ~TapeSource() = default;
    virtual std::string title() const = 0;
    virtual uint32_t frame_count() const = 0;
    virtual double fps() const;
    virtual int audio_rate() const;
    virtual std::string path() const { return {}; }
    // Must be safe to call from several threads.
    virtual bool read_frame(uint32_t index, TapeFrame& out) = 0;
};

// Decodes a tape frame's picture into a 320x240 framebuffer.
bool decode_video(const TapeFrame& frame, Framebuffer& out);

// Blank "T-120" test tape: colour bars with a moving marker and a 1 kHz
// line-up tone at -20 dBFS.
class TestPatternSource : public TapeSource {
public:
    explicit TestPatternSource(double seconds = 2 * 3600.0, std::string title = "T-120 TEST TAPE");
    std::string title() const override { return title_; }
    uint32_t frame_count() const override { return frames_; }
    bool read_frame(uint32_t index, TapeFrame& out) override;

private:
    std::string title_;
    uint32_t frames_;
};

}  // namespace rt90
