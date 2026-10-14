// Port numbers and bit layouts of the VCR-1 memory-mapped I/O page.
//
// Every port lives at address 0xFF00 + port, so the firmware can reach it
// either with LD/ST on the full address or with the short IN/OUT forms.
// Keep this file in sync with the .equ block at the top of firmware/vcr1.asm.
#pragma once

#include <cstdint>

namespace rt90::io {

// --- System / front panel (0x00-0x0F) ---
constexpr uint8_t BTN_HELD  = 0x00;  // R: buttons currently held
constexpr uint8_t BTN_EDGE  = 0x01;  // R: buttons pressed since last read (read clears)
constexpr uint8_t FRAME_LO  = 0x02;  // R: vsync counter, low byte
constexpr uint8_t FRAME_HI  = 0x03;  // R: vsync counter, high byte
constexpr uint8_t POWER_LED = 0x04;  // W: 1 = power LED on

// Button bits for BTN_HELD / BTN_EDGE.
enum Button : uint8_t {
    BTN_POWER = 0x01,
    BTN_PLAY  = 0x02,  // play / pause toggle
    BTN_STOP  = 0x04,
    BTN_REW   = 0x08,
    BTN_FF    = 0x10,
    BTN_EJECT = 0x20,
    BTN_TRACK = 0x40,
};

// --- Tape deck (0x10-0x1F) ---
constexpr uint8_t DECK_STATUS = 0x10;  // R: see DeckStatus bits
constexpr uint8_t DECK_CMD    = 0x11;  // W: see DeckCmd
constexpr uint8_t DECK_HOURS  = 0x12;  // R: tape counter hours
constexpr uint8_t DECK_MINS   = 0x13;  // R: tape counter minutes
constexpr uint8_t DECK_SECS   = 0x14;  // R: tape counter seconds
constexpr uint8_t DECK_FRAMES = 0x15;  // R: tape counter frames (0-29)
constexpr uint8_t DECK_SPEED  = 0x16;  // R/W: shuttle speed for REW/FF (frames per vsync)

enum DeckStatus : uint8_t {
    DECK_HAS_TAPE = 0x01,
    DECK_MOTOR    = 0x02,  // reels turning (play / rew / ff)
    DECK_BOT      = 0x04,  // at beginning of tape
    DECK_EOT      = 0x08,  // at end of tape
    DECK_CHANGED  = 0x10,  // tape inserted/removed since last ACK
};

enum DeckCmd : uint8_t {
    CMD_STOP  = 0,
    CMD_PLAY  = 1,
    CMD_PAUSE = 2,
    CMD_REW   = 3,
    CMD_FF    = 4,
    CMD_EJECT = 5,
    CMD_ACK   = 6,  // acknowledge DECK_CHANGED
};

// --- GPU (0x20-0x2F) ---
constexpr uint8_t GPU_SCREEN    = 0x20;  // W: Screen mode
constexpr uint8_t GPU_OSD_X     = 0x21;  // W: OSD cursor column (0-39)
constexpr uint8_t GPU_OSD_Y     = 0x22;  // W: OSD cursor row (0-29)
constexpr uint8_t GPU_OSD_CHAR  = 0x23;  // W: write char at cursor, cursor advances
constexpr uint8_t GPU_OSD_CLEAR = 0x24;  // W: clear whole OSD layer
constexpr uint8_t GPU_OSD_COLOR = 0x25;  // W: OsdColor for following chars
constexpr uint8_t GPU_FX        = 0x26;  // W: effect triggers (FX_* bits)

enum Screen : uint8_t {
    SCREEN_OFF    = 0,  // CRT off (black)
    SCREEN_STATIC = 1,  // snow (no signal on the RF input)
    SCREEN_BLUE   = 2,  // VCR blue "NO SIGNAL" background
    SCREEN_TAPE   = 3,  // picture from the tape
};

enum OsdColor : uint8_t {
    OSD_GREEN = 0,
    OSD_WHITE = 1,
    OSD_RED   = 2,
};

enum GpuFx : uint8_t {
    FX_TRACKING = 0x01,  // run the tracking auto-adjust (briefly cleans up the picture)
};

// OSD glyph codes outside printable ASCII.
enum Glyph : uint8_t {
    GLYPH_PLAY  = 0x10,  // ▶
    GLYPH_REV   = 0x11,  // ◀
    GLYPH_STOP  = 0x12,  // ■
    GLYPH_PAUSE = 0x13,  // ‖
    GLYPH_EJECT = 0x14,  // ⏏
};

}  // namespace rt90::io
