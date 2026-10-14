// Phase 3 tests: RAM bounds, ring buffer, VRAM, frame/audio timing, the tape
// streamer, static, interlacing and the TvSet video pipeline.
#include <cmath>
#include <thread>

#include "app/tv_set.h"
#include "av/av_sync.h"
#include "av/interlace.h"
#include "av/static_noise.h"
#include "av/timing.h"
#include "core/ram.h"
#include "core/ring_buffer.h"
#include "gpu/vram.h"
#include "tape/tape_streamer.h"
#include "test.h"
#include "vcr/io_ports.h"

using namespace rt90;
using namespace rt90::io;

namespace {

double rms(const int16_t* s, size_t n) {
    double acc = 0;
    for (size_t i = 0; i < n; ++i) acc += double(s[i]) * s[i];
    return n ? std::sqrt(acc / double(n)) / 32768.0 : 0.0;
}

double mean_luma(const Framebuffer& fb) {
    double acc = 0;
    for (uint32_t p : fb.px) acc += luma(p);
    return acc / double(fb.px.size());
}

// Counts reads so tests can see whether frames came from the stream buffer.
struct CountingSource : TestPatternSource {
    std::atomic<int> reads{0};
    explicit CountingSource(double s) : TestPatternSource(s) {}
    bool read_frame(uint32_t index, TapeFrame& out) override {
        ++reads;
        return TestPatternSource::read_frame(index, out);
    }
};

}  // namespace

// ---- RAM -----------------------------------------------------------------

TEST(ram_is_64k_and_bounds_checked) {
    Ram ram;
    CHECK_EQ(ram.size(), 65536u);
    CHECK(ram.write(0, 1));
    CHECK(ram.write(0xFFFF, 2));
    CHECK_EQ(ram.read(0xFFFF), 2);
    CHECK(!ram.write(0x10000, 3));
    CHECK_EQ(ram.read(0x10000), 0xFF);
    CHECK_EQ(ram.read(static_cast<size_t>(-1)), 0xFF);
    CHECK_EQ(ram.violations(), 3u);
}

TEST(ram_load_is_all_or_nothing) {
    Ram ram;
    const uint8_t data[4] = {1, 2, 3, 4};
    CHECK(ram.load(0xFFFC, data, 4));
    CHECK_EQ(ram.read(0xFFFF), 4);
    CHECK(!ram.load(0xFFFD, data, 4));          // would run past the end
    CHECK_EQ(ram.read(0xFFFD), 2);               // untouched
    CHECK(!ram.load(0x20000, data, 1));
    CHECK(!ram.load(10, data, static_cast<size_t>(-1)));  // overflow-safe size check
}

TEST(ram_tracks_usage) {
    Ram ram;
    ram.write(0x8000, 1);
    ram.write(0x8000, 2);
    ram.write(0x8005, 3);
    CHECK_EQ(ram.bytes_used(0x8000, 0x9000), 2u);
    ram.clear(0x8000, 0x8001);
    CHECK_EQ(ram.bytes_used(0x8000, 0x9000), 1u);
    CHECK_EQ(ram.read(0x8000), 0);
    ram.clear(0x8000, 0x20000);  // clamped, no crash
}

TEST(ram_firmware_usage_is_small) {
    Machine m;
    CHECK(m.boot_default());
    m.run_frames(120);
    const size_t work = m.bus().ram().bytes_used(memmap::RAM_START, memmap::RAM_END + 1u);
    const size_t stack = m.bus().ram().bytes_used(memmap::STACK_START, memmap::STACK_END + 1u);
    CHECK(work > 0 && work < 64);
    CHECK(stack > 0 && stack <= 16);
    CHECK_EQ(m.bus().ram().violations(), 0u);
}

// ---- ring buffer --------------------------------------------------------------

