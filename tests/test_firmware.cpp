// Firmware tests: boot the real VCR-1 firmware on the emulated machine and
// drive it through the front panel like a user would.
#include "core/assembler.h"
#include "test.h"
#include "vcr/io_ports.h"
#include "vcr/machine.h"

using namespace rt90;
using namespace rt90::io;

namespace {

constexpr int kSecond = 30;  // ~frames per second

TapeInfo test_tape(double seconds = 600) {
    TapeInfo t;
    t.title = "TEST TAPE";
    t.fps = Machine::kFps;
    t.frame_count = static_cast<uint32_t>(seconds * t.fps);
    return t;
}

void boot(Machine& m) {
    std::string err;
    const bool ok = m.boot_default(&err);
    CHECK(ok);
    if (!ok) std::fprintf(stderr, "%s", err.c_str());
}

bool osd_has(const Machine& m, const std::string& text) {
    return m.gpu().osd_text().find(text) != std::string::npos;
}

// Settles the machine past the power-on static.
void power_up(Machine& m) {
    boot(m);
    m.run_frames(2 * kSecond);
}

}  // namespace

TEST(firmware_assembles_and_fits_in_rom) {
    AsmResult r = assemble(kFirmwareVcr1Source);
    CHECK(r.ok());
    if (!r.ok()) std::fprintf(stderr, "%s", r.error_text().c_str());
    CHECK(r.max_addr <= memmap::ROM_END + 1u);
    CHECK(r.size() > 200);
}

TEST(firmware_power_on_shows_static_then_settles) {
    Machine m;
    boot(m);
    m.run_frames(2);
    CHECK(m.power_led());
    CHECK_EQ(m.gpu().screen(), SCREEN_STATIC);
    m.run_frames(2 * kSecond);
    // No tape: the TV keeps showing snow.
    CHECK_EQ(m.gpu().screen(), SCREEN_STATIC);
    CHECK(m.cpu().fault() == CpuFault::None);
}

TEST(firmware_insert_tape_shows_static_then_blue_screen) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape());
    m.run_frames(2);
    CHECK_EQ(m.gpu().screen(), SCREEN_STATIC);  // switching tapes
    m.run_frames(kSecond + 2);
    CHECK_EQ(m.gpu().screen(), SCREEN_BLUE);
    CHECK(osd_has(m, "NO SIGNAL"));
}

TEST(firmware_play_pause_stop) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape());
    m.run_frames(2 * kSecond);

    m.tap(BTN_PLAY);
    m.run_frames(2);
    CHECK(m.deck().state() == DeckState::Playing);
    CHECK(osd_has(m, "PLAY \x10"));
    CHECK(osd_has(m, "0:00:00"));
    CHECK_EQ(m.gpu().screen(), SCREEN_STATIC);  // 0.5 s burst when play starts
    m.run_frames(kSecond / 2 + 1);
    CHECK_EQ(m.gpu().screen(), SCREEN_TAPE);

    m.run_frames(3 * kSecond);
    CHECK(!osd_has(m, "PLAY"));  // label times out after 3 s

    m.tap(BTN_PLAY);  // play/pause toggle
    m.run_frames(2);
    CHECK(m.deck().state() == DeckState::Paused);
    const uint32_t pos = m.deck().position();
    m.run_frames(5 * kSecond);
    CHECK_EQ(m.deck().position(), pos);
    CHECK(osd_has(m, "PAUSE \x13"));  // pause label stays up
    CHECK(osd_has(m, "0:00:0"));

    m.tap(BTN_PLAY);
    m.run_frames(2);
    CHECK(m.deck().state() == DeckState::Playing);
    CHECK_EQ(m.gpu().screen(), SCREEN_TAPE);  // resuming from pause: no static

    m.tap(BTN_STOP);
    m.run_frames(2);
    CHECK(m.deck().state() == DeckState::Stopped);
    CHECK(osd_has(m, "STOP \x12"));
    CHECK_EQ(m.gpu().screen(), SCREEN_BLUE);
}

TEST(firmware_counter_tracks_position) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape(3 * 3600));
    m.run_frames(2 * kSecond);
    m.deck().seek(static_cast<uint32_t>((3600 + 23 * 60 + 45) * Machine::kFps));
    m.tap(BTN_PLAY);
    m.run_frames(2);
    CHECK(osd_has(m, "1:23:45"));
}

TEST(firmware_rewind_and_fast_forward) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape(600));
    m.run_frames(2 * kSecond);

    m.tap(BTN_FF);
    m.run_frames(2);
    CHECK(m.deck().state() == DeckState::FastForward);
    CHECK(osd_has(m, "FF \x10\x10"));
    m.run_frames(5 * kSecond);
    const uint32_t after_ff = m.deck().position();
    CHECK(after_ff > 5u * kSecond * 10u);  // much faster than real time
    CHECK(osd_has(m, "FF"));               // FF label is persistent

    m.tap(BTN_FF);  // second press -> play
    m.run_frames(2);
    CHECK(m.deck().state() == DeckState::Playing);

    m.tap(BTN_REW);
    m.run_frames(2);
    CHECK(m.deck().state() == DeckState::Rewinding);
    CHECK(osd_has(m, "REW \x11\x11"));
    for (int i = 0; i < 30 * kSecond && m.deck().state() == DeckState::Rewinding; ++i) m.run_frame();
    m.run_frames(2);
    CHECK_EQ(m.deck().position(), 0u);
    CHECK(m.deck().state() == DeckState::Stopped);  // auto-stop at BOT
    CHECK(osd_has(m, "STOP"));
}

