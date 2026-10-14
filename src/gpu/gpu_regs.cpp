#include "gpu/gpu_regs.h"

#include "vcr/io_ports.h"

namespace rt90 {

void GpuRegs::reset() {
    cells_.fill(Cell{});
    shown_.fill(Cell{});
    screen_ = shown_screen_ = io::SCREEN_OFF;
    cur_x_ = cur_y_ = color_ = 0;
}

void GpuRegs::latch() {
    shown_ = cells_;
    shown_screen_ = screen_;
}

std::string GpuRegs::row_text(int row) const {
    std::string s;
    for (int c = 0; c < kCols; ++c) s += static_cast<char>(cell(c, row).ch);
    size_t end = s.find_last_not_of(' ');
    return end == std::string::npos ? std::string() : s.substr(0, end + 1);
}

std::string GpuRegs::osd_text() const {
    std::string out;
    for (int r = 0; r < kRows; ++r) {
        std::string t = row_text(r);
        size_t start = t.find_first_not_of(' ');
        if (start == std::string::npos) continue;
        if (!out.empty()) out += " | ";
        out += t.substr(start);
    }
    return out;
}

uint8_t GpuRegs::io_peek(uint8_t port) const {
    using namespace io;
    switch (port) {
        case GPU_SCREEN:    return screen_;
        case GPU_OSD_X:     return cur_x_;
        case GPU_OSD_Y:     return cur_y_;
        case GPU_OSD_COLOR: return color_;
        default:            return 0;
    }
}

uint8_t GpuRegs::io_read(uint8_t port) { return io_peek(port); }

void GpuRegs::io_write(uint8_t port, uint8_t value) {
    using namespace io;
    switch (port) {
        case GPU_SCREEN: screen_ = value <= SCREEN_TAPE ? value : uint8_t{SCREEN_OFF}; break;
        case GPU_OSD_X: cur_x_ = static_cast<uint8_t>(value % kCols); break;
        case GPU_OSD_Y: cur_y_ = static_cast<uint8_t>(value % kRows); break;
        case GPU_OSD_COLOR: color_ = value; break;
        case GPU_OSD_CLEAR: cells_.fill(Cell{}); break;
        case GPU_OSD_CHAR:
            cells_[static_cast<size_t>(cur_y_ * kCols + cur_x_)] = Cell{value, color_};
            if (++cur_x_ >= kCols) {
                cur_x_ = 0;
                cur_y_ = static_cast<uint8_t>((cur_y_ + 1) % kRows);
            }
            break;
        case GPU_FX:
            if (value & FX_TRACKING) ++tracking_requests_;
            break;
        default: break;
    }
}

}  // namespace rt90
