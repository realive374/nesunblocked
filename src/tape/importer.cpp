#include "tape/importer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

#include "av/timing.h"
#include "gpu/framebuffer.h"
#include "tape/jpeg.h"
#include "tape/vhs_format.h"
#include "util/file_util.h"
#include "util/log.h"

#ifdef RT90_HAVE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#endif

namespace rt90 {

std::string tape_path_for(const std::string& mp4_path) {
    const std::string next_to = replace_ext(mp4_path, ".vhs");
    // Probe that the folder is writable.
    const std::string probe = next_to + ".probe";
    if (FILE* f = fopen_utf8(probe, "wb")) {
        std::fclose(f);
        remove_file(probe);
        return next_to;
    }
    const std::string dir = user_data_dir() + "Tapes";
    make_dirs(dir);
    return dir + (dir.find('\\') != std::string::npos ? "\\" : "/") + path_stem(mp4_path) + ".vhs";
}

std::string find_existing_tape(const std::string& mp4_path) {
    const int64_t size = file_size(mp4_path);
    const int64_t mtime = file_mtime(mp4_path);
    const std::string dir = user_data_dir() + "Tapes";
    const std::string candidates[] = {replace_ext(mp4_path, ".vhs"),
                                      dir + (dir.find('\\') != std::string::npos ? "\\" : "/") +
                                          path_stem(mp4_path) + ".vhs"};
    for (const std::string& c : candidates) {
        if (!file_exists(c)) continue;
        VhsReader r;
        if (!r.open(c)) continue;
        if (static_cast<int64_t>(r.header().source_size) == size && r.header().source_mtime == mtime &&
            r.header().frame_count > 0)
            return c;
    }
    return {};
}

#ifndef RT90_HAVE_FFMPEG

bool importer_available() { return false; }

ImportResult import_mp4(const std::string&, const ImportOptions&, const std::atomic<bool>*, std::atomic<float>*) {
    ImportResult r;
    r.error = "this build cannot record MP4 files";
    return r;
}

#else

bool importer_available() { return true; }

namespace {

struct FormatCloser {
    void operator()(AVFormatContext* f) const { avformat_close_input(&f); }
};
struct CodecFree {
    void operator()(AVCodecContext* c) const { avcodec_free_context(&c); }
};
struct FrameFree {
    void operator()(AVFrame* f) const { av_frame_free(&f); }
};
struct PacketFree {
    void operator()(AVPacket* p) const { av_packet_free(&p); }
};
struct SwrFree {
    void operator()(SwrContext* s) const { swr_free(&s); }
};
struct SwsFree {
    void operator()(SwsContext* s) const { sws_freeContext(s); }
};

std::string av_err(int code) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, buf, sizeof buf);
    return buf;
}

std::unique_ptr<AVCodecContext, CodecFree> open_decoder(AVStream* st, bool video) {
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec) return nullptr;
    std::unique_ptr<AVCodecContext, CodecFree> ctx(avcodec_alloc_context3(codec));
    if (!ctx || avcodec_parameters_to_context(ctx.get(), st->codecpar) < 0) return nullptr;
    ctx->pkt_timebase = st->time_base;
    if (video) {
        ctx->thread_count = 0;  // auto: one per core
        ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        ctx->skip_loop_filter = AVDISCARD_NONREF;  // invisible at 320x240, saves time
    }
    if (avcodec_open2(ctx.get(), codec, nullptr) < 0) return nullptr;
    return ctx;
}

