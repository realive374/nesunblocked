// Platform integration: the native File menu and the Open dialog.
#pragma once

#include <SDL.h>

#include <string>

namespace rt90::platform {

enum MenuCommand : int {
    MENU_OPEN = 1,
    MENU_EJECT,
    MENU_EXIT,
    MENU_PLAY,
    MENU_STOP,
    MENU_REWIND,
    MENU_FFWD,
    MENU_TRACKING,
    MENU_NOISE,
    MENU_FULLSCREEN,
    MENU_INTERLACE,
    MENU_DEBUG,
    MENU_CONTROLS,
    MENU_ABOUT,
};

// Adds the menu bar (Windows). Menu picks arrive as SDL_USEREVENT events
// of type menu_event_type() with the MenuCommand in event.user.code.
void install_menu(SDL_Window* window);
Uint32 menu_event_type();
// Hides the menu bar in fullscreen and brings it back in a window.
void set_menu_visible(SDL_Window* window, bool visible);

// Shows a native "Open" dialog. Returns false if cancelled / unavailable.
bool open_file_dialog(SDL_Window* window, std::string& path_utf8);

}  // namespace rt90::platform
