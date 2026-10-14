#ifdef _WIN32
#include <SDL.h>
#include <SDL_syswm.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>

#include <vector>

#include "platform/platform.h"

namespace rt90::platform {

namespace {

Uint32 g_menu_event = 0;
HMENU g_menu = nullptr;

HWND hwnd_of(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return nullptr;
    return info.info.win.window;
}

void SDLCALL message_hook(void*, void*, unsigned int message, Uint64 wparam, Sint64) {
    if (message != WM_COMMAND || HIWORD(wparam) != 0) return;
    const int id = LOWORD(wparam);
    if (id < MENU_OPEN || id > MENU_ABOUT) return;
    SDL_Event ev{};
    ev.type = g_menu_event;
    ev.user.code = id;
    SDL_PushEvent(&ev);
}

std::string narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

}  // namespace

Uint32 menu_event_type() {
    if (!g_menu_event) g_menu_event = SDL_RegisterEvents(1);
    return g_menu_event;
}

void install_menu(SDL_Window* window) {
    menu_event_type();
    HWND hwnd = hwnd_of(window);
    if (!hwnd) return;
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, MENU_OPEN, L"&Open Tape or Video...\tCtrl+O");
    AppendMenuW(file, MF_STRING, MENU_EJECT, L"&Eject\tEsc");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, MENU_EXIT, L"E&xit\tAlt+F4");
    HMENU vcr = CreatePopupMenu();
    AppendMenuW(vcr, MF_STRING, MENU_PLAY, L"&Play / Pause\tSpace");
    AppendMenuW(vcr, MF_STRING, MENU_STOP, L"&Stop\tS");
    AppendMenuW(vcr, MF_STRING, MENU_REWIND, L"&Rewind\tLeft");
    AppendMenuW(vcr, MF_STRING, MENU_FFWD, L"&Fast-forward\tRight");
    AppendMenuW(vcr, MF_STRING, MENU_TRACKING, L"&Tracking\tT");
    HMENU view = CreatePopupMenu();
    AppendMenuW(view, MF_STRING, MENU_NOISE, L"&Noise Slider\tN");
    AppendMenuW(view, MF_STRING, MENU_FULLSCREEN, L"&Fullscreen\tF");
    AppendMenuW(view, MF_STRING, MENU_INTERLACE, L"&Interlaced Look\tI");
    AppendMenuW(view, MF_STRING, MENU_DEBUG, L"&Debug Overlay\tF1");
    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, MENU_CONTROLS, L"&Controls");
    AppendMenuW(help, MF_STRING, MENU_ABOUT, L"&About RetroTube 90");
    g_menu = CreateMenu();
    AppendMenuW(g_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(g_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(vcr), L"&VCR");
    AppendMenuW(g_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"V&iew");
    AppendMenuW(g_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"&Help");
    SetMenu(hwnd, g_menu);
    SDL_SetWindowsMessageHook(message_hook, nullptr);
}

void set_menu_visible(SDL_Window* window, bool visible) {
    if (HWND hwnd = hwnd_of(window)) SetMenu(hwnd, visible ? g_menu : nullptr);
}

bool open_file_dialog(SDL_Window* window, std::string& path_utf8) {
    std::vector<wchar_t> buf(32768, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = hwnd_of(window);
    ofn.lpstrFilter =
        L"Videos and tapes (*.mp4;*.m4v;*.mov;*.vhs)\0*.mp4;*.m4v;*.mov;*.vhs\0"
        L"VHS tapes (*.vhs)\0*.vhs\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.lpstrTitle = L"Insert a tape";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return false;
    path_utf8 = narrow(buf.data());
    return !path_utf8.empty();
}

}  // namespace rt90::platform
#endif