TEST(ring_buffer_basic_and_wraparound) {
    RingBuffer rb(10);
    CHECK_EQ(rb.capacity(), 16u);
    uint8_t buf[16];
    for (int round = 0; round < 10; ++round) {
        const uint8_t data[6] = {uint8_t(round), 1, 2, 3, 4, 5};
        CHECK(rb.write(data, 6));
        CHECK(rb.write(data, 6));
        CHECK(!rb.write(data, 6));  // only 4 bytes free: all or nothing
        CHECK_EQ(rb.size(), 12u);
        CHECK(rb.peek(buf, 1));
        CHECK_EQ(buf[0], uint8_t(round));
        CHECK(rb.read(buf, 6));
        CHECK(rb.read(buf, 6));
        CHECK_EQ(buf[0], uint8_t(round));
        CHECK_EQ(buf[5], 5);
        CHECK(!rb.read(buf, 1));
    }
    rb.write(buf, 5);
    rb.skip(100);
    CHECK_EQ(rb.size(), 0u);
}

TEST(ring_buffer_threaded_stream) {
    RingBuffer rb(4096);
    constexpr uint32_t kCount = 200000;
    std::thread producer([&] {
        for (uint32_t i = 0; i < kCount;) {
            if (rb.write(&i, sizeof i)) ++i;
            else std::this_thread::yield();
        }
    });
    bool ok = true;
    for (uint32_t expect = 0; expect < kCount;) {
        uint32_t v;
        if (rb.read(&v, sizeof v)) {
            ok &= v == expect;
            ++expect;
        } else {
            std::this_thread::yield();
        }
    }
    producer.join();
    CHECK(ok);
}

// ---- VRAM and timing -----------------------------------------------------------

TEST(vram_double_buffers) {
    Vram v;
    CHECK_EQ(v.front_frame(), Vram::kNone);
    v.back().fill(rgb(1, 2, 3));
    CHECK(v.front().at(0, 0) != rgb(1, 2, 3));
    v.swap(7);
    CHECK_EQ(v.front().at(0, 0), rgb(1, 2, 3));
    CHECK_EQ(v.front_frame(), 7u);
    v.back().fill(rgb(4, 5, 6));
    CHECK_EQ(v.front().at(0, 0), rgb(1, 2, 3));  // back writes don't tear the front
    CHECK_EQ(Vram::kBytes, 2u * 320u * 240u * 4u);
}

TEST(timing_audio_is_locked_to_ntsc_frames) {
    CHECK(std::fabs(kNtscFps - 29.97002997) < 1e-6);
    // 30000 frames are exactly 1001 seconds.
    CHECK_EQ(audio_sample_at(30000), 1001u * 22050u);
    uint64_t total = 0;
    for (uint64_t f = 0; f < 30000; ++f) {
        const uint32_t n = audio_samples_in_frame(f);
        CHECK(n == 735 || n == 736);
        total += n;
    }
    CHECK_EQ(total, 1001u * 22050u);
}

// ---- tape streamer --------------------------------------------------------------

