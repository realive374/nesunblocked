#include "app/tv_set.h"

#include <algorithm>
#include <cmath>

#include "av/timing.h"
#include "vcr/io_ports.h"

namespace rt90 {

TvSet::TvSet() {
    gpu_.set_pass(PassSlot::Snow, &snow_);
    gpu_.set_pass(PassSlot::Field, &interlace_);
    gpu_.set_pass(PassSlot::TapeSignal, &vhs_);
    machine_.set_vsync_callback([this] { on_vsync(); });
}

TvSet::~TvSet() {
    machine_.set_vsync_callback(nullptr);
    streamer_.detach();
}

bool TvSet::boot(std::string* error) { return machine_.boot_default(error); }

void TvSet::set_noise(float level) {
    vhs_.set_intensity(level);
    vhs_audio_.set_intensity(level);
}

void TvSet::insert_tape(std::shared_ptr<TapeSource> source) {
    if (!source) return;
    Deck& deck = machine_.deck();
    if (deck.has_tape()) deck.eject();
    vram_.invalidate();
    TapeInfo info;
    info.title = source->title();
    info.path = source->path();
    info.frame_count = source->frame_count();
    info.fps = source->fps();
    streamer_.attach(std::move(source));
    deck.insert(info);
}

void TvSet::take_audio(std::vector<int16_t>& out) {
    out.insert(out.end(), audio_.begin(), audio_.end());
    audio_.clear();
}

void TvSet::on_vsync() {
    // The firmware fires FX_TRACKING when the TRACKING button is pressed.
    const uint32_t tracking = machine_.gpu().tracking_requests();
    if (tracking != tracking_seen_) {
        tracking_seen_ = tracking;
        vhs_.start_tracking();
    }
    const DeckState st = machine_.deck().state();
    vhs_.set_search(st == DeckState::FastForward || st == DeckState::Rewinding);

    TapeFrame* played = nullptr;
    update_tape_picture(played);
    gpu_.compose(machine_.gpu(), vram_.front_frame() == Vram::kNone ? nullptr : &vram_.front(), machine_.frame(),
                 overlay_);
    ++composed_;
    produce_audio(played);
}

// Pulls the tape frame under the head into VRAM. Sets `played` when a new
// frame was read at normal play speed (its audio should be heard).
void TvSet::update_tape_picture(TapeFrame*& played) {
    const Deck& deck = machine_.deck();
    if (!deck.has_tape()) {
        if (streamer_.attached()) streamer_.detach();  // the firmware ejected it
        vram_.invalidate();
        return;
    }
    const DeckState st = deck.state();
    if (st == DeckState::Stopped || deck.tape().frame_count == 0) {
        vram_.invalidate();
        return;
    }
    const uint32_t idx = std::min(deck.position(), deck.tape().frame_count - 1);
    if (vram_.front_frame() == idx) return;  // paused / end of tape: hold the picture
    int stride = 1;
    if (st == DeckState::FastForward) stride = deck.shuttle_speed();
    if (st == DeckState::Rewinding) stride = -static_cast<int>(deck.shuttle_speed());
    if (!streamer_.fetch(idx, stride, frame_)) return;
    if (!decode_video(frame_, vram_.back())) return;
    vram_.swap(idx);
    if (st == DeckState::Playing) played = &frame_;
}

void TvSet::produce_audio(const TapeFrame* played) {
    const uint8_t screen = machine_.gpu().screen();
    // Each vsync produces exactly its share of the 22050 Hz timeline; while
    // playing, the tape frame's own audio slice is used so sound stays locked
    // to the picture.
    size_t n = audio_samples_in_frame(machine_.frame() - 1);
    if (played && screen == io::SCREEN_TAPE && !played->audio.empty()) n = played->audio.size();
    mix_.assign(n, 0.0f);
    if (played && screen == io::SCREEN_TAPE) {
        vhs_audio_.process(played->audio.data(), std::min(n, played->audio.size()), mix_.data());
    } else if (screen == io::SCREEN_STATIC) {
        hiss_.mix(mix_.data(), n);
    }
    const size_t base = audio_.size();
    audio_.resize(base + n);
    for (size_t i = 0; i < n; ++i) {
        const float v = std::clamp(mix_[i] * volume, -1.0f, 1.0f);
        audio_[base + i] = static_cast<int16_t>(std::lround(v * 32767.0f));
    }
    audio_total_ += n;
}

}  // namespace rt90
