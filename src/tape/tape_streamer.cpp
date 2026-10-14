#include "tape/tape_streamer.h"

#include <chrono>
#include <cstring>

namespace rt90 {

TapeStreamer::TapeStreamer(size_t buffer_bytes) : ring_(buffer_bytes) {}

TapeStreamer::~TapeStreamer() { detach(); }

void TapeStreamer::attach(std::shared_ptr<TapeSource> source) {
    detach();
    source_ = std::move(source);
    if (!source_) return;
    ring_.skip(ring_.size());
    stop_ = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        start_ = stream_start_ = 0;
        stride_ = stream_stride_ = 1;
        gen_.fetch_add(1);
    }
    thread_ = std::thread([this] { run(); });
}

void TapeStreamer::detach() {
    if (thread_.joinable()) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        thread_.join();
    }
    source_.reset();
    ring_.skip(ring_.size());
}

void TapeStreamer::restart(uint32_t start, int stride) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        start_ = stream_start_ = start;
        stride_ = stream_stride_ = stride == 0 ? 1 : stride;
        gen_.fetch_add(1);
    }
    cv_.notify_all();
}

bool TapeStreamer::on_stream(uint32_t index, int stride) const {
    if (stride != stream_stride_) return false;
    const int64_t delta = static_cast<int64_t>(index) - static_cast<int64_t>(stream_start_);
    if (stride > 0) return delta >= 0 && delta % stride == 0;
    return delta <= 0 && (-delta) % (-stride) == 0;
}

void TapeStreamer::run() {
    std::shared_ptr<TapeSource> src = source_;
    const uint32_t count = src->frame_count();
    TapeFrame frame;
    std::vector<uint8_t> record;
    while (!stop_) {
        uint32_t gen, start;
        int stride;
        {
            std::lock_guard<std::mutex> lock(mu_);
            gen = gen_.load();
            start = start_;
            stride = stride_;
        }
        int64_t cursor = start;
        while (!stop_ && gen_.load() == gen) {
            if (cursor < 0 || cursor >= count || !src->read_frame(static_cast<uint32_t>(cursor), frame)) {
                // End of the stream in this direction: wait for a restart.
                std::unique_lock<std::mutex> lock(mu_);
                cv_.wait_for(lock, std::chrono::milliseconds(50), [&] { return stop_ || gen_.load() != gen; });
                continue;
            }
            RecordHeader h{};
            h.gen = gen;
            h.index = static_cast<uint32_t>(cursor);
            h.video_bytes = static_cast<uint32_t>(frame.video.size());
            h.audio_samples = static_cast<uint32_t>(frame.audio.size());
            h.codec = static_cast<uint8_t>(frame.codec);
            record.resize(sizeof h + frame.video.size() + frame.audio.size() * sizeof(int16_t));
            std::memcpy(record.data(), &h, sizeof h);
            if (!frame.video.empty()) std::memcpy(record.data() + sizeof h, frame.video.data(), frame.video.size());
            if (!frame.audio.empty())
                std::memcpy(record.data() + sizeof h + frame.video.size(), frame.audio.data(),
                            frame.audio.size() * sizeof(int16_t));
            if (record.size() > ring_.capacity()) break;  // cannot ever fit; consumer reads directly
            // Wait for room (the consumer frees space every vsync).
            while (!stop_ && gen_.load() == gen && !ring_.write(record.data(), record.size())) {
                std::unique_lock<std::mutex> lock(mu_);
                cv_.wait_for(lock, std::chrono::milliseconds(5), [&] { return stop_ || gen_.load() != gen; });
            }
            cursor += stride;
        }
        if (!stop_ && gen_.load() == gen) {  // oversized record: idle until restarted
            std::unique_lock<std::mutex> lock(mu_);
            cv_.wait(lock, [&] { return stop_ || gen_.load() != gen; });
        }
    }
}

bool TapeStreamer::fetch(uint32_t index, int stride, TapeFrame& out) {
    if (!source_) return false;
    if (stride == 0) stride = 1;
    const uint32_t gen = gen_.load();
    RecordHeader h{};
    while (ring_.peek(&h, sizeof h)) {
        const size_t rec = sizeof h + h.video_bytes + h.audio_samples * sizeof(int16_t);
        if (ring_.size() < rec) break;  // producer is mid-write (cannot happen: writes are atomic)
        const bool stale = h.gen != gen;
        const bool behind = stride > 0 ? h.index < index : h.index > index;
        if (stale || (behind && h.index != index)) {
            ring_.skip(rec);
            continue;
        }
        if (h.index != index) break;  // buffer is ahead of us: resync below
        ring_.skip(sizeof h);
        out.index = h.index;
        out.codec = static_cast<VideoCodec>(h.codec);
        out.video.resize(h.video_bytes);
        out.audio.resize(h.audio_samples);
        ring_.read(out.video.data(), h.video_bytes);
        ring_.read(out.audio.data(), h.audio_samples * sizeof(int16_t));
        ++hits_;
        cv_.notify_all();  // space freed
        if (stride != stream_stride_) restart(static_cast<uint32_t>(std::max<int64_t>(0, int64_t{index} + stride)), stride);
        return true;
    }
    ++misses_;
    // Not buffered: point the read-ahead at what comes next, read directly.
    if (!on_stream(index, stride)) {
        const int64_t next = int64_t{index} + stride;
        restart(static_cast<uint32_t>(next < 0 ? 0 : next), stride);
    }
    return source_->read_frame(index, out);
}

}  // namespace rt90
