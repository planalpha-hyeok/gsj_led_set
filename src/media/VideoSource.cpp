#include "media/VideoSource.h"

#include "core/Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace led {

namespace {

constexpr std::size_t kMaxQueuedFrames = 4;

std::string ffmpegError(int code)
{
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(code, buf, sizeof(buf));
    return std::string(buf);
}

void initFfmpegOnce()
{
    static std::once_flag flag;
    std::call_once(flag, [] { av_log_set_level(AV_LOG_ERROR); });
}

// 사용 중단된 yuvj* 포맷을 일반 포맷 + 풀레인지로 변환
AVPixelFormat normalizePixelFormat(AVPixelFormat fmt, int& fullRange)
{
    switch (fmt) {
    case AV_PIX_FMT_YUVJ420P: fullRange = 1; return AV_PIX_FMT_YUV420P;
    case AV_PIX_FMT_YUVJ422P: fullRange = 1; return AV_PIX_FMT_YUV422P;
    case AV_PIX_FMT_YUVJ444P: fullRange = 1; return AV_PIX_FMT_YUV444P;
    case AV_PIX_FMT_YUVJ440P: fullRange = 1; return AV_PIX_FMT_YUV440P;
    default: return fmt;
    }
}

int swsColorspace(AVColorSpace cs, int height)
{
    switch (cs) {
    case AVCOL_SPC_BT709: return SWS_CS_ITU709;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M: return SWS_CS_ITU601;
    case AVCOL_SPC_SMPTE240M: return SWS_CS_SMPTE240M;
    case AVCOL_SPC_FCC: return SWS_CS_FCC;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL: return SWS_CS_BT2020;
    default: return height >= 720 ? SWS_CS_ITU709 : SWS_CS_ITU601;
    }
}

int64_t frameDurationTicks(const AVFrame* f)
{
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 30, 100)
    return f->duration;
#else
    return f->pkt_duration;
#endif
}

} // namespace

struct VideoSource::Impl {
    struct Queued {
        double time;
        std::shared_ptr<const Frame> frame;
    };

    std::string path;
    bool loop = true;

    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    SwsContext* sws = nullptr;
    int streamIndex = -1;
    double timeBase = 0.0;
    int64_t streamStart = 0;
    double defaultFrameDuration = 1.0 / 30.0;
    double durationHint = -1.0;

    SwsContext* configuredSws = nullptr;
    int configuredColorspace = -1;
    int configuredRange = -1;

    std::thread thread;
    std::atomic<bool> stopRequested{false};

    std::mutex mutex;
    std::condition_variable cv;
    std::deque<Queued> queue;
    std::shared_ptr<const Frame> current;
    bool finished = false;
    double endTime = -1.0;   // 반복 없이 끝났을 때 마지막 프레임이 끝나는 시각

    ~Impl()
    {
        if (sws)
            sws_freeContext(sws);
        if (codec)
            avcodec_free_context(&codec);
        if (format)
            avformat_close_input(&format);
    }

    bool open(std::string& error)
    {
        initFfmpegOnce();

        int r = avformat_open_input(&format, path.c_str(), nullptr, nullptr);
        if (r < 0) {
            error = "파일을 열 수 없음 (" + ffmpegError(r) + ")";
            format = nullptr;
            return false;
        }
        r = avformat_find_stream_info(format, nullptr);
        if (r < 0) {
            error = "스트림 정보를 읽을 수 없음 (" + ffmpegError(r) + ")";
            return false;
        }
        streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (streamIndex < 0) {
            error = "영상 스트림이 없음";
            return false;
        }

        AVStream* stream = format->streams[streamIndex];
        const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!decoder) {
            error = std::string("지원하지 않는 코덱: ") + avcodec_get_name(stream->codecpar->codec_id);
            return false;
        }
        codec = avcodec_alloc_context3(decoder);
        if (!codec) {
            error = "디코더 메모리 할당 실패";
            return false;
        }
        r = avcodec_parameters_to_context(codec, stream->codecpar);
        if (r < 0) {
            error = "디코더 설정 실패 (" + ffmpegError(r) + ")";
            return false;
        }
        codec->thread_count = 0; // 자동 (CPU 코어 수)
        codec->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        r = avcodec_open2(codec, decoder, nullptr);
        if (r < 0) {
            error = "디코더를 열 수 없음 (" + ffmpegError(r) + ")";
            return false;
        }

