#include "core/ram.h"

#include <algorithm>
#include <cstring>

namespace rt90 {

uint8_t Ram::read(size_t addr) const {
    if (!in_bounds(addr)) {
        ++violations_;
        return 0xFF;
    }
    return mem_[addr];
}

bool Ram::write(size_t addr, uint8_t value) {
    if (!in_bounds(addr)) {
        ++violations_;
        return false;
    }
    mem_[addr] = value;
    touched_.set(addr);
    return true;
}

bool Ram::load(size_t addr, const uint8_t* data, size_t n) {
    if (addr > kSize || n > kSize - addr) {
        ++violations_;
        return false;
    }
    if (n) std::memcpy(mem_.data() + addr, data, n);
    for (size_t i = 0; i < n; ++i) touched_.set(addr + i);
    return true;
}

void Ram::clear(size_t from, size_t to) {
    to = std::min(to, kSize);
    if (from >= to) return;
    std::fill(mem_.begin() + static_cast<std::ptrdiff_t>(from), mem_.begin() + static_cast<std::ptrdiff_t>(to),
              uint8_t{0});
    for (size_t a = from; a < to; ++a) touched_.reset(a);
}

size_t Ram::bytes_used(size_t from, size_t to) const {
    to = std::min(to, kSize);
    size_t n = 0;
    for (size_t a = from; a < to; ++a) n += touched_.test(a);
    return n;
}

}  // namespace rt90
