// RetroTube 90 - desktop host.
//
// Owns the SDL window, renderer and audio device and feeds the TvSet: real
// time goes in (nudged by AvSync so audio and video stay locked), keyboard
// input becomes front-panel button presses, and every new video frame is run
// through the CRT filter and shown. Files arrive from the File menu, drag &
// drop or the command line (dropping a file onto the .exe / a .vhs file
// association) and are inserted as tapes - MP4s are "recorded" first.
#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "app/tv_set.h"
#include "app/ui_overlay.h"
#include "av/av_sync.h"
#include "av/timing.h"
#include "platform/platform.h"
#include "tape/file_detect.h"
#include "tape/importer.h"
#include "tape/vhs_format.h"
#include "util/file_util.h"
#include "util/image_io.h"
#include "util/log.h"
#include "vcr/io_ports.h"

#ifndef RT90_VERSION
#define RT90_VERSION "dev"
#endif

namespace {

using namespace rt90;

constexpr int kCanvasW = kScreenW * 3;  // CRT canvas: 960x720
constexpr int kCanvasH = kScreenH * 3;

const char* const kControlsText =
    "Space\tplay / pause\n"
    "S\tstop\n"
    "Left / Right\trewind / fast-forward\n"
    "T\ttracking\n"
    "N\tnoise slider (then Left / Right)\n"
    "F\tfullscreen\n"
    "F1\tdebug overlay\n"
    "Esc\teject\n"
    "Ctrl+O\topen a video or tape\n"
    "I\tinterlaced look\n"
    "L\tinsert the built-in test tape\n"
    "P\tpower\n\n"
    "Drag an .mp4 or .vhs file onto the TV to play it,\n"
    "or press Space with no tape in to pick one.";

struct Options {
    long max_frames = -1;               // --frames N: quit after N video frames (smoke tests)
    bool test_tape = false;             // --test-tape: start with the built-in test tape
    bool autoplay = false;              // --play: also press PLAY
    std::map<long, std::string> shots;  // --shot FRAME:FILE.png (repeatable)
    float noise = -1;                   // --noise PERCENT
    bool show_slider = false;           // --slider: open the noise slider at start
    bool debug = false;                 // --debug: start with the F1 overlay
    std::string open;                   // file to insert at start (drop onto the .exe)
    std::string renderer;               // --renderer NAME: try this SDL renderer first
};

Options parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!std::strcmp(a, "--frames") && i + 1 < argc) o.max_frames = std::strtol(argv[++i], nullptr, 10);
        else if (!std::strcmp(a, "--test-tape")) o.test_tape = true;
        else if (!std::strcmp(a, "--play")) o.autoplay = true;
        else if (!std::strcmp(a, "--noise") && i + 1 < argc) o.noise = std::strtof(argv[++i], nullptr) / 100.0f;
        else if (!std::strcmp(a, "--slider")) o.show_slider = true;
        else if (!std::strcmp(a, "--debug")) o.debug = true;
        else if (!std::strcmp(a, "--renderer") && i + 1 < argc) o.renderer = argv[++i];
        else if (!std::strcmp(a, "--shot") && i + 1 < argc) {
            const std::string s = argv[++i];
            const size_t colon = s.find(':');
            if (colon != std::string::npos) o.shots[std::strtol(s.c_str(), nullptr, 10)] = s.substr(colon + 1);
        } else if (a[0] != '-' && o.open.empty()) {
            o.open = a;
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
    enum class Mode { Tv, Recording, NotRecognized };

    bool setup_renderer();
    bool try_renderer(const std::string& name);
    bool create_texture();
    void destroy_renderer();
    void handle_event(const SDL_Event& ev);
    void handle_menu(int cmd);
    void tap(uint8_t button);
    void change_noise(float delta);
    bool slider_visible() const { return SDL_GetTicks() < slider_until_; }
    void toggle_fullscreen();
    void open_dialog();
    void open_path(const std::string& path);
    void insert_vhs(const std::string& path, const std::string& shown_name);
    void insert_test_tape();
    void show_not_recognized(const std::string& name, const std::string& reason);
    void update_recording();
    void update_title();
    void draw_overlay(Framebuffer& fb);
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
    std::string last_osd_, last_title_;
    bool fullscreen_ = false;
    bool running_ = true;
    Uint32 slider_until_ = 0;  // the noise slider is on screen until this time

    Mode mode_ = Mode::Tv;
    ImportJob job_;
    std::string recording_title_;
    std::string bad_name_, bad_reason_;
    Uint32 bad_until_ = 0;
    uint64_t play_at_frame_ = 0;  // press PLAY once the new tape has threaded
};

