// Phase 4 tests: VHS picture and audio noise stay subtle and within limits.
#include <cmath>

#include "app/tv_set.h"
#include "app/ui_overlay.h"
#include "av/vhs_audio.h"
#include "av/vhs_noise.h"
#include "gpu/gpu.h"
#include "gpu/test_pattern.h"
#include "test.h"
#include "vcr/io_ports.h"

using namespace rt90;
using namespace rt90::io;

namespace {

// A natural-looking test image: gradients plus some detail.
Framebuffer test_image() {
    Framebuffer fb(kScreenW, kScreenH);
    for (int y = 0; y < fb.h; ++y)
        for (int x = 0; x < fb.w; ++x)
            fb.at(x, y) = rgb(40 + x * 160 / fb.w, 60 + y * 140 / fb.h, 120 + ((x / 16 + y / 16) % 2) * 60);
    return fb;
}

double psnr(const Framebuffer& a, const Framebuffer& b, int y0 = 0, int y1 = -1) {
    if (y1 < 0) y1 = a.h;
    double se = 0;
    size_t n = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < a.w; ++x) {
            const uint32_t p = a.at(x, y), q = b.at(x, y);
            for (int s = 0; s < 24; s += 8) {
                const double d = double((p >> s) & 0xFF) - double((q >> s) & 0xFF);
                se += d * d;
                ++n;
            }
        }
    const double mse = se / double(n);
    return mse <= 0 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

// Average PSNR of `frames` noisy frames against the clean picture.
double noisy_psnr(float intensity, int frames = 60, int y0 = 0, int y1 = -1) {
    VhsNoisePass vhs;
    vhs.set_intensity(intensity);
    const Framebuffer clean = test_image();
    double acc = 0;
    for (int i = 0; i < frames; ++i) {
        Framebuffer f = clean;
        vhs.apply(f, {uint64_t(i), SCREEN_TAPE});
        acc += psnr(clean, f, y0, y1);
    }
    return acc / frames;
}

std::vector<int16_t> sine(double freq, double amp, size_t n, int rate = 22050) {
    std::vector<int16_t> s(n);
    for (size_t i = 0; i < n; ++i)
        s[i] = int16_t(std::lround(amp * 32767 * std::sin(2 * 3.14159265358979 * freq * double(i) / rate)));
    return s;
}

}  // namespace

TEST(noise_default_is_15_percent) {
    VhsNoisePass vhs;
    CHECK(std::fabs(vhs.intensity() - 0.15f) < 1e-6f);
    TvSet tv;
    CHECK(std::fabs(tv.noise() - 0.15f) < 1e-6f);
    CHECK(std::fabs(tv.vhs_audio().intensity() - 0.15f) < 1e-6f);
    vhs.set_intensity(2.0f);
    CHECK_EQ(vhs.intensity(), 1.0f);
    vhs.set_intensity(-1.0f);
    CHECK_EQ(vhs.intensity(), 0.0f);
}

TEST(noise_picture_stays_watchable) {
    const double p0 = noisy_psnr(0.0f), p15 = noisy_psnr(0.15f), p50 = noisy_psnr(0.5f), p100 = noisy_psnr(1.0f);
    std::printf("    PSNR: 0%%=%.1f dB  15%%=%.1f dB  50%%=%.1f dB  100%%=%.1f dB\n", p0, p15, p50, p100);
    CHECK(p0 > 28.0);    // only VHS softness / chroma bleed
    CHECK(p15 > 25.0);   // default: subtle
    CHECK(p100 > 17.0);  // even at maximum the movie is clearly watchable
    CHECK(p0 >= p15 && p15 > p50 && p50 > p100);  // the slider really scales it
}

TEST(noise_head_switching_is_at_the_bottom_edge) {
    const double middle = noisy_psnr(0.15f, 30, 100, 140);
    const double bottom = noisy_psnr(0.15f, 30, 236, 240);
    CHECK(bottom < middle - 3.0);
}

TEST(noise_tracking_band_is_occasional) {
    VhsNoisePass vhs;
    Framebuffer f = test_image();
    int with_band = 0;
    const int frames = 30 * 60;  // one minute
    for (int i = 0; i < frames; ++i) {
        vhs.apply(f, {uint64_t(i), SCREEN_TAPE});
        with_band += vhs.band_strength() > 0;
    }
    CHECK(with_band > 0);
    CHECK(with_band < frames / 2);
}

