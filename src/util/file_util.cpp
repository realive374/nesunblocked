#include "util/file_util.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace rt90 {

#ifdef _WIN32
namespace {
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}
}  // namespace

FILE* fopen_utf8(const std::string& path, const char* mode) {
    return _wfopen(widen(path).c_str(), widen(mode).c_str());
}
bool file_exists(const std::string& path) { return GetFileAttributesW(widen(path).c_str()) != INVALID_FILE_ATTRIBUTES; }
int64_t file_size(const std::string& path) {
    struct _stat64 st;
    return _wstat64(widen(path).c_str(), &st) == 0 ? static_cast<int64_t>(st.st_size) : -1;
}
int64_t file_mtime(const std::string& path) {
    struct _stat64 st;
    return _wstat64(widen(path).c_str(), &st) == 0 ? static_cast<int64_t>(st.st_mtime) : 0;
}
bool remove_file(const std::string& path) { return _wremove(widen(path).c_str()) == 0; }
bool rename_file(const std::string& from, const std::string& to) {
    return MoveFileExW(widen(from).c_str(), widen(to).c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}
bool make_dirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if ((path[i] == '\\' || path[i] == '/') && cur.size() > 3) CreateDirectoryW(widen(cur).c_str(), nullptr);
    }
    CreateDirectoryW(widen(path).c_str(), nullptr);
    const DWORD a = GetFileAttributesW(widen(path).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
std::string user_data_dir() {
    const wchar_t* base = _wgetenv(L"LOCALAPPDATA");
    std::string dir = base ? narrow(base) : std::string(".");
    return dir + "\\RetroTube90\\";
}
std::string exe_dir() {
    std::vector<wchar_t> buf(32768);
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    return path_dir(narrow(std::wstring(buf.data(), n)));
}
int fseek64(FILE* f, int64_t off, int whence) { return _fseeki64(f, off, whence); }
int64_t ftell64(FILE* f) { return _ftelli64(f); }
#else
FILE* fopen_utf8(const std::string& path, const char* mode) { return std::fopen(path.c_str(), mode); }
bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}
int64_t file_size(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 ? static_cast<int64_t>(st.st_size) : -1;
}
int64_t file_mtime(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 ? static_cast<int64_t>(st.st_mtime) : 0;
}
bool remove_file(const std::string& path) { return std::remove(path.c_str()) == 0; }
bool rename_file(const std::string& from, const std::string& to) { return std::rename(from.c_str(), to.c_str()) == 0; }
bool make_dirs(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/' && cur.size() > 1) mkdir(cur.c_str(), 0755);
    }
    mkdir(path.c_str(), 0755);
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
std::string user_data_dir() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    std::string base = xdg && *xdg ? xdg : (home ? std::string(home) + "/.local/share" : std::string("."));
    return base + "/RetroTube90/";
}
std::string exe_dir() {
    std::vector<char> buf(4096);
    const ssize_t n = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    return n > 0 ? path_dir(std::string(buf.data(), static_cast<size_t>(n))) : std::string("./");
}
int fseek64(FILE* f, int64_t off, int whence) { return fseeko(f, static_cast<off_t>(off), whence); }
int64_t ftell64(FILE* f) { return static_cast<int64_t>(ftello(f)); }
#endif

namespace {
size_t last_sep(const std::string& p) {
    const size_t a = p.find_last_of('/');
    const size_t b = p.find_last_of('\\');
    if (a == std::string::npos) return b;
    if (b == std::string::npos) return a;
    return std::max(a, b);
}
}  // namespace

std::string path_dir(const std::string& path) {
    const size_t s = last_sep(path);
    return s == std::string::npos ? std::string() : path.substr(0, s + 1);
}

std::string path_filename(const std::string& path) {
    const size_t s = last_sep(path);
    return s == std::string::npos ? path : path.substr(s + 1);
}

std::string path_stem(const std::string& path) {
    const std::string name = path_filename(path);
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

std::string path_ext_lower(const std::string& path) {
    const std::string name = path_filename(path);
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return {};
    std::string e = name.substr(dot);
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

std::string replace_ext(const std::string& path, const std::string& new_ext) {
    return path_dir(path) + path_stem(path) + new_ext;
}

}  // namespace rt90