bool App::init() {
    make_dirs(user_data_dir());
    log_open(user_data_dir() + "retrotube90.log");
    log_info("RetroTube 90 %s starting", RT90_VERSION);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    window_ = SDL_CreateWindow("RetroTube 90", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kCanvasW, kCanvasH,
                               SDL_WINDOW_RESIZABLE);
    if (!window_ || !setup_renderer()) {
        log_error("SDL window/renderer setup failed: %s", SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RetroTube 90", "Could not open a video output.", window_);
        return false;
    }
    SDL_SetWindowMinimumSize(window_, kScreenW, kScreenH);
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    platform::install_menu(window_);

    SDL_AudioSpec want{}, have{};
    want.freq = kAudioRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    audio_dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);  // SDL converts to the device format
    if (audio_dev_) SDL_PauseAudioDevice(audio_dev_, 0);
    else log_warn("no audio device (%s); running silent", SDL_GetError());

    std::string err;
    if (!tv_.boot(&err)) {
        log_error("firmware failed to assemble:\n%s", err.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RetroTube 90", err.c_str(), window_);
        return false;
    }
    if (opt_.noise >= 0) tv_.set_noise(opt_.noise);
    if (opt_.show_slider) slider_until_ = 0xFFFFFFFFu;
    tv_.set_overlay([this](Framebuffer& fb) { draw_overlay(fb); });
    if (opt_.test_tape || opt_.autoplay) {
        tv_.insert_tape(std::make_shared<TestPatternSource>());
        if (opt_.autoplay) play_at_frame_ = 75;
    }
    if (!opt_.open.empty()) open_path(opt_.open);
    log_info("ready (audio %s, importer %s)", audio_dev_ ? "on" : "off", importer_available() ? "on" : "off");
    return true;
}

// ---- video output ----------------------------------------------------------------

// Tries SDL's renderers in turn (the default - Direct3D on Windows - then
// OpenGL, then software) and keeps the first one that is verified to really
// show our pixels. Some graphics drivers accept everything and then display a
// white or black window; the read-back test catches that.
bool App::setup_renderer() {
    std::vector<std::string> order;
    if (!opt_.renderer.empty()) order.push_back(opt_.renderer);
    order.insert(order.end(), {"", "opengl", "software"});
    for (const std::string& name : order) {
        if (try_renderer(name)) return true;
        destroy_renderer();
    }
    return false;
}

bool App::create_texture() {
    if (texture_) SDL_DestroyTexture(texture_);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kCanvasW, kCanvasH);
    if (!texture_) return false;
    SDL_SetTextureScaleMode(texture_, SDL_ScaleModeLinear);
    presented_ = ~0ull;  // re-upload the current frame
    return true;
}