        for (unsigned i = 0; i < format->nb_streams; ++i)
            if (int(i) != streamIndex)
                format->streams[i]->discard = AVDISCARD_ALL;

        timeBase = av_q2d(stream->time_base);
        streamStart = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;
        if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0)
            durationHint = stream->duration * timeBase;
        else if (format->duration != AV_NOPTS_VALUE && format->duration > 0)
            durationHint = static_cast<double>(format->duration) / AV_TIME_BASE;

        const AVRational rate = av_guess_frame_rate(format, stream, nullptr);
        if (rate.num > 0 && rate.den > 0)
            defaultFrameDuration = double(rate.den) / double(rate.num);

        logInfo("영상 열기: " + path + " (" + std::to_string(codec->width) + "x" +
                std::to_string(codec->height) + ", " + decoder->name + ")");

        thread = std::thread([this] { run(); });
        return true;
    }

    std::shared_ptr<Frame> convert(const AVFrame* f)
    {
        int fullRange = f->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
        const AVPixelFormat srcFormat = normalizePixelFormat(static_cast<AVPixelFormat>(f->format), fullRange);
        const int w = f->width;
        const int h = f->height;
        if (w <= 0 || h <= 0)
            return nullptr;

        sws = sws_getCachedContext(sws, w, h, srcFormat, w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr,
                                   nullptr);
        if (!sws)
            return nullptr;

        const int colorspace = swsColorspace(f->colorspace, h);
        if (sws != configuredSws || colorspace != configuredColorspace || fullRange != configuredRange) {
            sws_setColorspaceDetails(sws, sws_getCoefficients(colorspace), fullRange,
                                     sws_getCoefficients(SWS_CS_DEFAULT), 1, 0, 1 << 16, 1 << 16);
            configuredSws = sws;
            configuredColorspace = colorspace;
            configuredRange = fullRange;
        }

        auto out = std::make_shared<Frame>();
        out->width = w;
        out->height = h;
        out->stride = (w * 4 + 63) & ~63;
        out->pixels.resize(static_cast<std::size_t>(out->stride) * static_cast<std::size_t>(h));

        uint8_t* dst[4] = {out->pixels.data(), nullptr, nullptr, nullptr};
        int dstStride[4] = {out->stride, 0, 0, 0};
        sws_scale(sws, reinterpret_cast<const uint8_t* const*>(f->data), f->linesize, 0, h, dst, dstStride);

        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(srcFormat);
        out->premultiplied = !(desc && (desc->flags & AV_PIX_FMT_FLAG_ALPHA));
        out->serial = nextFrameSerial();
        return out;
    }

    bool push(double time, std::shared_ptr<const Frame> frame)
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return stopRequested.load() || queue.size() < kMaxQueuedFrames; });
        if (stopRequested)
            return false;
        queue.push_back(Queued{time, std::move(frame)});
        cv.notify_all();
        return true;
    }

    bool rewind()
    {
        int r = av_seek_frame(format, streamIndex, streamStart, AVSEEK_FLAG_BACKWARD);
        if (r < 0)
            r = avformat_seek_file(format, -1, INT64_MIN, 0, INT64_MAX, 0);
        if (r < 0) {
            logError("영상 처음으로 이동 실패: " + path + " (" + ffmpegError(r) + ")");
            return false;
        }
        avcodec_flush_buffers(codec);
        return true;
    }

    void run()
    {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        if (!packet || !frame) {
            logError("영상 디코딩 메모리 할당 실패: " + path);
            av_packet_free(&packet);
            av_frame_free(&frame);
            {
                std::lock_guard<std::mutex> lock(mutex);
                endTime = 0.0;
            }
            markFinished();
            return;
        }

        double loopOffset = 0.0;   // 반복 재생 시 누적 시간
        double lastEnd = 0.0;      // 마지막 프레임이 끝나는 시간
        double lastTime = -1.0;
        int framesThisLoop = 0;
        bool draining = false;
        bool done = false;

        while (!done && !stopRequested) {
            if (!draining) {
                const int r = av_read_frame(format, packet);
                if (r == AVERROR_EOF || (r < 0 && format->pb && avio_feof(format->pb))) {
                    avcodec_send_packet(codec, nullptr); // 디코더에 남은 프레임 배출
                    draining = true;
                } else if (r < 0) {
                    logError("영상 읽기 오류: " + path + " (" + ffmpegError(r) + ")");
                    break;
                } else {
                    if (packet->stream_index == streamIndex) {
                        const int s = avcodec_send_packet(codec, packet);
                        if (s < 0 && s != AVERROR(EAGAIN) && s != AVERROR_INVALIDDATA)
                            logWarning("영상 패킷 처리 오류: " + path + " (" + ffmpegError(s) + ")");
                    }
                    av_packet_unref(packet);
                }
            }

            while (!stopRequested) {
                const int r = avcodec_receive_frame(codec, frame);
                if (r == AVERROR(EAGAIN) && !draining)
                    break;
                // 배출 중에는 EAGAIN/오류도 끝으로 처리 (무한 반복 방지)
                if (r == AVERROR_EOF || (draining && r < 0)) {
                    if (r != AVERROR_EOF && r != AVERROR(EAGAIN))
                        logWarning("영상 끝 처리 중 오류: " + path + " (" + ffmpegError(r) + ")");
                    if (!loop || framesThisLoop == 0 || !rewind()) {
                        done = true;
                        std::lock_guard<std::mutex> lock(mutex);
                        endTime = lastEnd;
                    } else {
                        loopOffset = lastEnd;
                        framesThisLoop = 0;
                        draining = false;
                    }
                    break;
                }
                if (r < 0) {
                    logWarning("영상 디코딩 오류: " + path + " (" + ffmpegError(r) + ")");
                    break;
                }

                // 프레임 표시 시간 계산
                int64_t ts = frame->best_effort_timestamp;
                if (ts == AV_NOPTS_VALUE)
                    ts = frame->pts;
                double duration = defaultFrameDuration;
                const int64_t durTicks = frameDurationTicks(frame);
                if (durTicks > 0)
                    duration = durTicks * timeBase;

                double t;
                if (ts != AV_NOPTS_VALUE)
                    t = loopOffset + std::max(0.0, (ts - streamStart) * timeBase);
                else
                    t = lastTime < 0.0 ? loopOffset : lastTime + defaultFrameDuration;
                if (t < lastTime) // 시간 역행 방지
                    t = lastTime;
                lastTime = t;
                lastEnd = std::max(lastEnd, t + duration);

                std::shared_ptr<Frame> converted = convert(frame);
                av_frame_unref(frame);
                if (!converted)
                    continue;
                ++framesThisLoop;
                if (!push(t, std::move(converted)))
                    break;
            }
        }

        av_packet_free(&packet);
        av_frame_free(&frame);
        if (!stopRequested) {
            // 오류로 중단된 경우에도 끝난 시각을 알려 플레이리스트가 다음 항목으로 넘어가게 함
            std::lock_guard<std::mutex> lock(mutex);
            if (endTime < 0.0)
                endTime = lastEnd;
        }
        markFinished();
    }

    void markFinished()
    {
        std::lock_guard<std::mutex> lock(mutex);
        finished = true;
        cv.notify_all();
    }
};

