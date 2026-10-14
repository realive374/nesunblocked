// RetroTube 90 - desktop host.
//
// Phase 1: boots the VCR-1 firmware and ticks the CPU at a fixed 1 MHz clock
// from the main loop. Until the GPU lands (Phase 2) the screen mode is shown
// as a flat placeholder and the OSD text is mirrored into the window title.
#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "vcr/io_ports.h"
#include "vcr/machine.h"

namespace {

using namespace rt90;

constexpr int kInternalW = 320;
constexpr int kInternalH = 240;

struct Options {
    long max_frames = -1;      // --frames N: quit after N video frames (smoke tests)
    bool test_tape = false;    // --test-tape: start with a blank test tape inserted
    bool autoplay = false;     // --play: press PLAY once the tape is in
};

Options parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) o.max_frames = std::strtol(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--test-tape")) o.test_tape = true;
        else if (!std::strcmp(argv[i], "--play")) o.autoplay = o.test_tape = true;
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

// Placeholder picture for each screen mode (replaced by the GPU in Phase 2).
void draw_placeholder(uint8_t screen, std::vector<uint32_t>& px, std::mt19937& rng) {
    switch (screen) {
        case io::SCREEN_STATIC:
            for (uint32_t& p : px) {
                const uint32_t v = rng() & 0xFF;
                p = 0xFF000000u | (v << 16) | (v << 8) | v;
            }
            break;
        case io::SCREEN_BLUE:
            std::fill(px.begin(), px.end(), 0xFF0020C0u);
            break;
        case io::SCREEN_TAPE: {
            // SMPTE-ish colour bars stand in for tape video.
            static const uint32_t bars[7] = {0xFFC0C0C0, 0xFFC0C000, 0xFF00C0C0, 0xFF00C000,
                                             0xFFC000C0, 0xFFC00000, 0xFF0000C0};
            for (int y = 0; y < kInternalH; ++y)
                for (int x = 0; x < kInternalW; ++x)
                    px[static_cast<size_t>(y * kInternalW + x)] = bars[x * 7 / kInternalW];
            break;
        }
        default:
            std::fill(px.begin(), px.end(), 0xFF000000u);
            break;
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
                                          kInternalW * 3, kInternalH * 3, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : nullptr;
    if (!renderer && window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                        SDL_TEXTUREACCESS_STREAMING, kInternalW, kInternalH)
                                    : nullptr;
    if (!texture) {
        std::fprintf(stderr, "SDL window/renderer setup failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, kInternalW * 4, kInternalH * 4);  // keep 4:3

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

    std::vector<uint32_t> pixels(static_cast<size_t>(kInternalW * kInternalH));
    std::mt19937 rng(1990);
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

        draw_placeholder(machine.gpu().screen(), pixels, rng);
        SDL_UpdateTexture(texture, nullptr, pixels.data(), kInternalW * 4);
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
