#include "tape/tape_source.h"

#include <cmath>
#include <cstring>

#include "av/timing.h"
#include "gpu/test_pattern.h"
#include "stb_image.h"

namespace rt90 {

double TapeSource::fps() const { return kNtscFps; }
int TapeSource::audio_rate() const { return kAudioRate; }

bool decode_video(const TapeFrame& frame, Framebuffer& out) {
    if (out.w != kScreenW || out.h != kScreenH) out.resize(kScreenW, kScreenH);
    switch (frame.codec) {
        case VideoCodec::Raw:
            if (frame.video.size() != out.px.size() * 4) return false;
            std::memcpy(out.px.data(), frame.video.data(), frame.video.size());
            return true;
        case VideoCodec::Jpeg: {
            int w = 0, h = 0, n = 0;
            stbi_uc* pix = stbi_load_from_memory(frame.video.data(), static_cast<int>(frame.video.size()), &w, &h,
                                                 &n, 3);
            if (!pix) return false;
            const bool ok = w == kScreenW && h == kScreenH;
            if (ok)
                for (size_t i = 0; i < out.px.size(); ++i)
                    out.px[i] = rgb(pix[i * 3], pix[i * 3 + 1], pix[i * 3 + 2]);
            stbi_image_free(pix);
            return ok;
        }
        default:
            return false;
    }
}

TestPatternSource::TestPatternSource(double seconds, std::string title)
    : title_(std::move(title)), frames_(static_cast<uint32_t>(seconds * kNtscFps)) {}

bool TestPatternSource::read_frame(uint32_t index, TapeFrame& out) {
    if (index >= frames_) return false;
    Framebuffer fb(kScreenW, kScreenH);
    draw_test_pattern(fb, index);
    out.index = index;
    out.codec = VideoCodec::Raw;
    out.video.resize(fb.px.size() * 4);
    std::memcpy(out.video.data(), fb.px.data(), out.video.size());
    const uint64_t first = audio_sample_at(index);
    const uint32_t n = audio_samples_in_frame(index);
    out.audio.resize(n);
    constexpr double kAmp = 0.1 * 32767;  // -20 dBFS
    for (uint32_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(first + i) / kAudioRate;
        out.audio[i] = static_cast<int16_t>(std::lround(kAmp * std::sin(2 * 3.14159265358979323846 * 1000.0 * t)));
    }
    return true;
}

}  // namespace rt90
