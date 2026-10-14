#include "vcr/machine.h"

#include <algorithm>

#include "core/assembler.h"
#include "vcr/io_ports.h"

namespace rt90 {

uint8_t Machine::Panel::io_peek(uint8_t port) const {
    using namespace io;
    switch (port) {
        case BTN_HELD:  return held;
        case BTN_EDGE:  return edge;
        case FRAME_LO:  return frame ? static_cast<uint8_t>(*frame & 0xFF) : 0;
        case FRAME_HI:  return frame ? static_cast<uint8_t>((*frame >> 8) & 0xFF) : 0;
        case POWER_LED: return power_led ? 1 : 0;
        default:        return 0xFF;
    }
}

uint8_t Machine::Panel::io_read(uint8_t port) {
    uint8_t v = io_peek(port);
    if (port == io::BTN_EDGE) edge = 0;  // reading the edge latch clears it
    return v;
}

void Machine::Panel::io_write(uint8_t port, uint8_t value) {
    if (port == io::POWER_LED) power_led = value != 0;
}

Machine::Machine() : cpu_(bus_) {
    panel_.frame = &frame_;
    bus_.map_io(0x00, 0x0F, &panel_);
    bus_.map_io(0x10, 0x1F, &deck_);
    bus_.map_io(0x20, 0x2F, &gpu_);
}

bool Machine::boot(const std::string& source, std::string* error) {
    AsmResult res = assemble(source);
    if (!res.ok()) {
        if (error) *error = res.error_text();
        return false;
    }
    if (res.max_addr > memmap::ROM_END + 1u) {
        if (error) *error = "firmware does not fit in ROM";
        return false;
    }
    bus_.load_rom(res.slice(0));
    symbols_.assign(res.symbols.begin(), res.symbols.end());
    reset();
    return true;
}

void Machine::reset() {
    bus_.clear_ram();
    gpu_.reset();
    panel_.held = panel_.edge = 0;
    panel_.power_led = false;
    cpu_.reset(memmap::ROM_START);
    frame_ = 0;
    clock_ = 0;
    cycle_debt_ = 0;
}

uint16_t Machine::symbol(const std::string& name) const {
    for (const auto& [n, v] : symbols_)
        if (n == name) return v;
    return 0;
}

void Machine::press(uint8_t buttons) {
    panel_.edge |= static_cast<uint8_t>(buttons & ~panel_.held);
    panel_.held |= buttons;
}

void Machine::release(uint8_t buttons) { panel_.held &= static_cast<uint8_t>(~buttons); }

uint64_t Machine::frame_end_cycle(uint64_t frame) const {
    // Frame n ends at (n + 1) * kCpuHz / kFps cycles, computed exactly.
    return ((frame + 1) * kCpuHz * 1001) / 30000;
}

void Machine::vsync() {
    gpu_.latch();
    deck_.tick_frame();
    ++frame_;
    cpu_.vsync();
}

void Machine::run_frame() {
    const uint64_t end = frame_end_cycle(frame_);
    if (clock_ < end) clock_ += cpu_.run(end - clock_);
    vsync();
}

int Machine::advance(double seconds) {
    seconds = std::clamp(seconds, 0.0, 0.25);
    cycle_debt_ += seconds * static_cast<double>(kCpuHz);
    const uint64_t budget = static_cast<uint64_t>(cycle_debt_);
    cycle_debt_ -= static_cast<double>(budget);
    const uint64_t target = clock_ + budget;
    int frames = 0;
    while (clock_ < target) {
        const uint64_t end = frame_end_cycle(frame_);
        const uint64_t stop = std::min(end, target);
        clock_ += cpu_.run(stop - clock_);
        if (clock_ >= end) {
            vsync();
            ++frames;
        }
    }
    return frames;
}

}  // namespace rt90
