// NTSC frame timing shared by video and audio.
#pragma once

#include <cstdint>

namespace rt90 {

constexpr int kFpsNum = 30000;  // 29.97 fps = 30000 / 1001
constexpr int kFpsDen = 1001;
constexpr double kNtscFps = static_cast<double>(kFpsNum) / kFpsDen;
constexpr int kAudioRate = 22050;  // tape audio: mono 16-bit PCM

// Index of the first audio sample belonging to video frame `frame`. Frame n
// owns samples [audio_sample_at(n), audio_sample_at(n + 1)), so audio and
// video stay locked with no accumulated rounding error.
constexpr uint64_t audio_sample_at(uint64_t frame, int rate = kAudioRate) {
    return frame * static_cast<uint64_t>(rate) * kFpsDen / kFpsNum;
}
constexpr uint32_t audio_samples_in_frame(uint64_t frame, int rate = kAudioRate) {
    return static_cast<uint32_t>(audio_sample_at(frame + 1, rate) - audio_sample_at(frame, rate));
}

}  // namespace rt90
