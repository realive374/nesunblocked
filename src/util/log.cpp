#include "util/log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

#include "util/file_util.h"

namespace rt90 {

namespace {
std::mutex g_mu;
FILE* g_file = nullptr;
std::string g_path;
const auto g_start = std::chrono::steady_clock::now();

void vlog(const char* level, const char* fmt, va_list ap) {
    char msg[2048];
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_file) {
        std::fprintf(g_file, "[%9.3f] %-5s %s\n", t, level, msg);
        std::fflush(g_file);
    }
#ifdef NDEBUG
    if (!g_file)
#endif
        std::fprintf(stderr, "[%9.3f] %-5s %s\n", t, level, msg);
}
}  // namespace

bool log_open(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_file) std::fclose(g_file);
    g_file = fopen_utf8(path, "w");
    g_path = g_file ? path : std::string();
    if (g_file) {
        const std::time_t now = std::time(nullptr);
        char when[64];
        std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        std::fprintf(g_file, "RetroTube 90 log - %s\n", when);
    }
    return g_file != nullptr;
}

void log_close() {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_file) std::fclose(g_file);
    g_file = nullptr;
}

const std::string& log_path() { return g_path; }

#define RT90_LOG_IMPL(name, level)  \
    void name(const char* fmt, ...) { \
        va_list ap;                   \
        va_start(ap, fmt);            \
        vlog(level, fmt, ap);         \
        va_end(ap);                   \
    }
RT90_LOG_IMPL(log_info, "INFO")
RT90_LOG_IMPL(log_warn, "WARN")
RT90_LOG_IMPL(log_error, "ERROR")

}  // namespace rt90
