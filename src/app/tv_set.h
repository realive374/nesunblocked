// The whole TV + VCR combo without any platform code: the VCR machine
// (CPU, RAM, deck, firmware), the GPU with VRAM, the tape streamer and the
// audio path. The SDL host feeds it time and input and plays what comes out;
// tests drive it directly.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "av/interlace.h"
#include "av/static_noise.h"
#include "gpu/gpu.h"
#include "gpu/vram.h"
#include "tape/tape_source.h"
#include "tape/tape_streamer.h"
#include "vcr/machine.h"

namespace rt90 {

class TvSet {
public:
    TvSet();
    ~TvSet();
    TvSet(const TvSet&) = delete;
    TvSet& operator=(const TvSet&) = delete;

    bool boot(std::string* error = nullptr);

    // Puts a tape in the deck. If a tape is already in, it is ejected first
    // (the firmware sees the tape change and shows static).
    void insert_tape(std::shared_ptr<TapeSource> source);
    bool has_tape() const { return machine_.deck().has_tape(); }
    std::shared_ptr<TapeSource> tape() const { return streamer_.source(); }

    // Emulates `seconds` of time; returns the number of video frames produced.
    int advance(double seconds) { return machine_.advance(seconds); }
    void run_frames(int n) { machine_.run_frames(n); }

    // Host UI drawn on top of the OSD each frame (noise slider, recording screen...).
    void set_overlay(std::function<void(Framebuffer&)> overlay) { overlay_ = std::move(overlay); }

    // Picture of the most recent frame (320x240) and its CRT rendering.
    const Framebuffer& picture() const { return gpu_.picture(); }
    void render(Framebuffer& canvas) { gpu_.present(canvas); }
    uint64_t frames_composed() const { return composed_; }

    // Audio produced so far, mono 16-bit at kAudioRate. take_audio() moves
    // the pending samples out (appending to `out`).
    size_t pending_audio() const { return audio_.size(); }
    void take_audio(std::vector<int16_t>& out);
    uint64_t audio_samples_produced() const { return audio_total_; }
    float volume = 1.0f;

    Machine& machine() { return machine_; }
    const Machine& machine() const { return machine_; }
    Gpu& gpu() { return gpu_; }
    Vram& vram() { return vram_; }
    const Vram& vram() const { return vram_; }
    TapeStreamer& streamer() { return streamer_; }
    InterlacePass& interlace() { return interlace_; }
    // Tape frame currently on screen (Vram::kNone if none).
    uint32_t shown_tape_frame() const { return vram_.front_frame(); }

private:
    void on_vsync();
    void update_tape_picture(TapeFrame*& played);
    void produce_audio(const TapeFrame* played);

    Machine machine_;
    Gpu gpu_;
    Vram vram_;
    TapeStreamer streamer_;
    SnowPass snow_;
    InterlacePass interlace_;
    HissGenerator hiss_;
    TapeFrame frame_;
    std::function<void(Framebuffer&)> overlay_;
    std::vector<int16_t> audio_;
    std::vector<float> mix_;
    uint64_t audio_total_ = 0;
    uint64_t composed_ = 0;
};

}  // namespace rt90