TEST(noise_tracking_button_cleans_up_briefly) {
    VhsNoisePass vhs;
    vhs.set_intensity(0.6f);
    Framebuffer f = test_image();
    vhs.start_tracking();
    CHECK(vhs.tracking_active());
    for (int i = 0; i < 20; ++i) vhs.apply(f, {uint64_t(i), SCREEN_TAPE});
    CHECK(vhs.effective() < 0.6f * 0.3f);  // clean picture
    for (int i = 0; i < VhsNoisePass::kTrackingFrames; ++i) vhs.apply(f, {uint64_t(i), SCREEN_TAPE});
    CHECK(!vhs.tracking_active());
    CHECK(std::fabs(vhs.effective() - 0.6f) < 1e-6f);  // back to normal
}

TEST(noise_tracking_button_reaches_the_noise_pass) {
    TvSet tv;
    CHECK(tv.boot());
    tv.insert_tape(std::make_shared<TestPatternSource>(60));
    tv.run_frames(60);
    tv.machine().tap(BTN_PLAY);
    tv.run_frames(30);
    CHECK(!tv.vhs_noise().tracking_active());
    tv.machine().tap(BTN_TRACK);
    tv.run_frames(2);
    CHECK(tv.vhs_noise().tracking_active());
}

TEST(noise_audio_hiss_is_30db_below_programme) {
    VhsAudio a(22050);
    CHECK(std::fabs(a.hiss_db() + 30.0f) < 0.01f);
    const auto in = sine(440, 0.25, 22050 * 10);
    std::vector<float> out(in.size());
    a.process(in.data(), 22050 * 3, out.data());  // settle the level tracker
    a.reset_stats();
    a.process(in.data() + 22050 * 3, in.size() - 22050 * 3, out.data());
    const double rel_db = 20 * std::log10(a.measured_hiss_rms() / a.measured_input_rms());
    std::printf("    hiss: %.1f dB below programme\n", -rel_db);
    CHECK(rel_db > -32.0 && rel_db < -28.0);
    // The programme itself keeps its level: clearly audible.
    double acc = 0;
    for (size_t i = 0; i < in.size() - 22050 * 3; ++i) acc += double(out[i]) * out[i];
    const double out_rms = std::sqrt(acc / double(in.size() - 22050 * 3));
    CHECK(std::fabs(20 * std::log10(out_rms / a.measured_input_rms())) < 0.5);
}

TEST(noise_audio_limits) {
    VhsAudio a(22050);
    a.set_intensity(1.0f);
    CHECK(a.hiss_db() <= -18.0f);  // even at 100% the hiss stays well below the movie
    CHECK(a.wow_depth() <= 0.004f);
    a.set_intensity(0.0f);
    CHECK(std::isinf(a.hiss_db()));
    CHECK_EQ(a.wow_depth(), 0.0f);
    const auto in = sine(440, 0.25, 22050);
    std::vector<float> out(in.size());
    a.process(in.data(), in.size(), out.data());
    CHECK_EQ(a.measured_hiss_rms(), 0.0);
}

TEST(noise_audio_has_slight_wow_and_flutter) {
    VhsAudio a(22050);
    const size_t n = 22050 * 6;
    const auto in = sine(1000, 0.5, n);
    std::vector<float> out(n);
    a.process(in.data(), n, out.data());
    // Measure the period between rising zero crossings (interpolated).
    std::vector<double> periods;
    double last = -1;
    for (size_t i = 22050; i + 1 < n; ++i) {
        if (out[i] <= 0 && out[i + 1] > 0) {
            const double t = double(i) + out[i] / double(out[i] - out[i + 1]);
            if (last >= 0) periods.push_back(t - last);
            last = t;
        }
    }
    // Average over 20 cycles to smooth out the hiss.
    double lo = 1e9, hi = 0;
    for (size_t i = 0; i + 20 < periods.size(); i += 20) {
        double s = 0;
        for (size_t k = 0; k < 20; ++k) s += periods[i + k];
        lo = std::min(lo, s / 20);
        hi = std::max(hi, s / 20);
    }
    const double nominal = 22050.0 / 1000.0;
    const double dev = (hi - lo) / 2 / nominal;
    std::printf("    wow/flutter: +/-%.3f%%\n", dev * 100);
    CHECK(dev > 0.0002);  // audible as "tape", ...
    CHECK(dev < 0.004);   // ... but only slightly
}

TEST(noise_slider_and_screens_draw) {
    Framebuffer fb(kScreenW, kScreenH, rgb(0, 0, 0));
    draw_noise_slider(fb, 0.15f);
    int green = 0;
    for (uint32_t p : fb.px) green += p == Gpu::kOsdGreen;
    CHECK(green > 100);
    draw_recording_screen(fb, "my movie.mp4", 0.5f, 0);
    draw_not_recognized(fb, "notes.txt", "not a video file");
    CHECK_EQ(ellipsize("abcdefghij", 6), std::string("abc..."));
    CHECK_EQ(ellipsize("abc", 6), std::string("abc"));
}
