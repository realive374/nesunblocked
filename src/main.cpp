// RetroTube 90 - desktop host.
//
// Owns the SDL window, renderer and audio device and feeds the TvSet: real
// time goes in (nudged by AvSync so audio and video stay locked), keyboard
// input becomes front-panel button presses, and every new video frame is run
// through the CRT filter and shown.
#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "app/tv_set.h"
#include "app/ui_overlay.h"
#include "av/av_sync.h"
#include "av/timing.h"
#include "util/image_io.h"
#include "vcr/io_ports.h"

namespace {

using namespace rt90;

constexpr int kCanvasW = kScreenW * 3;  // CRT canvas: 960x720
constexpr int kCanvasH = kScreenH * 3;

struct Options {
    long max_frames = -1;               // --frames N: quit after N video frames (smoke tests)
    bool test_tape = false;             // --test-tape: start with a blank test tape inserted
    bool autoplay = false;              // --play: press PLAY once the tape is in
    std::map<long, std::string> shots;  // --shot FRAME:FILE.png (repeatable)
    float noise = -1;                   // --noise PERCENT
    bool show_slider = false;           // --slider: open the noise slider at start
};

Options parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) o.max_frames = std::strtol(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--test-tape")) o.test_tape = true;
        else if (!std::strcmp(argv[i], "--play")) o.autoplay = true;
        else if (!std::strcmp(argv[i], "--noise") && i + 1 < argc) o.noise = std::strtof(argv[++i], nullptr) / 100.0f;
        else if (!std::strcmp(argv[i], "--slider")) o.show_slider = true;
        else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) {
            const std::string a = argv[++i];
            const size_t colon = a.find(':');
            if (colon != std::string::npos) o.shots[std::strtol(a.c_str(), nullptr, 10)] = a.substr(colon + 1);
        }
    }
    return o;
}

// OSD glyph codes -> UTF-8 for logs.
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

class App {
public:
    explicit App(const Options& opt) : opt_(opt) {}
    ~App() { shutdown(); }

    bool init();
    int run();

private:
    void handle_event(const SDL_Event& ev);
    void change_noise(float delta);
    bool slider_visible() const { return SDL_GetTicks() < slider_until_; }
    void pump_audio();
    void present();
    void shutdown();

    Options opt_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    SDL_AudioDeviceID audio_dev_ = 0;
    TvSet tv_;
    AvSync sync_{kAudioRate};
    Framebuffer canvas_{kCanvasW, kCanvasH};
    std::vector<int16_t> audio_buf_;
    uint64_t presented_ = 0;
    std::string last_osd_;
    bool fullscreen_ = false;
    bool running_ = true;
    bool autoplay_pending_ = false;
    Uint32 slider_until_ = 0;  // the noise slider is on screen until this time
};

bool App::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    window_ = SDL_CreateWindow("RetroTube 90", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kCanvasW, kCanvasH,
                               SDL_WINDOW_RESIZABLE);
    if (window_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_ && window_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    if (renderer_)
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kCanvasW,
                                     kCanvasH);
    if (!texture_) {
        std::fprintf(stderr, "SDL window/renderer setup failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(texture_, SDL_ScaleModeLinear);
    SDL_RenderSetLogicalSize(renderer_, kCanvasW, kCanvasH);  // keep 4:3, letterbox the rest

    SDL_AudioSpec want{}, have{};
    want.freq = kAudioRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    audio_dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);  // SDL converts to the device format
    if (audio_dev_) SDL_PauseAudioDevice(audio_dev_, 0);
    else std::fprintf(stderr, "No audio device (%s); running silent\n", SDL_GetError());

    std::string err;
    if (!tv_.boot(&err)) {
        std::fprintf(stderr, "Firmware failed to assemble:\n%s", err.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RetroTube 90", err.c_str(), window_);
        return false;
    }
    if (opt_.noise >= 0) tv_.set_noise(opt_.noise);
    if (opt_.show_slider) slider_until_ = 0xFFFFFFFFu;
    tv_.set_overlay([this](Framebuffer& fb) {
        if (slider_visible()) draw_noise_slider(fb, tv_.noise());
    });
    if (opt_.test_tape || opt_.autoplay) tv_.insert_tape(std::make_shared<TestPatternSource>());
    autoplay_pending_ = opt_.autoplay;
    return true;
}

void App::change_noise(float delta) {
    const float v = std::round((tv_.noise() + delta) * 100.0f) / 100.0f;
    tv_.set_noise(v);
    slider_until_ = SDL_GetTicks() + 3000;
}

void App::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_QUIT) {
        running_ = false;
    } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        const SDL_Keycode key = ev.key.keysym.sym;
        if (key == SDLK_f) {
            fullscreen_ = !fullscreen_;
            SDL_SetWindowFullscreen(window_, fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        } else if (key == SDLK_n) {
            // N opens the noise slider; N again (or 3 s without input) closes it.
            slider_until_ = slider_visible() ? 0 : SDL_GetTicks() + 3000;
        } else if (slider_visible() && (key == SDLK_LEFT || key == SDLK_RIGHT)) {
            change_noise(key == SDLK_LEFT ? -0.05f : 0.05f);  // arrows adjust while the slider is open
        } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
            change_noise(-0.05f);
        } else if (key == SDLK_EQUALS || key == SDLK_PLUS || key == SDLK_KP_PLUS) {
            change_noise(0.05f);
        } else if (key == SDLK_i) {
            tv_.interlace().enabled = !tv_.interlace().enabled;
        } else if (key == SDLK_l && !tv_.has_tape()) {
            tv_.insert_tape(std::make_shared<TestPatternSource>());
        } else if (uint8_t b = key_to_button(key)) {
            tv_.machine().press(b);
        }
    } else if (ev.type == SDL_KEYUP) {
        if (uint8_t b = key_to_button(ev.key.keysym.sym)) tv_.machine().release(b);
    } else if (ev.type == SDL_MOUSEWHEEL) {
        change_noise(ev.wheel.y > 0 ? 0.05f : -0.05f);
    }
}

