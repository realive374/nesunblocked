// Identifies dropped/opened files by their content (magic bytes), not by
// their extension: an MP4 renamed to .txt is still an MP4.
#pragma once

#include <cstdint>
#include <string>

namespace rt90 {

enum class FileKind { Missing, Empty, Mp4, Vhs, Unknown };

struct Detection {
    FileKind kind = FileKind::Unknown;
    std::string reason;  // human-readable, for the NOT RECOGNIZED screen
};

// Checks the first bytes of `data` (`file_size` is the whole file's size).
Detection detect_bytes(const uint8_t* data, size_t n, int64_t file_size);
Detection detect_file(const std::string& path);
const char* file_kind_name(FileKind k);

}  // namespace rt90
