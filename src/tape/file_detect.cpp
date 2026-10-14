#include "tape/file_detect.h"

#include <cstring>

#include "util/file_util.h"

namespace rt90 {

const char* file_kind_name(FileKind k) {
    switch (k) {
        case FileKind::Missing: return "missing";
        case FileKind::Empty:   return "empty";
        case FileKind::Mp4:     return "mp4";
        case FileKind::Vhs:     return "vhs";
        case FileKind::Unknown: return "unknown";
    }
    return "?";
}

Detection detect_bytes(const uint8_t* d, size_t n, int64_t file_size) {
    if (file_size == 0 || n == 0) return {FileKind::Empty, "the file is empty"};
    if (n >= 4 && std::memcmp(d, "VHS1", 4) == 0) return {FileKind::Vhs, ""};
    // ISO base media file (MP4/M4V/MOV): the first box is "ftyp":
    //   u32 big-endian box size, "ftyp", 4-char major brand, ...
    if (n >= 12 && std::memcmp(d + 4, "ftyp", 4) == 0) {
        const uint32_t box = (uint32_t{d[0]} << 24) | (uint32_t{d[1]} << 16) | (uint32_t{d[2]} << 8) | d[3];
        bool brand_ok = true;
        for (int i = 8; i < 12; ++i) brand_ok &= d[i] >= 0x20 && d[i] < 0x7F;
        const bool size_ok = box >= 16 && box <= 4096 && static_cast<int64_t>(box) <= file_size;
        if (brand_ok && size_ok) return {FileKind::Mp4, ""};
        return {FileKind::Unknown, "the MP4 header is damaged"};
    }
    if (n >= 8 && std::memcmp(d + 4, "ftyp", 4) == 0) return {FileKind::Unknown, "the MP4 header is damaged"};
    return {FileKind::Unknown, "this is not a video file"};
}

Detection detect_file(const std::string& path) {
    const int64_t size = file_size(path);
    FILE* f = fopen_utf8(path, "rb");
    if (!f || size < 0) {
        if (f) std::fclose(f);
        return {FileKind::Missing, "the file could not be opened"};
    }
    uint8_t buf[16] = {};
    const size_t n = std::fread(buf, 1, sizeof buf, f);
    std::fclose(f);
    return detect_bytes(buf, n, size);
}

}  // namespace rt90
