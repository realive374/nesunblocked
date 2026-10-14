// System bus for the VCR-1: decodes the 16-bit address space into ROM, RAM,
// stack and the memory-mapped I/O page.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/isa.h"

namespace rt90 {

// A device that owns some of the 256 I/O ports at 0xFF00-0xFFFF.
class IoDevice {
public:
    virtual ~IoDevice() = default;
    // `port` is the low byte of the address (0x00-0xFF).
    virtual uint8_t io_read(uint8_t port) = 0;
    virtual void io_write(uint8_t port, uint8_t value) = 0;
    // Side-effect-free read used by debuggers. Defaults to 0.
    virtual uint8_t io_peek(uint8_t /*port*/) const { return 0; }
};

class Bus {
public:
    static constexpr size_t kSize = 0x10000;

    Bus();

    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t value);
    // Reads memory without triggering I/O side effects.
    uint8_t peek(uint16_t addr) const;

    // Copies a firmware image into ROM (bypasses write protection).
    void load_rom(const std::vector<uint8_t>& image, uint16_t origin = memmap::ROM_START);
    // Clears RAM and stack (ROM is preserved).
    void clear_ram();

    // Attaches `dev` to ports [first, last] inclusive.
    void map_io(uint8_t first, uint8_t last, IoDevice* dev);

    uint64_t rom_write_violations() const { return rom_violations_; }

private:
    std::array<uint8_t, kSize> mem_{};
    std::array<IoDevice*, 256> io_{};
    uint64_t rom_violations_ = 0;
};

}  // namespace rt90