bool App::try_renderer(const std::string& name) {
    int index = -1;
    if (!name.empty()) {
        for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
            SDL_RendererInfo info;
            if (SDL_GetRenderDriverInfo(i, &info) == 0 && name == info.name) index = i;
        }
        if (index < 0) return false;
    }
    const Uint32 flags = name == "software" ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_PRESENTVSYNC;
    renderer_ = SDL_CreateRenderer(window_, index, flags);
    if (!renderer_ || !create_texture()) {
        log_warn("renderer '%s' unavailable: %s", name.empty() ? "default" : name.c_str(), SDL_GetError());
        return false;
    }
    SDL_RendererInfo info{};
    SDL_GetRendererInfo(renderer_, &info);

    // Self-test: show a known colour and read it back from the back buffer.
    canvas_.fill(rgb(0x20, 0x90, 0x40));
    SDL_UpdateTexture(texture_, nullptr, canvas_.px.data(), kCanvasW * 4);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer_, &w, &h);
    SDL_Rect probe{w / 2, h / 2, 1, 1};
    Uint32 px = 0;
    const bool read_ok = SDL_RenderReadPixels(renderer_, &probe, SDL_PIXELFORMAT_ARGB8888, &px, 4) == 0;
    SDL_RenderPresent(renderer_);
    const bool match = std::abs(red(px) - 0x20) < 24 && std::abs(green(px) - 0x90) < 24 && std::abs(blue(px) - 0x40) < 24;
    if (read_ok && !match) {
        log_warn("renderer '%s' failed the display test (read back %06X); trying another", info.name,
                 static_cast<unsigned>(px & 0xFFFFFF));
        return false;
    }
    SDL_RenderSetLogicalSize(renderer_, kCanvasW, kCanvasH);  // keep 4:3, letterbox the rest
    log_info("renderer: %s (%s)", info.name, read_ok ? "display test passed" : "display test not supported");
    return true;
}

void App::destroy_renderer() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    texture_ = nullptr;
    renderer_ = nullptr;
}

// ---- input -------------------------------------------------------------------

void App::tap(uint8_t button) {
    tv_.machine().press(button);
    tv_.machine().release(button);
}

void App::change_noise(float delta) {
    const float v = std::round((tv_.noise() + delta) * 100.0f) / 100.0f;
    tv_.set_noise(v);
    slider_until_ = SDL_GetTicks() + 3000;
}

void App::toggle_fullscreen() {
    fullscreen_ = !fullscreen_;
    platform::set_menu_visible(window_, !fullscreen_);
    SDL_SetWindowFullscreen(window_, fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void App::handle_menu(int cmd) {
    switch (cmd) {
        case platform::MENU_OPEN: open_dialog(); break;
        case platform::MENU_EJECT: tap(io::BTN_EJECT); break;
        case platform::MENU_EXIT: running_ = false; break;
        case platform::MENU_PLAY: tap(io::BTN_PLAY); break;
        case platform::MENU_STOP: tap(io::BTN_STOP); break;
        case platform::MENU_REWIND: tap(io::BTN_REW); break;
        case platform::MENU_FFWD: tap(io::BTN_FF); break;
        case platform::MENU_TRACKING: tap(io::BTN_TRACK); break;
        case platform::MENU_NOISE: slider_until_ = SDL_GetTicks() + 3000; break;
        case platform::MENU_FULLSCREEN: toggle_fullscreen(); break;
        case platform::MENU_INTERLACE: tv_.interlace().enabled = !tv_.interlace().enabled; break;
        case platform::MENU_DEBUG: opt_.debug = !opt_.debug; break;
        case platform::MENU_CONTROLS:
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "RetroTube 90 - Controls", kControlsText, window_);
            break;
        case platform::MENU_ABOUT:
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "About RetroTube 90",
                                     "RetroTube 90 " RT90_VERSION "\nA 1990s TV + VCR emulator.\n\n"
                                     "Video decoding by FFmpeg (LGPL). Window and sound by SDL2.",
                                     window_);
            break;
        default: break;
    }
}

