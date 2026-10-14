#include "vcr/deck.h"

#include <algorithm>
#include <cmath>

#include "vcr/io_ports.h"

namespace rt90 {

const char* deck_state_name(DeckState s) {
    switch (s) {
        case DeckState::Empty:       return "EMPTY";
        case DeckState::Stopped:     return "STOP";
        case DeckState::Playing:     return "PLAY";
        case DeckState::Paused:      return "PAUSE";
        case DeckState::Rewinding:   return "REW";
        case DeckState::FastForward: return "FF";
    }
    return "?";
}

void Deck::insert(const TapeInfo& tape) {
    tape_ = tape;
    if (tape_.fps <= 0) tape_.fps = 29.97;
    position_ = 0;
    state_ = DeckState::Stopped;
    changed_ = true;
}

void Deck::eject() {
    if (state_ == DeckState::Empty) return;
    tape_ = TapeInfo{};
    position_ = 0;
    state_ = DeckState::Empty;
    changed_ = true;
}

void Deck::seek(uint32_t frame) {
    if (!has_tape()) return;
    position_ = std::min(frame, tape_.frame_count);
}

void Deck::tick_frame() {
    switch (state_) {
        case DeckState::Playing:
            if (position_ < tape_.frame_count) ++position_;
            break;
        case DeckState::FastForward:
            position_ = std::min<uint32_t>(position_ + shuttle_, tape_.frame_count);
            break;
        case DeckState::Rewinding:
            position_ = position_ > shuttle_ ? position_ - shuttle_ : 0;
            break;
        default:
            break;
    }
}

void Deck::command(uint8_t cmd) {
    using namespace io;
    if (cmd == CMD_ACK) {
        changed_ = false;
        return;
    }
    if (!has_tape()) return;
    switch (cmd) {
        case CMD_STOP:  state_ = DeckState::Stopped; break;
        case CMD_PLAY:  state_ = DeckState::Playing; break;
        case CMD_PAUSE: state_ = DeckState::Paused; break;
        case CMD_REW:   state_ = DeckState::Rewinding; break;
        case CMD_FF:    state_ = DeckState::FastForward; break;
        case CMD_EJECT: eject(); break;
        default: break;
    }
}

uint8_t Deck::status() const {
    using namespace io;
    uint8_t s = 0;
    if (has_tape()) {
        s |= DECK_HAS_TAPE;
        if (state_ == DeckState::Playing || state_ == DeckState::Rewinding || state_ == DeckState::FastForward)
            s |= DECK_MOTOR;
        if (position_ == 0) s |= DECK_BOT;
        if (position_ >= tape_.frame_count) s |= DECK_EOT;
    }
    if (changed_) s |= DECK_CHANGED;
    return s;
}

void Deck::counter(int& h, int& m, int& s, int& f) const {
    const double fps = tape_.fps > 0 ? tape_.fps : 29.97;
    const double secs = position_ / fps;
    const long total = static_cast<long>(std::floor(secs));
    h = static_cast<int>(std::min<long>(total / 3600, 255));
    m = static_cast<int>((total / 60) % 60);
    s = static_cast<int>(total % 60);
    f = static_cast<int>(std::floor((secs - static_cast<double>(total)) * fps));
}

uint8_t Deck::io_peek(uint8_t port) const {
    using namespace io;
    int h, m, s, f;
    counter(h, m, s, f);
    switch (port) {
        case DECK_STATUS: return status();
        case DECK_HOURS:  return static_cast<uint8_t>(h);
        case DECK_MINS:   return static_cast<uint8_t>(m);
        case DECK_SECS:   return static_cast<uint8_t>(s);
        case DECK_FRAMES: return static_cast<uint8_t>(f);
        case DECK_SPEED:  return shuttle_;
        default:          return 0xFF;
    }
}

uint8_t Deck::io_read(uint8_t port) { return io_peek(port); }

void Deck::io_write(uint8_t port, uint8_t value) {
    using namespace io;
    if (port == DECK_CMD) command(value);
    else if (port == DECK_SPEED) shuttle_ = std::max<uint8_t>(1, value);
}

}  // namespace rt90
