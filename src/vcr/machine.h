// The VCR "motherboard": wires the VCR-1 CPU, bus, tape deck, front panel and
// GPU registers together and keeps them on a common clock.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/bus.h"
#include "core/cpu.h"
#include "gpu/gpu_regs.h"
#include "vcr/deck.h"

namespace rt90 {

// Firmware source embedded at build time from firmware/vcr1.asm.
extern const char* const kFirmwareVcr1Source;

class Machine {
public:
    static constexpr uint64_t kCpuHz = 1'000'000;  // VCR-1 clock: 1 MHz
    static constexpr double kFps = 30000.0 / 1001.0;  // NTSC 29.97

    Machine();
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;

    // Assembles `source`, loads it into ROM and resets. On failure returns
    // false and fills `error`.
    bool boot(const std::string& source, std::string* error = nullptr);
    bool boot_default(std::string* error = nullptr) { return boot(kFirmwareVcr1Source, error); }
    void reset();

    // Front-panel buttons (io::Button bits).
    void press(uint8_t buttons);
    void release(uint8_t buttons);
    // Press + release in one go; the edge latch makes sure the firmware sees it.
    void tap(uint8_t buttons) {
        press(buttons);
        release(buttons);
    }

    // Advances emulated time by `seconds` of wall-clock time (clamped to
    // 250 ms to avoid a spiral of death after a stall). Returns the number of
    // video frames (vsyncs) that elapsed.
    int advance(double seconds);
    // Runs exactly one frame: the CPU cycles up to the next vsync, then vsync.
    void run_frame();
    void run_frames(int n) {
        for (int i = 0; i < n; ++i) run_frame();
    }

    Bus& bus() { return bus_; }
    Cpu& cpu() { return cpu_; }
    const Cpu& cpu() const { return cpu_; }
    Deck& deck() { return deck_; }
    const Deck& deck() const { return deck_; }
    GpuRegs& gpu() { return gpu_; }
    const GpuRegs& gpu() const { return gpu_; }

    uint64_t frame() const { return frame_; }
    bool power_led() const { return panel_.power_led; }
    uint16_t symbol(const std::string& name) const;  // firmware symbol address (0 if unknown)

private:
    struct Panel : IoDevice {
        uint8_t held = 0;
        uint8_t edge = 0;
        uint64_t* frame = nullptr;
        bool power_led = false;
        uint8_t io_read(uint8_t port) override;
        void io_write(uint8_t port, uint8_t value) override;
        uint8_t io_peek(uint8_t port) const override;
    };

    uint64_t frame_end_cycle(uint64_t frame) const;
    void vsync();

    Bus bus_;
    Cpu cpu_;
    Deck deck_;
    GpuRegs gpu_;
    Panel panel_;
    uint64_t frame_ = 0;
    uint64_t clock_ = 0;      // cycles run since reset
    double cycle_debt_ = 0;   // fractional cycles owed by advance()
    std::vector<std::pair<std::string, uint16_t>> symbols_;
};

}  // namespace rt90