// Converts decoded pictures into letterboxed 320x240 frames.
class Scaler {
public:
    bool convert(const AVFrame* f, Framebuffer& out) {
        if (f->width <= 0 || f->height <= 0) return false;
        if (!sws_ || f->width != src_w_ || f->height != src_h_ || f->format != src_fmt_) setup(f);
        if (!sws_) return false;
        uint8_t* dst[4] = {rgb_.data(), nullptr, nullptr, nullptr};
        int stride[4] = {dst_w_ * 3, 0, 0, 0};
        sws_scale(sws_.get(), f->data, f->linesize, 0, f->height, dst, stride);
        out.resize(kScreenW, kScreenH, 0xFF000000u);
        for (int y = 0; y < dst_h_; ++y) {
            const uint8_t* s = rgb_.data() + static_cast<size_t>(y) * static_cast<size_t>(dst_w_) * 3;
            uint32_t* d = out.row(off_y_ + y) + off_x_;
            for (int x = 0; x < dst_w_; ++x) d[x] = rgb(s[x * 3], s[x * 3 + 1], s[x * 3 + 2]);
        }
        return true;
    }

private:
    void setup(const AVFrame* f) {
        src_w_ = f->width;
        src_h_ = f->height;
        src_fmt_ = f->format;
        double sar = 1.0;
        if (f->sample_aspect_ratio.num > 0 && f->sample_aspect_ratio.den > 0) sar = av_q2d(f->sample_aspect_ratio);
        const double dar = static_cast<double>(src_w_) * sar / src_h_;
        const double tape = 4.0 / 3.0;
        if (dar >= tape) {  // widescreen: letterbox
            dst_w_ = kScreenW;
            dst_h_ = std::clamp(static_cast<int>(std::lround(kScreenW / dar / 2)) * 2, 2, kScreenH);
        } else {  // tall: pillarbox
            dst_h_ = kScreenH;
            dst_w_ = std::clamp(static_cast<int>(std::lround(kScreenH * dar / 2)) * 2, 2, kScreenW);
        }
        off_x_ = (kScreenW - dst_w_) / 2;
        off_y_ = (kScreenH - dst_h_) / 2;
        rgb_.assign(static_cast<size_t>(dst_w_) * static_cast<size_t>(dst_h_) * 3, 0);
        sws_.reset(sws_getContext(src_w_, src_h_, static_cast<AVPixelFormat>(src_fmt_), dst_w_, dst_h_,
                                  AV_PIX_FMT_RGB24, SWS_AREA, nullptr, nullptr, nullptr));
        if (!sws_) return;
        // Use the right YUV->RGB matrix (HD video is BT.709) and range.
        const int cs = (f->colorspace == AVCOL_SPC_BT709 || (f->colorspace == AVCOL_SPC_UNSPECIFIED && src_h_ >= 720))
                           ? SWS_CS_ITU709
                           : SWS_CS_DEFAULT;
        const int full_range = f->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
        sws_setColorspaceDetails(sws_.get(), sws_getCoefficients(cs), full_range, sws_getCoefficients(SWS_CS_DEFAULT),
                                 1, 0, 1 << 16, 1 << 16);
    }

    std::unique_ptr<SwsContext, SwsFree> sws_;
    int src_w_ = 0, src_h_ = 0, src_fmt_ = -1;
    int dst_w_ = 0, dst_h_ = 0, off_x_ = 0, off_y_ = 0;
    std::vector<uint8_t> rgb_;
};

// Collects the converted audio timeline and pairs it with video frames.
class Recorder {
public:
    Recorder(VhsWriter& w, int quality) : writer_(w), quality_(quality) {}

    void add_audio(const int16_t* s, size_t n) { audio_.insert(audio_.end(), s, s + n); }
    void audio_done() { audio_done_ = true; }
    uint64_t audio_total() const { return audio_read_ + audio_.size(); }

    // Queues the picture for the next output frame.
    bool push_picture(const Framebuffer& fb) {
        std::vector<uint8_t> jpeg;
        if (!encode_jpeg(fb, quality_, jpeg)) return false;
        pending_.push_back(std::move(jpeg));
        return flush(false);
    }
    // Writes queued frames whose audio has arrived (all of them if `force`).
    bool flush(bool force) {
        while (!pending_.empty()) {
            const uint32_t need = audio_samples_in_frame(written_);
            const bool have_audio = audio_.size() >= need;
            // Don't let the picture run too far ahead of a lagging audio track.
            if (!have_audio && !force && !audio_done_ && pending_.size() < 600) break;
            slice_.assign(need, 0);
            const size_t take = std::min<size_t>(need, audio_.size());
            std::copy(audio_.begin(), audio_.begin() + static_cast<std::ptrdiff_t>(take), slice_.begin());
            audio_.erase(audio_.begin(), audio_.begin() + static_cast<std::ptrdiff_t>(take));
            audio_read_ += take;
            if (!writer_.add_frame(pending_.front(), slice_.data(), slice_.size())) return false;
            pending_.pop_front();
            ++written_;
        }
        return true;
    }
    uint64_t frames_queued() const { return written_ + pending_.size(); }

private:
    VhsWriter& writer_;
    int quality_;
    std::deque<int16_t> audio_;
    uint64_t audio_read_ = 0;
    bool audio_done_ = false;
    std::deque<std::vector<uint8_t>> pending_;
    std::vector<int16_t> slice_;
    uint64_t written_ = 0;
};

}  // namespace

