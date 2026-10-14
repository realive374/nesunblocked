// Lock-free single-producer / single-consumer byte ring buffer. The tape
// streamer thread writes chunk records into it; the video thread reads them.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rt90 {

class RingBuffer {
public:
    // Capacity is rounded up to a power of two (minimum 16 bytes).
    explicit RingBuffer(size_t capacity);

    size_t capacity() const { return mask_ + 1; }
    size_t size() const;   // bytes available to read
    size_t free() const;   // bytes available to write
    double fill_ratio() const { return static_cast<double>(size()) / static_cast<double>(capacity()); }

    // Producer side: writes all `n` bytes or nothing.
    bool write(const void* data, size_t n);
    // Consumer side: reads exactly `n` bytes or nothing.
    bool read(void* out, size_t n);
    // Consumer side: copies `n` bytes without consuming them.
    bool peek(void* out, size_t n) const;
    // Consumer side: drops `n` bytes (or everything if fewer are available).
    void skip(size_t n);

private:
    void copy_out(size_t pos, void* out, size_t n) const;

    std::vector<uint8_t> buf_;
    size_t mask_;
    std::atomic<size_t> head_{0};  // total bytes written (producer)
    std::atomic<size_t> tail_{0};  // total bytes read (consumer)
};

}  // namespace rt90
