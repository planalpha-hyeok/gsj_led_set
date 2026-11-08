#include "media/VideoSource.h"

#include "core/Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
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
    std::call_once(flag, [] {
        av_log_set_level(AV_LOG_ERROR);
        avdevice_register_all(); // 캡처보드/웹캠 입력
    });
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

// 하드웨어 디코딩 장치 후보 (앞쪽 우선)
const AVHWDeviceType kHardwareTypes[] = {
#if defined(_WIN32)
    AV_HWDEVICE_TYPE_D3D11VA,
    AV_HWDEVICE_TYPE_DXVA2,
#elif defined(__APPLE__)
    AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
#else
    AV_HWDEVICE_TYPE_VAAPI,
    AV_HWDEVICE_TYPE_VDPAU,
#endif
};

// 하드웨어 장치는 종류별로 1개만 만들어 모든 영상이 함께 사용 (프로그램 종료 시까지 유지)
AVBufferRef* sharedHardwareDevice(AVHWDeviceType type)
{
    static std::mutex mutex;
    static std::map<AVHWDeviceType, AVBufferRef*> devices;
    static std::map<AVHWDeviceType, bool> failed;
    std::lock_guard<std::mutex> lock(mutex);
    auto it = devices.find(type);
    if (it != devices.end())
        return it->second;
    if (failed[type])
        return nullptr;
    AVBufferRef* device = nullptr;
    const int r = av_hwdevice_ctx_create(&device, type, nullptr, nullptr, 0);
    if (r < 0) {
        failed[type] = true;
        logInfo(std::string("하드웨어 디코딩 장치 사용 불가: ") + av_hwdevice_get_type_name(type) + " (" +
                ffmpegError(r) + ")");
        return nullptr;
    }
    devices[type] = device;
    logInfo(std::string("하드웨어 디코딩 장치 준비: ") + av_hwdevice_get_type_name(type));
    return device;
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
    bool hardware = false;           // 하드웨어 디코딩 요청
    bool live = false;
    std::string inputFormat;
    std::string liveSize;
    double liveFps = 0.0;
    double startTime = 0.0;          // 재생 시작 위치(초). 프레임 시각 0 = 이 위치
    AVPixelFormat hwPixelFormat = AV_PIX_FMT_NONE;
    std::string hwName;              // 사용 중인 하드웨어 종류 (비어 있으면 CPU)
    std::atomic<bool> hwFallback{false};
    int hwTransferErrors = 0;

    // 재생 시계 (출력이 요청한 최신 시각). 디코딩이 늦으면 프레임을 건너뛰는 기준
    std::atomic<double> requestedTime{-1.0};
    std::atomic<std::uint64_t> dropped{0};

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
        closeInput();
    }

    void closeInput()
    {
        if (codec)
            avcodec_free_context(&codec);
        if (format)
            avformat_close_input(&format);
        streamIndex = -1;
    }

    // 읽기 대기 중에도 정지 요청이 오면 즉시 빠져나오게 함 (라이브 장치가 응답하지 않을 때 대비)
    static int interruptCallback(void* opaque)
    {
        return static_cast<Impl*>(opaque)->stopRequested.load() ? 1 : 0;
    }

    static AVPixelFormat chooseFormat(AVCodecContext* ctx, const AVPixelFormat* formats)
    {
        auto* self = static_cast<Impl*>(ctx->opaque);
        for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p)
            if (*p == self->hwPixelFormat)
                return *p;
        // 이 영상은 하드웨어 디코딩 불가: CPU 형식 선택
        for (const AVPixelFormat* p = formats; *p != AV_PIX_FMT_NONE; ++p) {
            const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*p);
            if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
                if (!self->hwFallback.exchange(true))
                    logWarning("하드웨어 디코딩을 지원하지 않는 영상이라 CPU로 디코딩: " + self->path);
                return *p;
            }
        }
        return AV_PIX_FMT_NONE;
    }

    bool setupHardware(const AVCodec* decoder)
    {
        for (AVHWDeviceType type : kHardwareTypes) {
            AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
            for (int i = 0;; ++i) {
                const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
                if (!config)
                    break;
                if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == type) {
                    pixelFormat = config->pix_fmt;
                    break;
                }
            }
            if (pixelFormat == AV_PIX_FMT_NONE)
                continue;
            AVBufferRef* device = sharedHardwareDevice(type);
            if (!device)
                continue;
            codec->hw_device_ctx = av_buffer_ref(device);
            if (!codec->hw_device_ctx)
                continue;
            hwPixelFormat = pixelFormat;
            hwName = av_hwdevice_get_type_name(type);
            codec->opaque = this;
            codec->get_format = &Impl::chooseFormat;
            return true;
        }
        return false;
    }

    bool open(std::string& error)
    {
        initFfmpegOnce();
        std::string decoderInfo;
        if (!openInput(error, decoderInfo))
            return false;
        if (!live && startTime > 0.0) {
            if (!rewind())
                logWarning("시작 위치로 이동하지 못해 처음부터 재생: " + path);
            if (durationHint > 0.0)
                durationHint = std::max(0.0, durationHint - startTime);
        }
        logInfo((live ? "라이브 입력 열기: " : "영상 열기: ") + path + " (" + decoderInfo +
                (startTime > 0.0 ? ", " + std::to_string(startTime) + "초부터" : std::string()) + ")");
        thread = std::thread([this] { run(); });
        return true;
    }

    bool openInput(std::string& error, std::string& decoderInfo)
    {
        closeInput(); // 재연결 시 이전 상태 정리
        format = avformat_alloc_context();
        if (!format) {
            error = "메모리 할당 실패";
            return false;
        }
        format->interrupt_callback.callback = &Impl::interruptCallback;
        format->interrupt_callback.opaque = this;

        const AVInputFormat* inputFmt = nullptr;
        std::string url = path;
        AVDictionary* options = nullptr;
        if (live) {
            inputFmt = av_find_input_format(inputFormat.c_str());
            if (!inputFmt) {
                avformat_free_context(format);
                format = nullptr;
                error = "라이브 입력 방식을 지원하지 않음: " + inputFormat;
                return false;
            }
            if (inputFormat == "dshow") {
                url = "video=" + path;
                av_dict_set(&options, "rtbufsize", "128M", 0);
            }
            if (!liveSize.empty())
                av_dict_set(&options, "video_size", liveSize.c_str(), 0);
            if (liveFps > 0.0)
                av_dict_set(&options, "framerate", std::to_string(liveFps).c_str(), 0);
        }

        int r = avformat_open_input(&format, url.c_str(), inputFmt, &options);
        av_dict_free(&options);
        if (r < 0) {
            error = std::string(live ? "입력 장치를 열 수 없음 (" : "파일을 열 수 없음 (") + ffmpegError(r) + ")";
            format = nullptr; // 실패 시 FFmpeg가 해제함
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
        const bool hwReady = hardware && !live && setupHardware(decoder);
        if (hardware && !hwReady)
            logInfo("하드웨어 디코딩을 쓸 수 없어 CPU로 디코딩: " + path);
        if (hwReady) {
            codec->thread_count = 1; // 하드웨어 디코딩은 단일 스레드가 안정적
        } else {
            codec->thread_count = 0; // 자동 (CPU 코어 수)
            codec->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
        }
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
        if (live)
            durationHint = -1.0;
        else if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0)
            durationHint = stream->duration * timeBase;
        else if (format->duration != AV_NOPTS_VALUE && format->duration > 0)
            durationHint = static_cast<double>(format->duration) / AV_TIME_BASE;

        const AVRational rate = av_guess_frame_rate(format, stream, nullptr);
        if (rate.num > 0 && rate.den > 0)
            defaultFrameDuration = double(rate.den) / double(rate.num);

        decoderInfo = std::to_string(codec->width) + "x" + std::to_string(codec->height) + ", " + decoder->name +
                      ", " + (hwReady ? "하드웨어 " + hwName : std::string("CPU"));
        return true;
    }

    // 라이브 입력이 끊겼을 때: 닫고 2초마다 다시 엶. 정지 요청 시 false
    bool reconnect()
    {
        closeInput();
        logWarning("라이브 입력 끊김, 2초마다 다시 연결 시도: " + path);
        while (!stopRequested) {
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait_for(lock, std::chrono::seconds(2), [this] { return stopRequested.load(); });
            }
            if (stopRequested)
                return false;
            std::string error;
            std::string info;
            if (openInput(error, info)) {
                logInfo("라이브 입력 다시 연결됨: " + path + " (" + info + ")");
                return true;
            }
        }
        return false;
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
        if (live) {
            // 라이브: 가장 최근 프레임 1장만 보관 (기다리지 않음)
            std::lock_guard<std::mutex> lock(mutex);
            if (stopRequested)
                return false;
            queue.clear();
            queue.push_back(Queued{time, std::move(frame)});
            cv.notify_all();
            return true;
        }
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return stopRequested.load() || queue.size() < kMaxQueuedFrames; });
        if (stopRequested)
            return false;
        queue.push_back(Queued{time, std::move(frame)});
        cv.notify_all();
        return true;
    }

    // 시작 위치(startTime)로 이동. 그 앞의 프레임은 run()에서 버림
    bool rewind()
    {
        const int64_t target = streamStart + static_cast<int64_t>(startTime / timeBase);
        int r = av_seek_frame(format, streamIndex, target, AVSEEK_FLAG_BACKWARD);
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

        AVFrame* swFrame = av_frame_alloc();
        std::uint64_t droppedSinceLog = 0;
        auto lastDropLog = std::chrono::steady_clock::now();
        double lastPushedTime = -1.0;

        double loopOffset = 0.0;   // 반복 재생 시 누적 시간
        double lastEnd = 0.0;      // 마지막 프레임이 끝나는 시간
        double lastTime = -1.0;
        int framesThisLoop = 0;
        bool draining = false;
        bool done = false;

        while (!done && !stopRequested) {
            if (!draining) {
                const int r = av_read_frame(format, packet);
                if (live && r < 0 && r != AVERROR(EAGAIN)) {
                    // 라이브 입력 끊김: 재연결
                    if (stopRequested || !reconnect())
                        break;
                    continue;
                }
                if (live && r == AVERROR(EAGAIN)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }
                if (r == AVERROR_EOF || (r < 0 && format->pb && avio_feof(format->pb))) {
                    avcodec_send_packet(codec, nullptr); // 디코더에 남은 프레임 배출
                    draining = true;
                } else if (r < 0) {
                    if (!stopRequested) // 정지 요청으로 중단된 경우는 오류 아님
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
                if (ts != AV_NOPTS_VALUE) {
                    const double mediaTime = (ts - streamStart) * timeBase;
                    // 시작 위치 앞의 프레임(키프레임부터 디코딩된 부분)은 표시하지 않음
                    if (startTime > 0.0 && mediaTime + duration * 0.5 < startTime) {
                        av_frame_unref(frame);
                        continue;
                    }
                    t = loopOffset + std::max(0.0, mediaTime - startTime);
                } else
                    t = lastTime < 0.0 ? loopOffset : lastTime + defaultFrameDuration;
                if (t < lastTime) // 시간 역행 방지
                    t = lastTime;
                lastTime = t;
                lastEnd = std::max(lastEnd, t + duration);
                ++framesThisLoop;

                // 디코딩이 재생 시각보다 늦으면 따라잡기
                //  - 2프레임 이상 늦은 프레임은 변환하지 않고 버림 (단, 0.5초마다 1장은 표시)
                //  - 0.5초 이상 늦으면 참조되지 않는 프레임은 디코딩 자체를 생략
                const double clock = live ? -1.0 : requestedTime.load();
                const double lag = clock >= 0.0 ? clock - t : 0.0;
                codec->skip_frame = lag > 0.5 ? AVDISCARD_NONREF : AVDISCARD_DEFAULT;
                const double dropThreshold = std::max(0.05, 2.0 * defaultFrameDuration);
                if (lag > dropThreshold && lastPushedTime >= 0.0 && t - lastPushedTime < 0.5) {
                    av_frame_unref(frame);
                    ++dropped;
                    ++droppedSinceLog;
                    const auto now = std::chrono::steady_clock::now();
                    if (now - lastDropLog > std::chrono::seconds(5)) {
                        logWarning("영상 처리가 재생 속도보다 느려 프레임 " + std::to_string(droppedSinceLog) +
                                   "개를 건너뜀: " + path);
                        droppedSinceLog = 0;
                        lastDropLog = now;
                    }
                    continue;
                }

                // 하드웨어 프레임은 CPU 메모리로 복사 후 변환
                const AVFrame* source = frame;
                if (hwPixelFormat != AV_PIX_FMT_NONE && frame->format == hwPixelFormat) {
                    av_frame_unref(swFrame);
                    const int tr = av_hwframe_transfer_data(swFrame, frame, 0);
                    if (tr < 0) {
                        if (hwTransferErrors++ < 3)
                            logError("하드웨어 프레임 복사 실패: " + path + " (" + ffmpegError(tr) + ")");
                        av_frame_unref(frame);
                        continue;
                    }
                    av_frame_copy_props(swFrame, frame);
                    source = swFrame;
                }

                std::shared_ptr<Frame> converted = convert(source);
                av_frame_unref(frame);
                if (!converted)
                    continue;
                lastPushedTime = t;
                if (!push(t, std::move(converted)))
                    break;
            }
        }

        av_packet_free(&packet);
        av_frame_free(&frame);
        av_frame_free(&swFrame);
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

VideoSource::VideoSource(std::string path, bool loop, bool hardware)
    : d(std::make_unique<Impl>())
{
    d->path = std::move(path);
    d->loop = loop;
    d->hardware = hardware;
}

VideoSource::VideoSource(std::string path, VideoOptions options)
    : d(std::make_unique<Impl>())
{
    d->path = std::move(path);
    d->loop = options.loop;
    d->hardware = options.hardware;
    d->live = options.live;
    d->inputFormat = options.inputFormat;
    d->liveSize = options.liveSize;
    d->liveFps = options.liveFps;
    d->startTime = std::max(0.0, options.startTime);
}

std::uint64_t VideoSource::droppedFrames() const
{
    return d->dropped.load();
}

std::string VideoSource::decoderName() const
{
    return d->hwName.empty() || d->hwFallback ? std::string("CPU") : d->hwName;
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
    if (d->live) {
        // 라이브: 시각과 관계없이 가장 최근 프레임
        std::lock_guard<std::mutex> lock(d->mutex);
        if (!d->queue.empty()) {
            d->current = d->queue.back().frame;
            d->queue.clear();
        }
        return d->current;
    }
    // 가장 앞선 요청 시각을 재생 시계로 사용
    double prev = d->requestedTime.load();
    while (seconds > prev && !d->requestedTime.compare_exchange_weak(prev, seconds)) {
    }
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
