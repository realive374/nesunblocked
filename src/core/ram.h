// Fixed-size 64 KB emulated memory backing the VCR-1 address space.
//
// Every access is bounds-checked: out-of-range reads return 0xFF (open bus)
// and out-of-range writes are dropped; both are counted as violations so bugs
// show up in the debug overlay and tests instead of corrupting the host.
#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>

namespace rt90 {

class Ram {
public:
    static constexpr size_t kSize = 64 * 1024;

    size_t size() const { return kSize; }
    static bool in_bounds(size_t addr) { return addr < kSize; }

    uint8_t read(size_t addr) const;
    bool write(size_t addr, uint8_t value);
    // Copies `n` bytes to [addr, addr+n). All-or-nothing: fails without
    // writing anything if the range does not fit.
    bool load(size_t addr, const uint8_t* data, size_t n);
    // Zeroes [from, to) (clamped to the memory size).
    void clear(size_t from, size_t to);

    // Number of distinct addresses in [from, to) written since the last clear.
    size_t bytes_used(size_t from, size_t to) const;
    uint64_t violations() const { return violations_; }

private:
    std::array<uint8_t, kSize> mem_{};
    std::bitset<kSize> touched_;
    mutable uint64_t violations_ = 0;
};

}  // namespace rt90
