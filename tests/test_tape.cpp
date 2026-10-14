// Phase 5 tests: the .vhs format, file detection and MP4 import.
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <thread>

#include "app/tv_set.h"
#include "av/timing.h"
#include "gpu/test_pattern.h"
#include "tape/file_detect.h"
#include "tape/importer.h"
#include "tape/jpeg.h"
#include "tape/vhs_format.h"
#include "test.h"
#include "util/file_util.h"
#include "vcr/io_ports.h"

using namespace rt90;
using namespace rt90::io;

namespace {

const std::string kMedia = RT90_TEST_MEDIA_DIR;
const std::string kTmp = RT90_TEST_TMP_DIR;

std::string tmp(const std::string& name) {
    make_dirs(kTmp);
    return kTmp + "/" + name;
}

void write_bytes(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = fopen_utf8(path, "wb");
    if (!data.empty()) std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
}

std::vector<uint8_t> read_bytes(const std::string& path) {
    std::vector<uint8_t> out;
    FILE* f = fopen_utf8(path, "rb");
    if (!f) return out;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return out;
}

void copy_file(const std::string& from, const std::string& to) { write_bytes(to, read_bytes(from)); }

// Writes a small synthetic tape: test-pattern frames with a ramp in the audio.
bool write_test_tape(const std::string& path, uint32_t frames, std::string title = "TEST") {
    VhsWriter w;
    if (!w.open(path)) return false;
    Framebuffer fb(kScreenW, kScreenH);
    std::vector<uint8_t> jpeg;
    for (uint32_t i = 0; i < frames; ++i) {
        draw_test_pattern(fb, i);
        encode_jpeg(fb, 60, jpeg);
        std::vector<int16_t> audio(audio_samples_in_frame(i));
        for (size_t k = 0; k < audio.size(); ++k) audio[k] = static_cast<int16_t>(i * 100 + k);
        if (!w.add_frame(jpeg, audio.data(), audio.size())) return false;
    }
    VhsHeader h;
    h.title = std::move(title);
    return w.finish(h);
}

double frame_luma(const Framebuffer& fb, int y0, int y1) {
    double acc = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < fb.w; ++x) acc += luma(fb.at(x, y));
    return acc / double((y1 - y0) * fb.w);
}

}  // namespace

// ---- format --------------------------------------------------------------------

TEST(vhs_header_round_trip) {
    VhsHeader h;
    h.frame_count = h.index_count = 1234;
    h.duration_us = 41174133;
    h.index_offset = 999999;
    h.title = "Night of the Living Tape";
    h.source_size = 123456789;
    h.source_mtime = 1700000000;
    uint8_t buf[VhsHeader::kSize];
    h.serialize(buf);
    CHECK(std::memcmp(buf, "VHS1", 4) == 0);
    VhsHeader p;
    std::string err;
    CHECK(p.parse(buf, &err));
    CHECK_EQ(p.frame_count, 1234u);
    CHECK_EQ(p.duration_us, 41174133u);
    CHECK_EQ(p.index_offset, 999999u);
    CHECK_EQ(p.title, h.title);
    CHECK_EQ(p.width, 320);
    CHECK_EQ(p.height, 240);
    CHECK_EQ(p.fps_num, 30000u);
    CHECK_EQ(p.fps_den, 1001u);
    CHECK_EQ(p.audio_rate, 22050u);
    CHECK_EQ(p.source_mtime, int64_t{1700000000});

    buf[0] = 'X';
    CHECK(!p.parse(buf, &err));
    CHECK(err.find("not a VHS1") != std::string::npos);
    h.serialize(buf);
    buf[4] = 9;  // version 9
    CHECK(!p.parse(buf, &err));
}

TEST(vhs_title_is_truncated_on_utf8_boundary) {
    VhsHeader h;
    std::string t;
    for (int i = 0; i < 100; ++i) t += "\xC3\xA9";  // 'é' x100 = 200 bytes
    h.title = t;
    uint8_t buf[VhsHeader::kSize];
    h.serialize(buf);
    VhsHeader p;
    CHECK(p.parse(buf, nullptr));
    CHECK(p.title.size() <= 127);
    CHECK_EQ(p.title.size() % 2, 0u);  // no half character
}