void App::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_QUIT) {
        running_ = false;
    } else if (ev.type == SDL_RENDER_DEVICE_RESET) {
        // The GPU driver was reset (sleep, display change...): textures are gone.
        log_warn("render device reset; recreating the screen texture");
        if (!create_texture()) {
            destroy_renderer();
            if (!setup_renderer()) running_ = false;
        }
    } else if (ev.type == SDL_RENDER_TARGETS_RESET) {
        presented_ = ~0ull;
    } else if (ev.type == platform::menu_event_type()) {
        handle_menu(ev.user.code);
    } else if (ev.type == SDL_DROPFILE) {
        if (ev.drop.file) {
            const std::string path = ev.drop.file;
            SDL_free(ev.drop.file);
            SDL_RaiseWindow(window_);  // take keyboard focus back from Explorer
            open_path(path);
        }
    } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
        const SDL_Keycode key = ev.key.keysym.sym;
        const bool ctrl = (ev.key.keysym.mod & KMOD_CTRL) != 0;
        if (mode_ == Mode::Recording) {
            if (key == SDLK_ESCAPE) job_.cancel();
            return;
        }
        if (mode_ == Mode::NotRecognized) {  // any key dismisses the screen
            mode_ = Mode::Tv;
            if (key == SDLK_ESCAPE) return;
        }
        if (key == SDLK_o && ctrl) {
            open_dialog();
        } else if (key == SDLK_f) {
            toggle_fullscreen();
        } else if (key == SDLK_F1) {
            opt_.debug = !opt_.debug;
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
        } else if (key == SDLK_l) {
            insert_test_tape();
        } else if (key == SDLK_SPACE && !tv_.has_tape()) {
            open_dialog();  // nothing to play yet: ask for a tape
        } else if (uint8_t b = key_to_button(key)) {
            tv_.machine().press(b);
        }
    } else if (ev.type == SDL_KEYUP) {
        if (uint8_t b = key_to_button(ev.key.keysym.sym)) tv_.machine().release(b);
    } else if (ev.type == SDL_MOUSEWHEEL) {
        change_noise(ev.wheel.y > 0 ? 0.05f : -0.05f);
    }
}

// ---- tapes ---------------------------------------------------------------------

void App::open_dialog() {
    std::string path;
    const bool picked = platform::open_file_dialog(window_, path);
    SDL_RaiseWindow(window_);
    if (picked) open_path(path);
}

void App::open_path(const std::string& path) {
    log_info("open: %s", path.c_str());
    if (mode_ == Mode::Recording) {
        job_.cancel();
        job_.take_result();
        tv_.volume = 1.0f;
        mode_ = Mode::Tv;
    }
    // Switch the set on if the user turned it off (the firmware powers up by
    // itself at reset, so leave it alone before the first frame).
    if (tv_.machine().frame() > 0 && !tv_.machine().power_led()) tap(io::BTN_POWER);
    const Detection d = detect_file(path);
    log_info("open: detected %s", file_kind_name(d.kind));
    switch (d.kind) {
        case FileKind::Vhs:
            insert_vhs(path, path_filename(path));
            break;
        case FileKind::Mp4:
            if (!importer_available()) {
                show_not_recognized(path_filename(path), "this build cannot record MP4s");
                break;
            }
            recording_title_ = path_stem(path);
            job_.start(path);
            mode_ = Mode::Recording;
            tv_.volume = 0.0f;  // the TV is quiet while the VCR records
            break;
        default:
            show_not_recognized(path_filename(path), d.reason);
            break;
    }
}

void App::insert_vhs(const std::string& path, const std::string& shown_name) {
    std::string err;
    auto src = VhsSource::open(path, &err);
    if (!src) {
        log_warn("open: %s: %s", path.c_str(), err.c_str());
        show_not_recognized(shown_name, err);
        return;
    }
    log_info("insert: \"%s\" (%u frames, %.1f s)", src->title().c_str(), src->frame_count(),
             src->frame_count() / kNtscFps);
    tv_.insert_tape(src);
    mode_ = Mode::Tv;
    // The firmware shows a second of static while the tape threads, then we
    // press PLAY (which adds the short burst of static).
    play_at_frame_ = std::max<uint64_t>(tv_.machine().frame() + 26, 40);  // after power-on static
}

void App::insert_test_tape() {
    if (mode_ == Mode::Recording) return;
    if (tv_.machine().frame() > 0 && !tv_.machine().power_led()) tap(io::BTN_POWER);
    log_info("insert: built-in test tape");
    tv_.insert_tape(std::make_shared<TestPatternSource>());
    mode_ = Mode::Tv;
    play_at_frame_ = std::max<uint64_t>(tv_.machine().frame() + 26, 40);
}