TEST(firmware_auto_rewind_at_end_of_tape) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape(10));
    m.run_frames(2 * kSecond);
    m.tap(BTN_PLAY);
    m.run_frames(11 * kSecond);
    CHECK(m.deck().state() == DeckState::Rewinding || m.deck().state() == DeckState::Stopped);
    m.run_frames(5 * kSecond);
    CHECK(m.deck().state() == DeckState::Stopped);
    CHECK_EQ(m.deck().position(), 0u);
}

TEST(firmware_eject) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape());
    m.run_frames(2 * kSecond);
    m.tap(BTN_PLAY);
    m.run_frames(kSecond);
    m.tap(BTN_EJECT);
    m.run_frames(2);
    CHECK(!m.deck().has_tape());
    CHECK(osd_has(m, "EJECT"));
    CHECK_EQ(m.gpu().screen(), SCREEN_STATIC);
    m.run_frames(3 * kSecond);
    CHECK_EQ(m.gpu().screen(), SCREEN_STATIC);  // still no tape
    // Transport buttons can't do anything without a tape - the OSD says so.
    m.run_frames(2 * kSecond);
    m.tap(BTN_PLAY);
    m.run_frames(2);
    CHECK(!osd_has(m, "PLAY"));
    CHECK(osd_has(m, "NO TAPE"));
    CHECK(m.deck().state() == DeckState::Empty);
}

TEST(firmware_tracking_button) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape());
    m.run_frames(2 * kSecond);
    m.tap(BTN_PLAY);
    m.run_frames(kSecond);
    const uint32_t before = m.gpu().tracking_requests();
    m.tap(BTN_TRACK);
    m.run_frames(2);
    CHECK_EQ(m.gpu().tracking_requests(), before + 1);
    CHECK(osd_has(m, "TRACKING"));
}

TEST(firmware_power_off_and_on) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape());
    m.run_frames(2 * kSecond);
    m.tap(BTN_PLAY);
    m.run_frames(kSecond);
    m.tap(BTN_POWER);
    m.run_frames(2);
    CHECK(!m.power_led());
    CHECK_EQ(m.gpu().screen(), SCREEN_OFF);
    CHECK(m.deck().state() == DeckState::Stopped);
    CHECK_EQ(m.gpu().osd_text(), std::string());
    m.tap(BTN_PLAY);  // ignored in standby
    m.run_frames(2);
    CHECK(m.deck().state() == DeckState::Stopped);
    m.tap(BTN_POWER);
    m.run_frames(2);
    CHECK(m.power_led());
    CHECK_EQ(m.gpu().screen(), SCREEN_STATIC);
    m.run_frames(2 * kSecond);
    CHECK_EQ(m.gpu().screen(), SCREEN_BLUE);  // tape still in, stopped
}

TEST(firmware_uses_little_cpu_and_never_faults) {
    Machine m;
    power_up(m);
    m.deck().insert(test_tape());
    const uint8_t buttons[] = {BTN_PLAY, BTN_FF, BTN_REW, BTN_PLAY, BTN_TRACK, BTN_STOP, BTN_PLAY, BTN_PLAY};
    for (uint8_t b : buttons) {
        m.tap(b);
        m.run_frames(kSecond);
    }
    CHECK(m.cpu().fault() == CpuFault::None);
    CHECK_EQ(m.cpu().regs().sp, memmap::STACK_RESET);  // balanced stack at WAI
    m.cpu().run(20000);  // one frame's work, then back to sleep in WAI
    CHECK(m.cpu().waiting());
    // The firmware should spend most of its time asleep in WAI.
    CHECK(m.cpu().busy_cycles() * 10 < m.cpu().total_cycles());
}

TEST(machine_realtime_advance_hits_ntsc_rate) {
    Machine m;
    boot(m);
    int frames = 0;
    for (int i = 0; i < 1000; ++i) frames += m.advance(0.01);  // 10 s in 10 ms steps
    CHECK(frames >= 299 && frames <= 300);  // 29.97 fps
    CHECK_EQ(static_cast<uint64_t>(frames), m.frame());
    const int stall = m.advance(10.0);  // big stalls are clamped to 250 ms
    CHECK(stall >= 7 && stall <= 8);
}

TEST(machine_osd_is_never_seen_half_drawn) {
    // Advance in tiny real-time slices so the host samples the GPU in the
    // middle of the firmware's draw routine; the latched OSD must still only
    // ever show complete strings.
    Machine m;
    boot(m);
    m.deck().insert(test_tape());
    bool saw_play = false;
    for (int i = 0; i < 4000; ++i) {
        if (i == 2500) m.tap(BTN_PLAY);
        m.advance(0.0007);
        const std::string osd = m.gpu().osd_text();
        CHECK(osd.empty() || osd == "NO SIGNAL" || osd.rfind("PLAY \x10 | 0:00:0", 0) == 0);
        saw_play |= !osd.empty() && osd[0] == 'P';
    }
    CHECK(saw_play);
}
