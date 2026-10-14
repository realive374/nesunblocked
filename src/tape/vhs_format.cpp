#include "tape/vhs_format.h"

#include <cstring>

#include "util/file_util.h"

namespace rt90 {

namespace {

void put16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
void put64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t* p) {
    uint32_t v = 0;
    for (int i = 3; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}
uint64_t get64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

constexpr uint32_t kChunkHeader = 12;  // tag + frame + size
constexpr uint32_t kMaxVideoBytes = 4u << 20;
constexpr uint32_t kMaxAudioSamples = 48000;

void set_error(std::string* e, const std::string& msg) {
    if (e) *e = msg;
}

}  // namespace

void VhsHeader::serialize(uint8_t out[kSize]) const {
    std::memset(out, 0, kSize);
    std::memcpy(out, kMagic, 4);
    put16(out + 4, version);
    put16(out + 6, kSize);
    put16(out + 8, width);
    put16(out + 10, height);
    put32(out + 12, fps_num);
    put32(out + 16, fps_den);
    put32(out + 20, audio_rate);
    put16(out + 24, audio_channels);
    put16(out + 26, audio_bits);
    put32(out + 28, frame_count);
    put64(out + 32, duration_us);
    put64(out + 40, index_offset);
    put32(out + 48, index_count);
    out[52] = video_codec;
    out[53] = jpeg_quality;
    // Title: UTF-8, truncated on a character boundary, always NUL-terminated.
    size_t n = std::min(title.size(), kTitleBytes - 1);
    while (n > 0 && n < title.size() && (static_cast<uint8_t>(title[n]) & 0xC0) == 0x80) --n;
    std::memcpy(out + 56, title.data(), n);
    put64(out + 184, source_size);
    put64(out + 192, static_cast<uint64_t>(source_mtime));
}

bool VhsHeader::parse(const uint8_t in[kSize], std::string* error) {
    if (std::memcmp(in, kMagic, 4) != 0) {
        set_error(error, "not a VHS1 tape");
        return false;
    }
    version = get16(in + 4);
    if (version != kVersion) {
        set_error(error, "unsupported tape version " + std::to_string(version));
        return false;
    }
    if (get16(in + 6) != kSize) {
        set_error(error, "bad header size");
        return false;
    }
    width = get16(in + 8);
    height = get16(in + 10);
    fps_num = get32(in + 12);
    fps_den = get32(in + 16);
    audio_rate = get32(in + 20);
    audio_channels = get16(in + 24);
    audio_bits = get16(in + 26);
    frame_count = get32(in + 28);
    duration_us = get64(in + 32);
    index_offset = get64(in + 40);
    index_count = get32(in + 48);
    video_codec = in[52];
    jpeg_quality = in[53];
    const char* t = reinterpret_cast<const char*>(in + 56);
    title.assign(t, strnlen(t, kTitleBytes));
    source_size = get64(in + 184);
    source_mtime = static_cast<int64_t>(get64(in + 192));
    if (width != 320 || height != 240 || fps_num == 0 || fps_den == 0 || audio_rate == 0 ||
        audio_rate > 192000 || audio_channels != 1 || audio_bits != 16 ||
        video_codec != static_cast<uint8_t>(VideoCodec::Jpeg)) {
        set_error(error, "unsupported tape format");
        return false;
    }
    if (index_count != frame_count) {
        set_error(error, "tape index does not match its length");
        return false;
    }
    return true;
}

// ---- writer -------------------------------------------------------------------

VhsWriter::~VhsWriter() { abort(); }

bool VhsWriter::open(const std::string& path, std::string* error) {
    abort();
    path_ = path;
    part_ = path + ".part";
    f_ = fopen_utf8(part_, "wb");
    if (!f_) {
        set_error(error, "cannot create " + part_);
        return false;
    }
    index_.clear();
    failed_ = false;
    uint8_t blank[VhsHeader::kSize] = {};
    return put(blank, sizeof blank);  // patched in finish()
}

bool VhsWriter::put(const void* p, size_t n) {
    if (!f_ || failed_) return false;
    if (n && std::fwrite(p, 1, n, f_) != n) failed_ = true;
    return !failed_;
}

bool VhsWriter::add_frame(const std::vector<uint8_t>& jpeg, const int16_t* audio, size_t samples) {
    if (!f_ || failed_) return false;
    VhsIndexEntry e;
    e.offset = static_cast<uint64_t>(ftell64(f_));
    e.video_bytes = static_cast<uint32_t>(jpeg.size());
    e.audio_samples = static_cast<uint32_t>(samples);
    const uint32_t frame = static_cast<uint32_t>(index_.size());
    uint8_t h[kChunkHeader];
    std::memcpy(h, "VIDF", 4);
    put32(h + 4, frame);
    put32(h + 8, e.video_bytes);
    put(h, sizeof h);
    put(jpeg.data(), jpeg.size());
    std::memcpy(h, "AUDF", 4);
    put32(h + 8, e.audio_samples * 2);
    put(h, sizeof h);
    // PCM is stored little-endian.
    std::vector<uint8_t> pcm(samples * 2);
    for (size_t i = 0; i < samples; ++i) put16(pcm.data() + i * 2, static_cast<uint16_t>(audio[i]));
    put(pcm.data(), pcm.size());
    if (!failed_) index_.push_back(e);
    return !failed_;
}

bool VhsWriter::finish(VhsHeader header, std::string* error) {
    if (!f_) {
        set_error(error, "tape is not open");
        return false;
    }
    header.frame_count = header.index_count = static_cast<uint32_t>(index_.size());
    header.index_offset = static_cast<uint64_t>(ftell64(f_));
    if (header.fps_num)
        header.duration_us = static_cast<uint64_t>(index_.size()) * 1000000ull * header.fps_den / header.fps_num;
    uint8_t ix[8];
    std::memcpy(ix, "INDX", 4);
    put32(ix + 4, header.index_count);
    put(ix, sizeof ix);
    for (const VhsIndexEntry& e : index_) {
        uint8_t b[16];
        put64(b, e.offset);
        put32(b + 8, e.video_bytes);
        put32(b + 12, e.audio_samples);
        put(b, sizeof b);
    }
    uint8_t hdr[VhsHeader::kSize];
    header.serialize(hdr);
    if (!failed_ && fseek64(f_, 0, SEEK_SET) != 0) failed_ = true;
    put(hdr, sizeof hdr);
    if (std::fflush(f_) != 0) failed_ = true;
    std::fclose(f_);
    f_ = nullptr;
    if (failed_) {
        remove_file(part_);
        set_error(error, "write failed (disk full?)");
        return false;
    }
    if (!rename_file(part_, path_)) {
        remove_file(part_);
        set_error(error, "cannot write " + path_);
        return false;
    }
    return true;
}

void VhsWriter::abort() {
    if (f_) {
        std::fclose(f_);
        f_ = nullptr;
        remove_file(part_);
    }
}

// ---- reader --------------------------------------------------------------------

VhsReader::~VhsReader() { close(); }

void VhsReader::close() {
    if (f_) std::fclose(f_);
    f_ = nullptr;
    index_.clear();
}

bool VhsReader::open(const std::string& path, std::string* error) {
    close();
    path_ = path;
    const int64_t size = file_size(path);
    f_ = fopen_utf8(path, "rb");
    if (!f_ || size < 0) {
        set_error(error, "cannot open tape");
        close();
        return false;
    }
    uint8_t hdr[VhsHeader::kSize];
    if (std::fread(hdr, 1, sizeof hdr, f_) != sizeof hdr) {
        set_error(error, "tape is too short");
        close();
        return false;
    }
    if (!header_.parse(hdr, error)) {
        close();
        return false;
    }
    const uint64_t index_bytes = 8 + uint64_t{header_.index_count} * 16;
    if (header_.index_offset < VhsHeader::kSize || header_.index_offset + index_bytes > static_cast<uint64_t>(size)) {
        set_error(error, "tape index is missing (unfinished recording?)");
        close();
        return false;
    }
    std::vector<uint8_t> raw(index_bytes);
    if (fseek64(f_, static_cast<int64_t>(header_.index_offset), SEEK_SET) != 0 ||
        std::fread(raw.data(), 1, raw.size(), f_) != raw.size() || std::memcmp(raw.data(), "INDX", 4) != 0 ||
        get32(raw.data() + 4) != header_.index_count) {
        set_error(error, "tape index is damaged");
        close();
        return false;
    }
    index_.resize(header_.index_count);
    for (uint32_t i = 0; i < header_.index_count; ++i) {
        const uint8_t* b = raw.data() + 8 + size_t{i} * 16;
        VhsIndexEntry& e = index_[i];
        e.offset = get64(b);
        e.video_bytes = get32(b + 8);
        e.audio_samples = get32(b + 12);
        const uint64_t end = e.offset + 2 * kChunkHeader + e.video_bytes + uint64_t{e.audio_samples} * 2;
        if (e.offset < VhsHeader::kSize || end > header_.index_offset || e.video_bytes > kMaxVideoBytes ||
            e.audio_samples > kMaxAudioSamples) {
            set_error(error, "tape index is damaged");
            close();
            return false;
        }
    }
    return true;
}

bool VhsReader::read_frame(uint32_t index, TapeFrame& out) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!f_ || index >= index_.size()) return false;
    const VhsIndexEntry& e = index_[index];
    std::vector<uint8_t> buf(2 * kChunkHeader + e.video_bytes + size_t{e.audio_samples} * 2);
    if (fseek64(f_, static_cast<int64_t>(e.offset), SEEK_SET) != 0 || std::fread(buf.data(), 1, buf.size(), f_) != buf.size())
        return false;
    const uint8_t* v = buf.data();
    const uint8_t* a = v + kChunkHeader + e.video_bytes;
    if (std::memcmp(v, "VIDF", 4) != 0 || get32(v + 4) != index || get32(v + 8) != e.video_bytes ||
        std::memcmp(a, "AUDF", 4) != 0 || get32(a + 8) != e.audio_samples * 2)
        return false;
    out.index = index;
    out.codec = VideoCodec::Jpeg;
    out.video.assign(v + kChunkHeader, v + kChunkHeader + e.video_bytes);
    out.audio.resize(e.audio_samples);
    for (uint32_t i = 0; i < e.audio_samples; ++i) out.audio[i] = static_cast<int16_t>(get16(a + kChunkHeader + i * 2));
    return true;
}

std::shared_ptr<VhsSource> VhsSource::open(const std::string& path, std::string* error) {
    auto src = std::make_shared<VhsSource>();
    if (!src->reader_.open(path, error)) return nullptr;
    return src;
}

}  // namespace rt90