void App::pump_audio() {
    audio_buf_.clear();
    tv_.take_audio(audio_buf_);
    if (!audio_dev_ || audio_buf_.empty()) return;
    // After a long stall, drop the backlog rather than lag behind the picture.
    if (SDL_GetQueuedAudioSize(audio_dev_) > kAudioRate) SDL_ClearQueuedAudio(audio_dev_);
    SDL_QueueAudio(audio_dev_, audio_buf_.data(), static_cast<Uint32>(audio_buf_.size() * sizeof(int16_t)));
}

void App::present() {
    // The GPU composes once per video frame; the CRT canvas is re-presented
    // at the display's refresh rate in between.
    if (tv_.frames_composed() != presented_) {
        presented_ = tv_.frames_composed();
        tv_.render(canvas_);
        SDL_UpdateTexture(texture_, nullptr, canvas_.px.data(), kCanvasW * 4);
        auto shot = opt_.shots.find(static_cast<long>(tv_.machine().frame()));
        if (shot != opt_.shots.end()) save_png(canvas_, shot->second);
    }
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

int App::run() {
    Uint64 last = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    while (running_) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) handle_event(ev);

        // Fixed-rate CPU clock: run as many cycles as real time elapsed,
        // adjusted by at most 0.5% to keep the audio queue steady.
        const Uint64 now = SDL_GetPerformanceCounter();
        const double dt = static_cast<double>(now - last) / freq;
        last = now;
        const size_t queued = audio_dev_ ? SDL_GetQueuedAudioSize(audio_dev_) / sizeof(int16_t)
                                         : static_cast<size_t>(sync_.target_samples());
        tv_.advance(sync_.adjust(dt, queued));
        pump_audio();

        Machine& m = tv_.machine();
        if (autoplay_pending_ && m.frame() > 75) {
            m.tap(io::BTN_PLAY);
            autoplay_pending_ = false;
        }
        if (m.cpu().fault() != CpuFault::None) {
            std::fprintf(stderr, "VCR-1 fault: %s at $%04X\n", fault_name(m.cpu().fault()), m.cpu().fault_pc());
            running_ = false;
        }
        const std::string osd = m.gpu().osd_text();
        if (osd != last_osd_) {
            std::printf("frame %6llu  deck=%-5s  OSD: %s\n", static_cast<unsigned long long>(m.frame()),
                        deck_state_name(m.deck().state()), osd_to_utf8(osd).c_str());
            std::fflush(stdout);
            last_osd_ = osd;
        }

        present();
        SDL_Delay(1);
        if (opt_.max_frames >= 0 && static_cast<long>(m.frame()) >= opt_.max_frames) running_ = false;
    }
    const Machine& m = tv_.machine();
    std::printf("exit: %llu frames, %llu CPU cycles, %llu instructions, %s\n",
                static_cast<unsigned long long>(m.frame()), static_cast<unsigned long long>(m.cpu().total_cycles()),
                static_cast<unsigned long long>(m.cpu().instructions()), m.cpu().describe().c_str());
    return m.cpu().fault() == CpuFault::None ? 0 : 1;
}

void App::shutdown() {
    if (audio_dev_) SDL_CloseAudioDevice(audio_dev_);
    audio_dev_ = 0;
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    texture_ = nullptr;
    renderer_ = nullptr;
    window_ = nullptr;
    SDL_Quit();
}

}  // namespace

int main(int argc, char** argv) {
    App app(parse_args(argc, argv));
    if (!app.init()) return 1;
    return app.run();
}
