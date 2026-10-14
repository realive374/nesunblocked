// Virtual tape transport. Tracks the tape position (in frames) and transport
// state; the firmware drives it through the DECK_* I/O ports.
#pragma once

#include <cstdint>
#include <string>

#include "core/bus.h"

namespace rt90 {

struct TapeInfo {
    std::string title;
    std::string path;          // file backing the tape (empty for blank tapes)
    uint32_t frame_count = 0;  // length in frames
    double fps = 29.97;
};

enum class DeckState { Empty, Stopped, Playing, Paused, Rewinding, FastForward };

const char* deck_state_name(DeckState s);

class Deck : public IoDevice {
public:
    static constexpr uint8_t kDefaultShuttle = 12;  // frames per vsync in REW/FF

    void insert(const TapeInfo& tape);
    void eject();

    // Advances the transport by one video frame (called on every vsync).
    void tick_frame();

    void command(uint8_t cmd);
    // Moves the head to `frame` (clamped). Used by seeking and tests.
    void seek(uint32_t frame);

    bool has_tape() const { return state_ != DeckState::Empty; }
    DeckState state() const { return state_; }
    const TapeInfo& tape() const { return tape_; }
    uint32_t position() const { return position_; }
    uint8_t status() const;
    uint8_t shuttle_speed() const { return shuttle_; }

    // Tape counter derived from the position.
    void counter(int& h, int& m, int& s, int& f) const;

    uint8_t io_read(uint8_t port) override;
    void io_write(uint8_t port, uint8_t value) override;
    uint8_t io_peek(uint8_t port) const override;

private:
    TapeInfo tape_;
    DeckState state_ = DeckState::Empty;
    uint32_t position_ = 0;
    uint8_t shuttle_ = kDefaultShuttle;
    bool changed_ = false;
};

}  // namespace rt90
