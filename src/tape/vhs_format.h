// The .vhs tape file format (version 1). All integers are little-endian.
//
//   Header (256 bytes)
//     0  char[4]  magic "VHS1"
//     4  u16      version (1)
//     6  u16      header size (256)
//     8  u16      width (320)          10  u16  height (240)
//    12  u32      fps numerator (30000) 16  u32  fps denominator (1001)
//    20  u32      audio sample rate (22050)
//    24  u16      audio channels (1)   26  u16  audio bits (16)
//    28  u32      frame count
//    32  u64      duration in microseconds
//    40  u64      index table offset
//    48  u32      index entry count (= frame count)
//    52  u8       video codec (2 = JPEG)  53  u8  JPEG quality
//    54  u16      reserved
//    56  char[128] title (UTF-8, NUL padded)
//   184  u64      source file size     192  i64  source file mtime
//   200..255      reserved (zero)
//
//   Chunks, interleaved per frame:
//     "VIDF" u32 frame u32 bytes  <JPEG data>
//     "AUDF" u32 frame u32 bytes  <mono s16 PCM, this frame's slice of audio>
//
//   Index table (for seeking, rewind and fast-forward):
//     "INDX" u32 count, then per frame: u64 VIDF offset, u32 video bytes,
//     u32 audio samples
#pragma once

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "tape/tape_source.h"

namespace rt90 {

struct VhsHeader {
    static constexpr char kMagic[5] = "VHS1";
    static constexpr uint16_t kVersion = 1;
    static constexpr uint16_t kSize = 256;
    static constexpr size_t kTitleBytes = 128;

    uint16_t version = kVersion;
    uint16_t width = 320, height = 240;
    uint32_t fps_num = 30000, fps_den = 1001;
    uint32_t audio_rate = 22050;
    uint16_t audio_channels = 1, audio_bits = 16;
    uint32_t frame_count = 0;
    uint64_t duration_us = 0;
    uint64_t index_offset = 0;
    uint32_t index_count = 0;
    uint8_t video_codec = static_cast<uint8_t>(VideoCodec::Jpeg);
    uint8_t jpeg_quality = 60;
    std::string title;
    uint64_t source_size = 0;
    int64_t source_mtime = 0;

    void serialize(uint8_t out[kSize]) const;
    // Returns false (with a reason) if the bytes are not a valid header.
    bool parse(const uint8_t in[kSize], std::string* error);
    double fps() const { return fps_den ? static_cast<double>(fps_num) / fps_den : 0.0; }
};

struct VhsIndexEntry {
    uint64_t offset = 0;  // file offset of the frame's VIDF chunk
    uint32_t video_bytes = 0;
    uint32_t audio_samples = 0;
};

// Writes a .vhs file. Frames go to "<path>.part"; finish() writes the index,
// patches the header and renames the file into place, so a cancelled or
// crashed recording never leaves a broken tape behind.
class VhsWriter {
public:
    ~VhsWriter();
    bool open(const std::string& path, std::string* error = nullptr);
    bool add_frame(const std::vector<uint8_t>& jpeg, const int16_t* audio, size_t samples);
    bool finish(VhsHeader header, std::string* error = nullptr);
    void abort();
    uint32_t frames() const { return static_cast<uint32_t>(index_.size()); }

private:
    bool put(const void* p, size_t n);
    FILE* f_ = nullptr;
    std::string path_, part_;
    std::vector<VhsIndexEntry> index_;
    bool failed_ = false;
};

// Reads a .vhs file. read_frame() is thread-safe.
class VhsReader {
public:
    ~VhsReader();
    bool open(const std::string& path, std::string* error = nullptr);
    void close();
    bool is_open() const { return f_ != nullptr; }
    const VhsHeader& header() const { return header_; }
    const std::vector<VhsIndexEntry>& index() const { return index_; }
    bool read_frame(uint32_t index, TapeFrame& out);
    const std::string& path() const { return path_; }

private:
    FILE* f_ = nullptr;
    std::string path_;
    VhsHeader header_;
    std::vector<VhsIndexEntry> index_;
    std::mutex mu_;
};

// TapeSource backed by a .vhs file.
class VhsSource : public TapeSource {
public:
    static std::shared_ptr<VhsSource> open(const std::string& path, std::string* error = nullptr);
    std::string title() const override { return reader_.header().title; }
    uint32_t frame_count() const override { return reader_.header().frame_count; }
    double fps() const override { return reader_.header().fps(); }
    int audio_rate() const override { return static_cast<int>(reader_.header().audio_rate); }
    std::string path() const override { return reader_.path(); }
    bool read_frame(uint32_t index, TapeFrame& out) override { return reader_.read_frame(index, out); }
    const VhsHeader& header() const { return reader_.header(); }

private:
    VhsReader reader_;
};

}  // namespace rt90
