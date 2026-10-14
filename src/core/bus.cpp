#include "core/bus.h"

#include <algorithm>

namespace rt90 {

Bus::Bus() { io_.fill(nullptr); }

uint8_t Bus::read(uint16_t addr) {
    if (addr >= memmap::IO_START) {
        IoDevice* dev = io_[addr & 0xFF];
        return dev ? dev->io_read(static_cast<uint8_t>(addr & 0xFF)) : 0xFF;  // open bus
    }
    return mem_[addr];
}

uint8_t Bus::peek(uint16_t addr) const {
    if (addr >= memmap::IO_START) {
        const IoDevice* dev = io_[addr & 0xFF];
        return dev ? dev->io_peek(static_cast<uint8_t>(addr & 0xFF)) : 0xFF;
    }
    return mem_[addr];
}

void Bus::write(uint16_t addr, uint8_t value) {
    if (addr <= memmap::ROM_END) {
        ++rom_violations_;  // ROM is read-only; the write is dropped
        return;
    }
    if (addr >= memmap::IO_START) {
        if (IoDevice* dev = io_[addr & 0xFF]) dev->io_write(static_cast<uint8_t>(addr & 0xFF), value);
        return;
    }
    mem_[addr] = value;
}

void Bus::load_rom(const std::vector<uint8_t>& image, uint16_t origin) {
    std::fill(mem_.begin(), mem_.begin() + memmap::ROM_END + 1, uint8_t{0});
    for (size_t i = 0; i < image.size(); ++i) {
        size_t addr = origin + i;
        if (addr > memmap::ROM_END) break;
        mem_[addr] = image[i];
    }
}

void Bus::clear_ram() {
    std::fill(mem_.begin() + memmap::RAM_START, mem_.begin() + memmap::IO_START, uint8_t{0});
}

void Bus::map_io(uint8_t first, uint8_t last, IoDevice* dev) {
    for (int p = first; p <= last; ++p) io_[p] = dev;
}

}  // namespace rt90