TEST(vhs_write_read_round_trip) {
    const std::string path = tmp("roundtrip.vhs");
    CHECK(write_test_tape(path, 90, "ROUND TRIP"));
    CHECK(!file_exists(path + ".part"));
    VhsReader r;
    std::string err;
    CHECK(r.open(path, &err));
    CHECK_EQ(r.header().frame_count, 90u);
    CHECK_EQ(r.header().title, std::string("ROUND TRIP"));
    CHECK_EQ(r.index().size(), 90u);
    CHECK(r.header().duration_us > 3000000 && r.header().duration_us < 3010000);
    // Random access in any order (seeking), with matching audio and pictures.
    for (uint32_t i : {0u, 89u, 45u, 1u, 60u}) {
        TapeFrame f;
        CHECK(r.read_frame(i, f));
        CHECK_EQ(f.index, i);
        CHECK(f.codec == VideoCodec::Jpeg);
        CHECK_EQ(f.audio.size(), size_t(audio_samples_in_frame(i)));
        CHECK_EQ(f.audio[3], int16_t(i * 100 + 3));
        Framebuffer pic;
        CHECK(decode_video(f, pic));
        Framebuffer ref(kScreenW, kScreenH);
        draw_test_pattern(ref, i);
        CHECK(std::fabs(frame_luma(pic, 0, 240) - frame_luma(ref, 0, 240)) < 4.0);  // JPEG q60 is close
    }
    TapeFrame f;
    CHECK(!r.read_frame(90, f));
}

TEST(vhs_jpeg_frames_are_compact) {
    const std::string path = tmp("size.vhs");
    CHECK(write_test_tape(path, 30));
    VhsReader r;
    CHECK(r.open(path));
    for (const VhsIndexEntry& e : r.index()) CHECK(e.video_bytes < 40000u);  // vs 230 KB raw
}

TEST(vhs_writer_abort_leaves_nothing) {
    const std::string path = tmp("aborted.vhs");
    remove_file(path);
    {
        VhsWriter w;
        CHECK(w.open(path));
        std::vector<uint8_t> jpeg(100, 1);
        int16_t a[735] = {};
        CHECK(w.add_frame(jpeg, a, 735));
        w.abort();
    }
    CHECK(!file_exists(path));
    CHECK(!file_exists(path + ".part"));
}

TEST(vhs_reader_rejects_damaged_tapes) {
    const std::string good = tmp("good.vhs");
    CHECK(write_test_tape(good, 20));
    std::vector<uint8_t> bytes = read_bytes(good);
    VhsReader r;
    std::string err;

    std::vector<uint8_t> cut(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(bytes.size() / 2));
    write_bytes(tmp("cut.vhs"), cut);
    CHECK(!r.open(tmp("cut.vhs"), &err));
    CHECK(err.find("index") != std::string::npos);

    std::vector<uint8_t> bad_index = bytes;
    bad_index[bad_index.size() - 16] ^= 0xFF;  // corrupt the last index entry's offset
    write_bytes(tmp("badindex.vhs"), bad_index);
    CHECK(!r.open(tmp("badindex.vhs"), &err));

    write_bytes(tmp("tiny.vhs"), {'V', 'H', 'S', '1'});
    CHECK(!r.open(tmp("tiny.vhs"), &err));
    CHECK(!r.open(tmp("does-not-exist.vhs"), &err));

    // A damaged chunk is caught at read time instead of crashing.
    std::vector<uint8_t> bad_chunk = bytes;
    bad_chunk[VhsHeader::kSize] = 'X';  // first VIDF tag
    write_bytes(tmp("badchunk.vhs"), bad_chunk);
    CHECK(r.open(tmp("badchunk.vhs"), &err));
    TapeFrame f;
    CHECK(!r.read_frame(0, f));
    CHECK(r.read_frame(1, f));
}

TEST(vhs_tape_plays_in_tvset) {
    const std::string path = tmp("play.vhs");
    CHECK(write_test_tape(path, 300));
    TvSet tv;
    CHECK(tv.boot());
    auto src = VhsSource::open(path);
    CHECK(src != nullptr);
    tv.insert_tape(src);
    tv.run_frames(50);
    tv.machine().tap(BTN_PLAY);
    tv.run_frames(60);
    CHECK_EQ(tv.machine().gpu().screen(), SCREEN_TAPE);
    CHECK(tv.shown_tape_frame() > 30 && tv.shown_tape_frame() < 70);
    CHECK(tv.machine().gpu().osd_text().find("PLAY") != std::string::npos);
}

// ---- detection --------------------------------------------------------------------