void App::show_not_recognized(const std::string& name, const std::string& reason) {
    log_warn("not recognized: %s (%s)", name.c_str(), reason.c_str());
    bad_name_ = name;
    bad_reason_ = reason;
    bad_until_ = SDL_GetTicks() + 6000;
    mode_ = Mode::NotRecognized;
}

void App::update_recording() {
    if (mode_ != Mode::Recording || !job_.done()) return;
    const ImportResult r = job_.take_result();
    tv_.volume = 1.0f;
    mode_ = Mode::Tv;
    if (r.ok) insert_vhs(r.vhs_path, path_filename(job_.source()));
    else if (!r.cancelled) show_not_recognized(path_filename(job_.source()), r.error);
}

void App::draw_overlay(Framebuffer& fb) {
    const uint64_t frame = tv_.machine().frame();
    if (mode_ == Mode::Recording) draw_recording_screen(fb, recording_title_, job_.progress(), frame);
    else if (mode_ == Mode::NotRecognized) draw_not_recognized(fb, bad_name_, bad_reason_);
    if (slider_visible() && mode_ == Mode::Tv) draw_noise_slider(fb, tv_.noise());
    // Empty deck: once the power-on snow settles, tell the user what to do.
    if (mode_ == Mode::Tv && !tv_.has_tape() && tv_.machine().power_led() && frame > 50 &&
        tv_.machine().gpu().screen() == io::SCREEN_STATIC && !slider_visible())
        draw_insert_hint(fb, frame);
}

void App::update_title() {
    std::string title = "RetroTube 90";
    if (mode_ == Mode::Recording) title += " - Recording " + recording_title_;
    else if (auto t = tv_.tape(); t && tv_.has_tape()) title += " - " + t->title();
    if (title != last_title_) {
        SDL_SetWindowTitle(window_, title.c_str());
        last_title_ = title;
    }
}

// ---- output ---------------------------------------------------------------------

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

        update_recording();
        if (mode_ == Mode::NotRecognized && SDL_GetTicks() > bad_until_) mode_ = Mode::Tv;
        Machine& m = tv_.machine();
        if (play_at_frame_ && m.frame() >= play_at_frame_) {
            if (tv_.has_tape()) tap(io::BTN_PLAY);
            play_at_frame_ = 0;
        }
        if (m.cpu().fault() != CpuFault::None) {
            log_error("VCR-1 fault: %s at $%04X (%s)", fault_name(m.cpu().fault()), m.cpu().fault_pc(),
                      m.cpu().describe().c_str());
            running_ = false;
        }
        const std::string osd = m.gpu().osd_text();
        if (osd != last_osd_) {
            log_info("frame %llu deck=%s OSD: %s", static_cast<unsigned long long>(m.frame()),
                     deck_state_name(m.deck().state()), osd_to_utf8(osd).c_str());
            last_osd_ = osd;
        }
        update_title();
        present();
        SDL_Delay(1);
        if (opt_.max_frames >= 0 && static_cast<long>(m.frame()) >= opt_.max_frames) running_ = false;
    }
    const Machine& m = tv_.machine();
    log_info("exit: %llu frames, %llu CPU cycles, %llu instructions, %s",
             static_cast<unsigned long long>(m.frame()), static_cast<unsigned long long>(m.cpu().total_cycles()),
             static_cast<unsigned long long>(m.cpu().instructions()), m.cpu().describe().c_str());
    return m.cpu().fault() == CpuFault::None ? 0 : 1;
}

void App::shutdown() {
    job_.cancel();
    if (audio_dev_) SDL_CloseAudioDevice(audio_dev_);
    audio_dev_ = 0;
    destroy_renderer();
    if (window_) SDL_DestroyWindow(window_);
    window_ = nullptr;
    SDL_Quit();
    log_close();
}

}  // namespace

int main(int argc, char** argv) {
    App app(parse_args(argc, argv));
    if (!app.init()) return 1;
    return app.run();
}