VideoSource::VideoSource(std::string path, bool loop)
    : d(std::make_unique<Impl>())
{
    d->path = std::move(path);
    d->loop = loop;
}

VideoSource::~VideoSource()
{
    stop();
}

bool VideoSource::open(std::string& error)
{
    return d->open(error);
}

std::shared_ptr<const Frame> VideoSource::frameAt(double seconds)
{
    std::lock_guard<std::mutex> lock(d->mutex);
    bool popped = false;
    while (!d->queue.empty() && d->queue.front().time <= seconds) {
        d->current = d->queue.front().frame;
        d->queue.pop_front();
        popped = true;
    }
    if (!d->current && !d->queue.empty())
        d->current = d->queue.front().frame; // 첫 프레임은 즉시 표시
    if (popped)
        d->cv.notify_all();
    return d->current;
}

bool VideoSource::waitForFirstFrame(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(d->mutex);
    d->cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                   [this] { return !d->queue.empty() || d->current || d->finished; });
    return !d->queue.empty() || d->current;
}

void VideoSource::stop()
{
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->stopRequested = true;
        d->cv.notify_all();
    }
    if (d->thread.joinable())
        d->thread.join();
}

const std::string& VideoSource::path() const
{
    return d->path;
}

double VideoSource::durationHint() const
{
    return d->durationHint;
}

double VideoSource::endTime() const
{
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->endTime;
}

} // namespace led