TEST(detect_mp4_by_magic_bytes) {
    const std::string mp4 = kMedia + "/sample_av.mp4";
    CHECK(detect_file(mp4).kind == FileKind::Mp4);
    // Renamed: still an MP4.
    copy_file(mp4, tmp("renamed.txt"));
    CHECK(detect_file(tmp("renamed.txt")).kind == FileKind::Mp4);
    copy_file(mp4, tmp("noext"));
    CHECK(detect_file(tmp("noext")).kind == FileKind::Mp4);
}

TEST(detect_vhs_even_if_named_mp4) {
    CHECK(write_test_tape(tmp("tape-named.mp4"), 5));
    CHECK(detect_file(tmp("tape-named.mp4")).kind == FileKind::Vhs);
}

TEST(detect_empty_missing_and_other_files) {
    write_bytes(tmp("empty.mp4"), {});
    Detection d = detect_file(tmp("empty.mp4"));
    CHECK(d.kind == FileKind::Empty);
    CHECK(!d.reason.empty());
    CHECK(detect_file(tmp("missing.mp4")).kind == FileKind::Missing);
    const std::string text = "Hello, this is not a movie.";
    write_bytes(tmp("notes.mp4"), std::vector<uint8_t>(text.begin(), text.end()));
    CHECK(detect_file(tmp("notes.mp4")).kind == FileKind::Unknown);
    write_bytes(tmp("png.mp4"), {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 0x0D});
    CHECK(detect_file(tmp("png.mp4")).kind == FileKind::Unknown);
}