ImportResult import_mp4(const std::string& mp4_path, const ImportOptions& opt, const std::atomic<bool>* cancel,
                        std::atomic<float>* progress) {
    ImportResult res;
    auto cancelled = [&] { return cancel && cancel->load(); };
    auto set_progress = [&](float p) {
        if (progress) progress->store(std::clamp(p, 0.0f, 1.0f));
    };
    av_log_set_level(AV_LOG_ERROR);

    if (opt.reuse_existing && opt.out_path.empty()) {
        const std::string existing = find_existing_tape(mp4_path);
        if (!existing.empty()) {
            VhsReader r;
            r.open(existing);
            res.ok = res.reused = true;
            res.vhs_path = existing;
            res.title = r.header().title;
            res.frames = r.header().frame_count;
            set_progress(1);
            log_info("import: reusing up-to-date tape %s", existing.c_str());
            return res;
        }
    }

    AVFormatContext* raw_fmt = nullptr;
    int rc = avformat_open_input(&raw_fmt, mp4_path.c_str(), nullptr, nullptr);
    if (rc < 0) {
        res.error = "the video could not be opened";
        log_error("import: open %s: %s", mp4_path.c_str(), av_err(rc).c_str());
        return res;
    }
    std::unique_ptr<AVFormatContext, FormatCloser> fmt(raw_fmt);
    if ((rc = avformat_find_stream_info(fmt.get(), nullptr)) < 0) {
        res.error = "the video is damaged";
        log_error("import: stream info: %s", av_err(rc).c_str());
        return res;
    }
    const int vi = av_find_best_stream(fmt.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (vi < 0) {
        res.error = "the file has no video";
        return res;
    }
    const int ai = av_find_best_stream(fmt.get(), AVMEDIA_TYPE_AUDIO, -1, vi, nullptr, 0);
    AVStream* vst = fmt->streams[vi];
    AVStream* ast = ai >= 0 ? fmt->streams[ai] : nullptr;
    auto vdec = open_decoder(vst, true);
    if (!vdec) {
        res.error = "this video format is not supported";
        log_error("import: no decoder for video codec %s", avcodec_get_name(vst->codecpar->codec_id));
        return res;
    }
    std::unique_ptr<AVCodecContext, CodecFree> adec;
    std::unique_ptr<SwrContext, SwrFree> swr;
    if (ast) {
        adec = open_decoder(ast, false);
        if (adec) {
            AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
            SwrContext* s = nullptr;
            if (swr_alloc_set_opts2(&s, &mono, AV_SAMPLE_FMT_S16, kAudioRate, &adec->ch_layout, adec->sample_fmt,
                                    adec->sample_rate, 0, nullptr) >= 0 &&
                swr_init(s) >= 0) {
                swr.reset(s);
            } else {
                swr_free(&s);
                adec.reset();
            }
        }
        if (!adec) log_warn("import: audio track not supported, recording without sound");
    }
    for (unsigned i = 0; i < fmt->nb_streams; ++i)
        if (static_cast<int>(i) != vi && (!adec || static_cast<int>(i) != ai)) fmt->streams[i]->discard = AVDISCARD_ALL;

    // Timeline: zero is the file's start time.
    const int64_t start_us = fmt->start_time != AV_NOPTS_VALUE ? fmt->start_time : 0;
    double duration = fmt->duration > 0 ? static_cast<double>(fmt->duration) / AV_TIME_BASE : 0.0;
    if (duration <= 0 && vst->duration > 0) duration = static_cast<double>(vst->duration) * av_q2d(vst->time_base);
    double limit = duration;
    if (opt.max_seconds > 0) limit = duration > 0 ? std::min(duration, opt.max_seconds) : opt.max_seconds;

    res.vhs_path = opt.out_path.empty() ? tape_path_for(mp4_path) : opt.out_path;
    AVDictionaryEntry* tag = av_dict_get(fmt->metadata, "title", nullptr, 0);
    res.title = tag && tag->value && *tag->value ? tag->value : path_stem(mp4_path);

    VhsWriter writer;
    std::string err;
    if (!writer.open(res.vhs_path, &err)) {
        res.error = "cannot save the tape next to the video";
        log_error("import: %s", err.c_str());
        return res;
    }
    log_info("import: %s -> %s (%.1f s, video %s %dx%d, audio %s)", mp4_path.c_str(), res.vhs_path.c_str(),
             duration, avcodec_get_name(vst->codecpar->codec_id), vst->codecpar->width, vst->codecpar->height,
             adec ? avcodec_get_name(ast->codecpar->codec_id) : "none");

    Recorder rec(writer, opt.jpeg_quality);
    if (!adec) rec.audio_done();
    Scaler scaler;
    Framebuffer picture(kScreenW, kScreenH), next(kScreenW, kScreenH);
    bool have_picture = false;
    uint64_t out_frame = 0;                       // next tape frame to emit
    double last_video_t = 0, video_frame_dur = 1.0 / kNtscFps;
    bool audio_started = false;
    std::vector<int16_t> abuf;
    bool failed = false, stop = false;

    std::unique_ptr<AVPacket, PacketFree> pkt(av_packet_alloc());
    std::unique_ptr<AVFrame, FrameFree> frame(av_frame_alloc());
    auto frame_time = [](uint64_t n) { return static_cast<double>(n) * kFpsDen / kFpsNum; };

    auto on_video = [&](AVFrame* f) {
        const int64_t ts = f->best_effort_timestamp != AV_NOPTS_VALUE ? f->best_effort_timestamp : f->pts;
        const double t = ts != AV_NOPTS_VALUE
                             ? static_cast<double>(ts) * av_q2d(vst->time_base) - static_cast<double>(start_us) / 1e6
                             : last_video_t + video_frame_dur;
        if (limit > 0 && t >= limit) {
            stop = true;
            return;
        }
        if (!scaler.convert(f, next)) return;
        if (have_picture && t > last_video_t) video_frame_dur = std::min(0.5, t - last_video_t);
        // Output frame n shows the newest source picture that started by n's time.
        if (have_picture) {
            while (frame_time(out_frame) < t - 1e-6 && !failed) {
                failed = !rec.push_picture(picture);
                ++out_frame;
            }
        }
        std::swap(picture, next);
        have_picture = true;
        last_video_t = std::max(last_video_t, t);
        if (duration > 0) set_progress(static_cast<float>(t / (limit > 0 ? limit : duration)) * 0.99f);
    };

    auto on_audio = [&](AVFrame* f) {
        if (!audio_started) {
            audio_started = true;
            // Align the first sample with the timeline (pad or trim).
            if (f->pts != AV_NOPTS_VALUE) {
                const double t = static_cast<double>(f->pts) * av_q2d(ast->time_base) -
                                 static_cast<double>(start_us) / 1e6;
                if (t > 0.0005) {
                    std::vector<int16_t> silence(static_cast<size_t>(t * kAudioRate), 0);
                    rec.add_audio(silence.data(), silence.size());
                } else if (t < -0.0005) {
                    swr_drop_output(swr.get(), static_cast<int>(-t * kAudioRate));
                }
            }
        }
        const int max_out = swr_get_out_samples(swr.get(), f->nb_samples);
        if (max_out <= 0) return;
        abuf.resize(static_cast<size_t>(max_out));
        uint8_t* out[1] = {reinterpret_cast<uint8_t*>(abuf.data())};
        const int n = swr_convert(swr.get(), out, max_out, const_cast<const uint8_t**>(f->extended_data), f->nb_samples);
        if (n > 0) rec.add_audio(abuf.data(), static_cast<size_t>(n));
    };

    auto drain = [&](AVCodecContext* dec, bool video) {
        while (avcodec_receive_frame(dec, frame.get()) == 0) {
            if (video) on_video(frame.get());
            else on_audio(frame.get());
            av_frame_unref(frame.get());
            if (failed || stop) return;
        }
    };

    while (!failed && !stop) {
        if (cancelled()) {
            writer.abort();
            res.cancelled = true;
            res.error = "recording cancelled";
            return res;
        }
        rc = av_read_frame(fmt.get(), pkt.get());
        if (rc < 0) break;  // end of file (or a read error: keep what we have)
        if (pkt->stream_index == vi) {
            if (avcodec_send_packet(vdec.get(), pkt.get()) >= 0) drain(vdec.get(), true);
        } else if (adec && pkt->stream_index == ai) {
            if (avcodec_send_packet(adec.get(), pkt.get()) >= 0) drain(adec.get(), false);
        }
        av_packet_unref(pkt.get());
        if (!failed && !rec.flush(false)) failed = true;
    }
    // Flush the decoders.
    if (!failed) {
        avcodec_send_packet(vdec.get(), nullptr);
        drain(vdec.get(), true);
        if (adec) {
            avcodec_send_packet(adec.get(), nullptr);
            drain(adec.get(), false);
            const int left = swr_get_out_samples(swr.get(), 0);
            if (left > 0) {
                abuf.resize(static_cast<size_t>(left));
                uint8_t* out[1] = {reinterpret_cast<uint8_t*>(abuf.data())};
                const int n = swr_convert(swr.get(), out, left, nullptr, 0);
                if (n > 0) rec.add_audio(abuf.data(), static_cast<size_t>(n));
            }
        }
    }
    rec.audio_done();
    if (!have_picture) {
        writer.abort();
        res.error = "no pictures could be decoded";
        return res;
    }
    // Hold the last picture until both the picture and the sound have ended.
    double end_t = last_video_t + video_frame_dur;
    end_t = std::max(end_t, static_cast<double>(rec.audio_total()) / kAudioRate);
    if (duration > 0) end_t = std::min(end_t, duration + 0.5);
    if (limit > 0) end_t = std::min(end_t, limit);
    const uint64_t total = std::max<uint64_t>(out_frame + 1, static_cast<uint64_t>(std::ceil(end_t * kNtscFps - 1e-6)));
    while (!failed && out_frame < total) {
        failed = !rec.push_picture(picture);
        ++out_frame;
    }
    if (!failed) failed = !rec.flush(true);
    if (failed) {
        writer.abort();
        res.error = "the tape could not be written (disk full?)";
        return res;
    }

    VhsHeader h;
    h.title = res.title;
    h.jpeg_quality = static_cast<uint8_t>(std::clamp(opt.jpeg_quality, 1, 100));
    h.source_size = static_cast<uint64_t>(std::max<int64_t>(0, file_size(mp4_path)));
    h.source_mtime = file_mtime(mp4_path);
    if (!writer.finish(h, &err)) {
        res.error = "the tape could not be written";
        log_error("import: %s", err.c_str());
        return res;
    }
    res.ok = true;
    res.frames = static_cast<uint32_t>(out_frame);
    set_progress(1);
    log_info("import: done, %u frames (%.1f s)", res.frames, res.frames / kNtscFps);
    return res;
}

#endif  // RT90_HAVE_FFMPEG

ImportJob::~ImportJob() {
    cancel();
    if (thread_.joinable()) thread_.join();
}

void ImportJob::start(const std::string& mp4_path, const ImportOptions& opt) {
    if (thread_.joinable()) {
        cancel();
        thread_.join();
    }
    cancel_ = false;
    done_ = false;
    progress_ = 0;
    source_ = mp4_path;
    result_ = ImportResult{};
    thread_ = std::thread([this, mp4_path, opt] {
        result_ = import_mp4(mp4_path, opt, &cancel_, &progress_);
        done_ = true;
    });
}

ImportResult ImportJob::take_result() {
    if (thread_.joinable()) thread_.join();
    return result_;
}

}  // namespace rt90
