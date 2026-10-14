// RetroTube 90 - desktop host.
//
// Boots the VCR-1 firmware, ticks the CPU at a fixed 1 MHz clock from the main
// loop, and shows the GPU's picture through the CRT filter.
#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "gpu/gpu.h"
#include "gpu/test_pattern.h"
#include "util/image_io.h"
#include "vcr/io_ports.h"
#include "vcr/machine.h"

namespace {

using namespace rt90;

constexpr int kCanvasW = kScreenW * 3;  // CRT canvas: 960x720
constexpr int kCanvasH = kScreenH * 3;

struct Options {
    long max_frames = -1;      // --frames N: quit after N video frames (smoke tests)
    bool test_tape = false;    // --test-tape: start with a blank test tape inserted
    bool autoplay = false;     // --play: press PLAY once the tape is in
    std::map<long, std::string> shots;  // --shot FRAME:FILE.png (repeatable)
};

Options parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) o.max_frames = std::strtol(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--test-tape")) o.test_tape = true;
        else if (!std::strcmp(argv[i], "--play")) o.autoplay = o.test_tape = true;
        else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) {
            const std::string a = argv[++i];
            const size_t colon = a.find(':');
            if (colon != std::string::npos) o.shots[std::strtol(a.c_str(), nullptr, 10)] = a.substr(colon + 1);
        }
    }
    return o;
}

TapeInfo blank_test_tape() {
    TapeInfo t;
    t.title = "T-120 TEST TAPE";
    t.fps = Machine::kFps;
    t.frame_count = static_cast<uint32_t>(2 * 3600 * t.fps);
    return t;
}

// OSD glyph codes -> UTF-8 for the window title.
std::string osd_to_utf8(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
            case io::GLYPH_PLAY:  out += "\xE2\x96\xB6"; break;  // ▶
            case io::GLYPH_REV:   out += "\xE2\x97\x80"; break;  // ◀
            case io::GLYPH_STOP:  out += "\xE2\x96\xA0"; break;  // ■
            case io::GLYPH_PAUSE: out += "\xE2\x80\x96"; break;  // ‖
            case io::GLYPH_EJECT: out += "\xE2\x8F\x8F"; break;  // ⏏
            default:              out += static_cast<char>(c); break;
        }
    }
    return out;
}

uint8_t key_to_button(SDL_Keycode key) {
    switch (key) {
        case SDLK_SPACE:  return io::BTN_PLAY;
        case SDLK_s:      return io::BTN_STOP;
        case SDLK_LEFT:   return io::BTN_REW;
        case SDLK_RIGHT:  return io::BTN_FF;
        case SDLK_ESCAPE: return io::BTN_EJECT;
        case SDLK_t:      return io::BTN_TRACK;
        case SDLK_p:      return io::BTN_POWER;
        default:          return 0;
    }
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parse_args(argc, argv);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("RetroTube 90", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          kCanvasW, kCanvasH, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : nullptr;
    if (!renderer && window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                        SDL_TEXTUREACCESS_STREAMING, kCanvasW, kCanvasH)
                                    : nullptr;
    if (!texture) {
        std::fprintf(stderr, "SDL window/renderer setup failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    SDL_RenderSetLogicalSize(renderer, kCanvasW, kCanvasH);  // keep 4:3, letterbox the rest

    Machine machine;
    std::string err;
    if (!machine.boot_default(&err)) {
        std::fprintf(stderr, "Firmware failed to assemble:\n%s", err.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RetroTube 90", err.c_str(), window);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (opt.test_tape) machine.deck().insert(blank_test_tape());

    Gpu gpu;
    Framebuffer tape_layer(kScreenW, kScreenH);
    Framebuffer canvas(kCanvasW, kCanvasH);
    uint64_t composed_frame = ~0ull;
    std::string last_title;
    bool fullscreen = false;
    bool running = true;
    bool autoplay_pending = opt.autoplay;
    Uint64 last = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                if (ev.key.keysym.sym == SDLK_f) {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                } else if (ev.key.keysym.sym == SDLK_l && !machine.deck().has_tape()) {
                    machine.deck().insert(blank_test_tape());  // dev shortcut until Phase 5 import
                } else if (uint8_t b = key_to_button(ev.key.keysym.sym)) {
                    machine.press(b);
                }
            } else if (ev.type == SDL_KEYUP) {
                if (uint8_t b = key_to_button(ev.key.keysym.sym)) machine.release(b);
            }
        }

        // Fixed-rate CPU clock: run exactly as many cycles as real time elapsed.
        const Uint64 now = SDL_GetPerformanceCounter();
        const double dt = static_cast<double>(now - last) / freq;
        last = now;
        machine.advance(dt);

        if (autoplay_pending && machine.frame() > 60) {
            machine.tap(io::BTN_PLAY);
            autoplay_pending = false;
        }

        if (machine.cpu().fault() != CpuFault::None) {
            std::fprintf(stderr, "VCR-1 fault: %s at $%04X\n", fault_name(machine.cpu().fault()),
                         machine.cpu().fault_pc());
            running = false;
        }

        std::string title = "RetroTube 90";
        const std::string osd = machine.gpu().osd_text();
        if (!osd.empty()) title += "  [" + osd_to_utf8(osd) + "]";
        if (title != last_title) {
            SDL_SetWindowTitle(window, title.c_str());
            std::printf("frame %6llu  deck=%-5s  %s\n", static_cast<unsigned long long>(machine.frame()),
                        deck_state_name(machine.deck().state()), title.c_str());
            std::fflush(stdout);
            last_title = title;
        }

        // The GPU composes once per video frame; the CRT canvas is re-presented
        // at the display's refresh rate in between.
        if (machine.frame() != composed_frame) {
            composed_frame = machine.frame();
            const bool tape_picture = machine.deck().has_tape();
            if (tape_picture) draw_test_pattern(tape_layer, machine.deck().position());
            gpu.compose(machine.gpu(), tape_picture ? &tape_layer : nullptr, machine.frame());
            gpu.present(canvas);
            SDL_UpdateTexture(texture, nullptr, canvas.px.data(), kCanvasW * 4);
            auto shot = opt.shots.find(static_cast<long>(machine.frame()));
            if (shot != opt.shots.end()) save_png(canvas, shot->second);
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(1);

        if (opt.max_frames >= 0 && static_cast<long>(machine.frame()) >= opt.max_frames) running = false;
    }

    std::printf("exit: %llu frames, %llu CPU cycles, %llu instructions, %s\n",
                static_cast<unsigned long long>(machine.frame()),
                static_cast<unsigned long long>(machine.cpu().total_cycles()),
                static_cast<unsigned long long>(machine.cpu().instructions()), machine.cpu().describe().c_str());
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return machine.cpu().fault() == CpuFault::None ? 0 : 1;
}
