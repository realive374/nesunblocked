#include "core/ring_buffer.h"

#include <algorithm>
#include <cstring>

namespace rt90 {

namespace {
size_t round_pow2(size_t n) {
    size_t p = 16;
    while (p < n) p <<= 1;
    return p;
}
}  // namespace

RingBuffer::RingBuffer(size_t capacity) : buf_(round_pow2(capacity)), mask_(buf_.size() - 1) {}

size_t RingBuffer::size() const {
    return head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire);
}

size_t RingBuffer::free() const { return capacity() - size(); }

bool RingBuffer::write(const void* data, size_t n) {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t tail = tail_.load(std::memory_order_acquire);
    if (n > capacity() - (head - tail)) return false;
    const size_t pos = head & mask_;
    const size_t first = std::min(n, capacity() - pos);
    const auto* src = static_cast<const uint8_t*>(data);
    std::memcpy(buf_.data() + pos, src, first);
    if (n > first) std::memcpy(buf_.data(), src + first, n - first);
    head_.store(head + n, std::memory_order_release);
    return true;
}

void RingBuffer::copy_out(size_t pos, void* out, size_t n) const {
    const size_t p = pos & mask_;
    const size_t first = std::min(n, capacity() - p);
    auto* dst = static_cast<uint8_t*>(out);
    std::memcpy(dst, buf_.data() + p, first);
    if (n > first) std::memcpy(dst + first, buf_.data(), n - first);
}

bool RingBuffer::peek(void* out, size_t n) const {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t head = head_.load(std::memory_order_acquire);
    if (head - tail < n) return false;
    copy_out(tail, out, n);
    return true;
}

bool RingBuffer::read(void* out, size_t n) {
    if (!peek(out, n)) return false;
    tail_.store(tail_.load(std::memory_order_relaxed) + n, std::memory_order_release);
    return true;
}

void RingBuffer::skip(size_t n) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t avail = head_.load(std::memory_order_acquire) - tail;
    tail_.store(tail + std::min(n, avail), std::memory_order_release);
}

}  // namespace rt90