TEST(detect_corrupt_mp4_headers) {
    // Too short to hold a brand.
    write_bytes(tmp("short.mp4"), {0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'i', 's'});
    CHECK(detect_file(tmp("short.mp4")).kind == FileKind::Unknown);
    // Box size larger than the file.
    write_bytes(tmp("bigbox.mp4"), {0x7F, 0, 0, 0, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm', 0, 0, 0, 0});
    CHECK(detect_file(tmp("bigbox.mp4")).kind == FileKind::Unknown);
    // Garbage brand.
    write_bytes(tmp("brand.mp4"), {0, 0, 0, 0x10, 'f', 't', 'y', 'p', 0x01, 0x02, 0x03, 0x04, 0, 0, 0, 0});
    CHECK(detect_file(tmp("brand.mp4")).kind == FileKind::Unknown);
    CHECK(detect_bytes(nullptr, 0, 0).kind == FileKind::Empty);
}

// ---- import -------------------------------------------------------------------------

TEST(import_converts_mp4_to_vhs) {
    if (!importer_available()) return;
    const std::string mp4 = tmp("movie.mp4");
    copy_file(kMedia + "/sample_av.mp4", mp4);
    remove_file(tmp("movie.vhs"));
    std::atomic<float> progress{0};
    ImportOptions opt;
    opt.reuse_existing = false;
    ImportResult r = import_mp4(mp4, opt, nullptr, &progress);
    CHECK(r.ok);
    if (!r.ok) std::fprintf(stderr, "    import error: %s\n", r.error.c_str());
    CHECK_EQ(r.vhs_path, tmp("movie.vhs"));  // saved next to the original
    CHECK_EQ(r.title, std::string("RetroTube Sample"));
    CHECK(r.frames >= 298 && r.frames <= 301);  // 10 s at 29.97 fps
    CHECK_EQ(progress.load(), 1.0f);

    VhsReader rd;
    CHECK(rd.open(r.vhs_path));
    CHECK_EQ(rd.header().jpeg_quality, 60);
    TapeFrame f;
    Framebuffer pic;
    CHECK(rd.read_frame(150, f));
    CHECK(decode_video(f, pic));
    // 16:9 source is letterboxed into the 4:3 tape: black bars top and bottom.
    CHECK(frame_luma(pic, 0, 25) < 3.0);
    CHECK(frame_luma(pic, 216, 240) < 3.0);
    CHECK(frame_luma(pic, 40, 200) > 30.0);
    // Audio is there (the sample has beeps).
    int16_t peak = 0;
    for (uint32_t i = 0; i < rd.header().frame_count; ++i) {
        rd.read_frame(i, f);
        for (int16_t s : f.audio) peak = std::max<int16_t>(peak, static_cast<int16_t>(std::abs(s)));
    }
    CHECK(peak > 5000);
}

TEST(import_keeps_audio_and_video_aligned) {
    if (!importer_available()) return;
    const std::string mp4 = tmp("sync.mp4");
    copy_file(kMedia + "/sample_av.mp4", mp4);
    ImportOptions opt;
    opt.reuse_existing = false;
    ImportResult r = import_mp4(mp4, opt);
    CHECK(r.ok);
    VhsReader rd;
    CHECK(rd.open(r.vhs_path));
    // Every second the source flashes white and beeps at the same moment.
    // Find both on the tape and compare their times.
    std::vector<double> flashes, beeps;
    TapeFrame f;
    Framebuffer pic;
    bool prev_bright = false, prev_loud = false;
    uint64_t sample_pos = 0;
    for (uint32_t i = 0; i < rd.header().frame_count; ++i) {
        rd.read_frame(i, f);
        decode_video(f, pic);
        const bool bright = frame_luma(pic, 40, 200) > 200;
        if (bright && !prev_bright) flashes.push_back(i / kNtscFps);
        prev_bright = bright;
        for (size_t k = 0; k < f.audio.size(); ++k) {
            const bool loud = std::abs(f.audio[k]) > 4000;
            if (loud && !prev_loud && (beeps.empty() || (double(sample_pos + k) / kAudioRate) - beeps.back() > 0.5))
                beeps.push_back(double(sample_pos + k) / kAudioRate);
            prev_loud = loud;
        }
        sample_pos += f.audio.size();
    }
    CHECK(flashes.size() >= 9);
    CHECK_EQ(flashes.size(), beeps.size());
    double worst = 0;
    for (size_t i = 0; i < std::min(flashes.size(), beeps.size()); ++i)
        worst = std::max(worst, std::fabs(flashes[i] - beeps[i]));
    std::printf("    worst A/V offset: %.1f ms\n", worst * 1000);
    CHECK(worst < 1.0 / kNtscFps);  // less than one frame
}

TEST(import_reuses_up_to_date_tape) {
    if (!importer_available()) return;
    const std::string mp4 = tmp("reuse.mp4");
    copy_file(kMedia + "/sample_av.mp4", mp4);
    ImportOptions opt;
    opt.reuse_existing = false;
    CHECK(import_mp4(mp4, opt).ok);
    ImportResult again = import_mp4(mp4);
    CHECK(again.ok);
    CHECK(again.reused);
    CHECK_EQ(again.vhs_path, tmp("reuse.vhs"));
}

TEST(import_can_be_cancelled) {
    if (!importer_available()) return;
    const std::string mp4 = tmp("cancel.mp4");
    copy_file(kMedia + "/sample_av.mp4", mp4);
    remove_file(tmp("cancel.vhs"));
    std::atomic<bool> cancel{true};
    ImportOptions opt;
    opt.reuse_existing = false;
    ImportResult r = import_mp4(mp4, opt, &cancel);
    CHECK(!r.ok);
    CHECK(r.cancelled);
    CHECK(!file_exists(tmp("cancel.vhs")));
    CHECK(!file_exists(tmp("cancel.vhs.part")));
}

TEST(import_fails_gracefully_on_corrupt_mp4) {
    if (!importer_available()) return;
    // Valid ftyp box, then garbage.
    std::vector<uint8_t> bytes = {0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm', 0, 0, 2, 0,
                                  'i', 's', 'o', 'm', 'm', 'p', '4', '1'};
    for (int i = 0; i < 5000; ++i) bytes.push_back(static_cast<uint8_t>(i * 37));
    write_bytes(tmp("corrupt.mp4"), bytes);
    CHECK(detect_file(tmp("corrupt.mp4")).kind == FileKind::Mp4);
    ImportResult r = import_mp4(tmp("corrupt.mp4"));
    CHECK(!r.ok);
    CHECK(!r.error.empty());
    CHECK(!file_exists(tmp("corrupt.vhs")));
    CHECK(!file_exists(tmp("corrupt.vhs.part")));
}

TEST(import_job_runs_in_background) {
    if (!importer_available()) return;
    const std::string mp4 = tmp("job.mp4");
    copy_file(kMedia + "/sample_av.mp4", mp4);
    ImportJob job;
    ImportOptions opt;
    opt.reuse_existing = false;
    job.start(mp4, opt);
    for (int i = 0; i < 6000 && !job.done(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(job.done());
    ImportResult r = job.take_result();
    CHECK(r.ok);
}
