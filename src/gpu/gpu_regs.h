// GPU register file as seen by the CPU: screen mode, the OSD character layer
// and effect triggers.
//
// The CPU writes into a back buffer while it draws; latch() (called at vsync)
// copies it to the front buffer that the renderer reads, so the picture never
// shows a half-drawn OSD.
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "core/bus.h"

namespace rt90 {

class GpuRegs : public IoDevice {
public:
    static constexpr int kCols = 40;  // 320 / 8
    static constexpr int kRows = 30;  // 240 / 8

    struct Cell {
        uint8_t ch = ' ';
        uint8_t color = 0;
    };

    // Latched (displayed) state.
    uint8_t screen() const { return shown_screen_; }
    const Cell& cell(int col, int row) const { return shown_[static_cast<size_t>(row * kCols + col)]; }

    // Copies the CPU-side registers to the displayed state (at vsync).
    void latch();
    // Returns the OSD row as text (trailing spaces trimmed). Glyph codes are
    // returned as-is.
    std::string row_text(int row) const;
    // Every non-empty row joined with " | ", handy for logs and tests.
    std::string osd_text() const;

    // Counter that increments every time the firmware fires FX_TRACKING.
    uint32_t tracking_requests() const { return tracking_requests_; }

    void reset();

    uint8_t io_read(uint8_t port) override;
    void io_write(uint8_t port, uint8_t value) override;
    uint8_t io_peek(uint8_t port) const override;

private:
    std::array<Cell, kCols * kRows> cells_{};  // written by the CPU
    std::array<Cell, kCols * kRows> shown_{};  // latched at vsync
    uint8_t screen_ = 0;
    uint8_t shown_screen_ = 0;
    uint8_t cur_x_ = 0, cur_y_ = 0, color_ = 0;
    uint32_t tracking_requests_ = 0;
};

}  // namespace rt90