TEST(streamer_reads_ahead_while_playing) {
    auto src = std::make_shared<CountingSource>(60);
    TapeStreamer st(4u << 20);
    st.attach(src);
    TapeFrame f;
    CHECK(st.fetch(0, 1, f));
    CHECK_EQ(f.index, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // let it fill
    CHECK(st.fill_ratio() > 0.5);
    const uint64_t hits_before = st.hits();
    for (uint32_t i = 1; i < 10; ++i) {
        CHECK(st.fetch(i, 1, f));
        CHECK_EQ(f.index, i);
        CHECK_EQ(f.audio.size(), size_t(audio_samples_in_frame(i)));
    }
    CHECK(st.hits() >= hits_before + 8);
    st.detach();
    CHECK(!st.attached());
}

TEST(streamer_follows_fast_forward_and_rewind) {
    auto src = std::make_shared<TestPatternSource>(120);
    TapeStreamer st(4u << 20);
    st.attach(src);
    TapeFrame f;
    for (uint32_t i = 100; i < 100 + 12 * 30; i += 12) {
        CHECK(st.fetch(i, 12, f));
        CHECK_EQ(f.index, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(st.hits() > 10);
    for (int64_t i = 2000; i >= 0; i -= 12) {
        CHECK(st.fetch(uint32_t(i), -12, f));
        CHECK_EQ(f.index, uint32_t(i));
    }
    // Random seeks always return the right frame.
    for (uint32_t i : {5u, 3000u, 17u, 2500u}) {
        CHECK(st.fetch(i, 1, f));
        CHECK_EQ(f.index, i);
    }
    CHECK(!st.fetch(999999, 1, f));  // past the end
}

// ---- static & interlace --------------------------------------------------------------

TEST(snow_is_animated_grey_noise) {
    SnowPass snow;
    Framebuffer a(kScreenW, kScreenH), b(kScreenW, kScreenH);
    snow.apply(a, {1, SCREEN_STATIC});
    snow.apply(b, {2, SCREEN_STATIC});
    CHECK(a.px != b.px);
    const double m = mean_luma(a);
    CHECK(m > 80 && m < 170);
    int colourful = 0;
    for (uint32_t p : a.px) colourful += std::abs(red(p) - blue(p)) > 40;
    CHECK(colourful < int(a.px.size() / 100));  // mostly monochrome
}

TEST(hiss_level_matches_request) {
    HissGenerator hiss;
    std::vector<float> buf(22050, 0.0f);
    hiss.mix(buf.data(), buf.size(), 0.05f);
    double acc = 0;
    for (float v : buf) acc += double(v) * v;
    const double level = std::sqrt(acc / double(buf.size()));
    CHECK(level > 0.04 && level < 0.06);
}

TEST(interlace_combs_motion_when_enabled) {
    InterlacePass il;
    Framebuffer fb(kScreenW, kScreenH, rgb(0, 0, 0));
    il.apply(fb, {1, SCREEN_TAPE});  // disabled: untouched
    CHECK_EQ(fb.at(0, 1), rgb(0, 0, 0));
    il.enabled = true;
    il.apply(fb, {1, SCREEN_TAPE});
    fb.fill(rgb(200, 200, 200));
    il.apply(fb, {2, SCREEN_TAPE});  // field 0 refreshed, odd lines keep the old frame
    CHECK_EQ(fb.at(10, 0), rgb(200, 200, 200));
    CHECK_EQ(fb.at(10, 1), rgb(0, 0, 0));
}

// ---- TvSet: the whole pipeline ---------------------------------------------------------

TEST(tvset_static_at_power_on_with_hiss) {
    TvSet tv;
    CHECK(tv.boot());
    tv.run_frames(10);
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_STATIC);
    std::vector<int16_t> audio;
    tv.take_audio(audio);
    CHECK(audio.size() >= 7350 - 1 && audio.size() <= 7360);
    CHECK(rms(audio.data(), audio.size()) > 0.02);
    CHECK(rms(audio.data(), audio.size()) < 0.15);
}

TEST(tvset_plays_test_tape_in_sync) {
    TvSet tv;
    CHECK(tv.boot());
    tv.run_frames(60);
    tv.insert_tape(std::make_shared<TestPatternSource>(600));
    tv.run_frames(2);
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_STATIC);  // switching tapes
    tv.run_frames(40);
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_BLUE);
    std::vector<int16_t> audio;
    tv.take_audio(audio);
    audio.clear();

    tv.machine().tap(BTN_PLAY);
    tv.run_frames(5);
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_STATIC);  // 0.5 s burst
    tv.run_frames(20);
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_TAPE);
    tv.take_audio(audio);
    audio.clear();

    // 10 seconds of playback: every frame shows the tape frame under the head
    // and produces exactly that frame's audio.
    uint64_t samples = 0;
    for (int i = 0; i < 300; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));  // give the streamer some time
        tv.run_frames(1);
        CHECK_EQ(tv.shown_tape_frame(), tv.machine().deck().position());
        tv.take_audio(audio);
        CHECK_EQ(audio.size(), size_t(audio_samples_in_frame(tv.shown_tape_frame())));
        samples += audio.size();
        if (i == 150) {  // 1 kHz tone at -20 dBFS
            const double level = rms(audio.data(), audio.size());
            CHECK(level > 0.06 && level < 0.08);
        }
        audio.clear();
    }
    CHECK(samples >= 220500 - 300 && samples <= 220500 + 300);
    std::printf("    streamer: %llu hits, %llu misses\n", (unsigned long long)tv.streamer().hits(),
                (unsigned long long)tv.streamer().misses());
    CHECK(tv.streamer().hits() > 250);  // served from the read-ahead buffer
}

