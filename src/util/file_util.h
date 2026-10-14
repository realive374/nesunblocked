// File helpers that take UTF-8 paths on every platform (on Windows they go
// through the wide-character APIs so names like "Amélie.mp4" work).
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace rt90 {

FILE* fopen_utf8(const std::string& path, const char* mode);
bool file_exists(const std::string& path);
// Returns -1 if the file cannot be stat'ed.
int64_t file_size(const std::string& path);
// Modification time in seconds since the epoch (0 if unknown).
int64_t file_mtime(const std::string& path);
bool remove_file(const std::string& path);
// Renames, replacing `to` if it exists.
bool rename_file(const std::string& from, const std::string& to);
bool make_dirs(const std::string& path);

// "C:\movies\Film.mp4" -> "C:\movies\" ; "Film.mp4" ; "Film" ; ".mp4"
std::string path_dir(const std::string& path);
std::string path_filename(const std::string& path);
std::string path_stem(const std::string& path);
std::string path_ext_lower(const std::string& path);
std::string replace_ext(const std::string& path, const std::string& new_ext);
// Per-user data folder for RetroTube 90 (e.g. %LOCALAPPDATA%\RetroTube90).
std::string user_data_dir();
// Directory of the running executable (with trailing separator).
std::string exe_dir();

// 64-bit seek / tell.
int fseek64(FILE* f, int64_t off, int whence);
int64_t ftell64(FILE* f);

}  // namespace rt90
