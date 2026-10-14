// "Recording": converts an MP4 into a .vhs tape with FFmpeg.
//
// The picture is scaled into 320x240 (letterboxed / pillarboxed to keep its
// shape), resampled to exactly 29.97 fps and JPEG-compressed at quality ~60.
// The sound is mixed down to mono 16-bit PCM at 22.05 kHz and cut into
// per-frame slices so it stays locked to the picture.
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace rt90 {

struct ImportOptions {
    int jpeg_quality = 60;
    std::string out_path;     // empty: next to the original (or the user data folder)
    double max_seconds = 0;   // > 0: only record this much (tests)
    bool reuse_existing = true;  // play an up-to-date .vhs from an earlier recording
};

struct ImportResult {
    bool ok = false;
    bool cancelled = false;
    bool reused = false;  // an existing up-to-date tape was used
    std::string error;
    std::string vhs_path;
    std::string title;
    uint32_t frames = 0;
};

bool importer_available();  // false if built without FFmpeg

// Where the .vhs for `mp4_path` goes: "<same folder>/<name>.vhs", or the user
// data folder if that folder is not writable.
std::string tape_path_for(const std::string& mp4_path);
// Finds an existing, valid .vhs recorded from this exact MP4 (same size and
// modification time). Returns its path or "".
std::string find_existing_tape(const std::string& mp4_path);

// Synchronous conversion. `cancel` and `progress` (0-1) may be null.
ImportResult import_mp4(const std::string& mp4_path, const ImportOptions& opt = {},
                        const std::atomic<bool>* cancel = nullptr, std::atomic<float>* progress = nullptr);

// Runs import_mp4 on a background thread.
class ImportJob {
public:
    ~ImportJob();
    void start(const std::string& mp4_path, const ImportOptions& opt = {});
    void cancel() { cancel_ = true; }
    bool running() const { return thread_.joinable() && !done_; }
    bool done() const { return done_; }
    float progress() const { return progress_; }
    // Waits for the thread and returns the result (call once done()).
    ImportResult take_result();
    const std::string& source() const { return source_; }

private:
    std::thread thread_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> done_{false};
    std::atomic<float> progress_{0};
    ImportResult result_;
    std::string source_;
};

}  // namespace rt90
