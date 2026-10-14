#include "core/bus.h"

#include <algorithm>

namespace rt90 {

Bus::Bus() { io_.fill(nullptr); }

uint8_t Bus::read(uint16_t addr) {
    if (addr >= memmap::IO_START) {
        IoDevice* dev = io_[addr & 0xFF];
        return dev ? dev->io_read(static_cast<uint8_t>(addr & 0xFF)) : 0xFF;  // open bus
    }
    return ram_.read(addr);
}

uint8_t Bus::peek(uint16_t addr) const {
    if (addr >= memmap::IO_START) {
        const IoDevice* dev = io_[addr & 0xFF];
        return dev ? dev->io_peek(static_cast<uint8_t>(addr & 0xFF)) : 0xFF;
    }
    return ram_.read(addr);
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
    ram_.write(addr, value);
}

void Bus::load_rom(const std::vector<uint8_t>& image, uint16_t origin) {
    ram_.clear(memmap::ROM_START, memmap::ROM_END + 1u);
    const size_t room = origin <= memmap::ROM_END ? memmap::ROM_END + 1u - origin : 0;
    ram_.load(origin, image.data(), std::min(image.size(), room));
}

void Bus::clear_ram() {
    ram_.clear(memmap::RAM_START, memmap::IO_START);
}

void Bus::map_io(uint8_t first, uint8_t last, IoDevice* dev) {
    for (int p = first; p <= last; ++p) io_[p] = dev;
}

}  // namespace rt90
