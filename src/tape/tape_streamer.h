// Streams tape frames ahead of the play head. A background thread reads
// frames from the TapeSource and writes them as records into a ring buffer;
// the video pipeline pulls them out at vsync. The read-ahead follows the
// current stride, so play (+1), fast-forward (+N) and rewind (-N) all stream.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "core/ring_buffer.h"
#include "tape/tape_source.h"

namespace rt90 {

class TapeStreamer {
public:
    explicit TapeStreamer(size_t buffer_bytes = 8u << 20);
    ~TapeStreamer();
    TapeStreamer(const TapeStreamer&) = delete;
    TapeStreamer& operator=(const TapeStreamer&) = delete;

    void attach(std::shared_ptr<TapeSource> source);
    void detach();
    bool attached() const { return source_ != nullptr; }
    const std::shared_ptr<TapeSource>& source() const { return source_; }

    // Returns frame `index`. `stride` is how the play head will move next
    // (+1 play, +N fast-forward, -N rewind) and steers the read-ahead.
    // Falls back to a direct read when the frame is not buffered.
    bool fetch(uint32_t index, int stride, TapeFrame& out);

    double fill_ratio() const { return ring_.fill_ratio(); }
    size_t buffered_bytes() const { return ring_.size(); }
    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }

private:
    struct RecordHeader {
        uint32_t gen;
        uint32_t index;
        uint32_t video_bytes;
        uint32_t audio_samples;
        uint8_t codec;
        uint8_t pad[3];
    };

    void run();
    void restart(uint32_t start, int stride);
    bool on_stream(uint32_t index, int stride) const;

    RingBuffer ring_;
    std::shared_ptr<TapeSource> source_;
    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
    std::atomic<uint32_t> gen_{0};
    // Producer target for the current generation (guarded by mu_).
    uint32_t start_ = 0;
    int stride_ = 1;
    // Consumer-side copy of the stream the producer is following.
    uint32_t stream_start_ = 0;
    int stream_stride_ = 1;
    uint64_t hits_ = 0, misses_ = 0;
};

}  // namespace rt90