TEST(tvset_pause_holds_frame_and_goes_quiet) {
    TvSet tv;
    CHECK(tv.boot());
    tv.insert_tape(std::make_shared<TestPatternSource>(600));
    tv.run_frames(60);
    tv.machine().tap(BTN_PLAY);
    tv.run_frames(60);
    tv.machine().tap(BTN_PLAY);  // pause
    tv.run_frames(2);
    const uint32_t f = tv.shown_tape_frame();
    std::vector<int16_t> audio;
    tv.take_audio(audio);
    audio.clear();
    tv.run_frames(30);
    CHECK_EQ(tv.shown_tape_frame(), f);
    tv.take_audio(audio);
    CHECK(rms(audio.data(), audio.size()) < 0.001);
}

TEST(tvset_fast_forward_and_rewind_show_picture) {
    TvSet tv;
    CHECK(tv.boot());
    tv.insert_tape(std::make_shared<TestPatternSource>(600));
    tv.run_frames(60);
    tv.machine().tap(BTN_FF);
    tv.run_frames(30);
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_TAPE);
    const uint32_t ff = tv.shown_tape_frame();
    CHECK(ff > 300);
    tv.machine().tap(BTN_REW);
    tv.run_frames(10);
    CHECK(tv.shown_tape_frame() < ff);
}

TEST(tvset_eject_returns_to_static) {
    TvSet tv;
    CHECK(tv.boot());
    tv.insert_tape(std::make_shared<TestPatternSource>(600));
    tv.run_frames(60);
    tv.machine().tap(BTN_PLAY);
    tv.run_frames(60);
    tv.machine().tap(BTN_EJECT);
    tv.run_frames(5);
    CHECK(!tv.has_tape());
    CHECK(!tv.streamer().attached());
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_STATIC);
    // Switching to another tape works.
    tv.insert_tape(std::make_shared<TestPatternSource>(30));
    tv.run_frames(60);
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_BLUE);
}

// ---- A/V sync controller -------------------------------------------------------------

TEST(avsync_holds_latency_against_clock_drift) {
    // Simulate 10 minutes with a sound card that runs 500 ppm fast: without
    // correction the audio would drift ~0.3 s (9 frames) ahead.
    for (double ppm : {500.0, -500.0}) {
        AvSync sync(kAudioRate);
        double queued = sync.target_samples();
        double min_q = queued, max_q = queued;
        const double wall_dt = 1.0 / 60.0;
        for (int i = 0; i < 60 * 600; ++i) {
            const double emu_dt = sync.adjust(wall_dt, static_cast<size_t>(queued));
            queued += emu_dt * kAudioRate;                         // produced by emulation
            queued -= wall_dt * kAudioRate * (1.0 + ppm * 1e-6);  // consumed by the device
            if (i > 60 * 30) {  // after settling
                min_q = std::min(min_q, queued);
                max_q = std::max(max_q, queued);
            }
        }
        const double frame_samples = kAudioRate / kNtscFps;
        CHECK(max_q - min_q < frame_samples);  // latency varies by < 1 frame
        CHECK(std::fabs(queued - sync.target_samples()) < frame_samples);
    }
}
