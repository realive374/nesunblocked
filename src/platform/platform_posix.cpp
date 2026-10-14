#ifndef _WIN32
#include <cstdio>

#include "platform/platform.h"

namespace rt90::platform {

namespace {
Uint32 g_menu_event = 0;
}

Uint32 menu_event_type() {
    if (!g_menu_event) g_menu_event = SDL_RegisterEvents(1);
    return g_menu_event;
}

void install_menu(SDL_Window*) { menu_event_type(); }
void set_menu_visible(SDL_Window*, bool) {}

// Uses zenity or kdialog when available.
bool open_file_dialog(SDL_Window*, std::string& path_utf8) {
    const char* cmds[] = {
        "zenity --file-selection --title='Insert a tape' "
        "--file-filter='Videos and tapes | *.mp4 *.m4v *.mov *.vhs' --file-filter='All files | *' 2>/dev/null",
        "kdialog --getopenfilename . '*.mp4 *.m4v *.mov *.vhs' 2>/dev/null",
    };
    for (const char* cmd : cmds) {
        FILE* p = popen(cmd, "r");
        if (!p) continue;
        char buf[4096];
        std::string out;
        while (std::fgets(buf, sizeof buf, p)) out += buf;
        const int rc = pclose(p);
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
        if (rc == 0 && !out.empty()) {
            path_utf8 = out;
            return true;
        }
        if (rc == 0 || rc == 256) return false;  // the dialog ran and was cancelled
    }
    return false;
}

}  // namespace rt90::platform
#endif
