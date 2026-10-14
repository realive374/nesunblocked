// Tiny thread-safe logger: writes timestamped lines to a log file (and to
// stderr in debug builds / when no file is open).
#pragma once

#include <string>

namespace rt90 {

// Opens (truncates) the log file. Safe to call once at startup.
bool log_open(const std::string& path);
void log_close();
const std::string& log_path();

void log_info(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
void log_warn(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
void log_error(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

}  // namespace rt90
