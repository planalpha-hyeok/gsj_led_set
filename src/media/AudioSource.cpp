#include "media/AudioSource.h"

#include "core/Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace led {

namespace {

constexpr std::size_t kBufferFrames = AudioSource::kSampleRate; // 1초 분량

std::string ffmpegError(int code)
{
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(code, buf, sizeof(buf));
    return buf;
}

} // namespace

struct AudioSource::Impl {
    std::string path;
    double startTime = 0.0;
    bool loop = false;

    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    SwrContext* swr = nullptr;
    int streamIndex = -1;
    double timeBase = 0.0;
    int64_t streamStart = 0;

    std::thread thread;
    std::atomic<bool> stopRequested{false};

    // 원형 버퍼 (스테레오 interleaved)
    mutable std::mutex mutex;
    std::condition_variable cv;
    std::vector<float> ring = std::vector<float>(kBufferFrames * kChannels);
    std::size_t readPos = 0;  // 프레임 단위
    std::size_t count = 0;    // 쌓인 프레임 수
    bool finished = false;
    std::atomic<std::uint64_t> consumed{0};

    ~Impl()
    {
        if (swr)
            swr_free(&swr);
        if (codec)
            avcodec_free_context(&codec);
        if (format)
            avformat_close_input(&format);
    }

    static int interruptCallback(void* opaque)
    {
        return static_cast<Impl*>(opaque)->stopRequested.load() ? 1 : 0;
    }

    bool open(std::string& error)
    {
        format = avformat_alloc_context();
        if (!format) {
            error = "메모리 할당 실패";
            return false;
        }
        format->interrupt_callback.callback = &Impl::interruptCallback;
        format->interrupt_callback.opaque = this;
        int r = avformat_open_input(&format, path.c_str(), nullptr, nullptr);
        if (r < 0) {
            format = nullptr;
            error = "파일을 열 수 없음 (" + ffmpegError(r) + ")";
            return false;
        }
        if ((r = avformat_find_stream_info(format, nullptr)) < 0) {
            error = "스트림 정보를 읽을 수 없음 (" + ffmpegError(r) + ")";
            return false;
        }
        streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (streamIndex < 0) {
            error = "소리 없음";
            return false;
        }
        AVStream* stream = format->streams[streamIndex];
        const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!decoder) {
            error = std::string("지원하지 않는 소리 코덱: ") + avcodec_get_name(stream->codecpar->codec_id);
            return false;
        }
        codec = avcodec_alloc_context3(decoder);
        if (!codec || avcodec_parameters_to_context(codec, stream->codecpar) < 0 ||
            avcodec_open2(codec, decoder, nullptr) < 0) {
            error = "소리 디코더를 열 수 없음";
            return false;
        }
        for (unsigned i = 0; i < format->nb_streams; ++i)
            if (int(i) != streamIndex)
                format->streams[i]->discard = AVDISCARD_ALL;

        timeBase = av_q2d(stream->time_base);
        streamStart = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;

        AVChannelLayout inLayout;
        if (codec->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC || codec->ch_layout.nb_channels <= 0)
            av_channel_layout_default(&inLayout, std::max(1, codec->ch_layout.nb_channels));
        else
            av_channel_layout_copy(&inLayout, &codec->ch_layout);
        AVChannelLayout outLayout;
        av_channel_layout_default(&outLayout, kChannels);
        r = swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_FLT, kSampleRate, &inLayout, codec->sample_fmt,
                                codec->sample_rate, 0, nullptr);
        av_channel_layout_uninit(&inLayout);
        av_channel_layout_uninit(&outLayout);
        if (r < 0 || swr_init(swr) < 0) {
            error = "소리 변환기를 만들 수 없음";
            return false;
        }

        if (startTime > 0.0)
            seekToStart();
        thread = std::thread([this] { run(); });
        return true;
    }

    bool seekToStart()
    {
        const int64_t target = streamStart + static_cast<int64_t>(startTime / timeBase);
        int r = av_seek_frame(format, streamIndex, target, AVSEEK_FLAG_BACKWARD);
        if (r < 0)
            r = avformat_seek_file(format, -1, INT64_MIN, 0, INT64_MAX, 0);
        avcodec_flush_buffers(codec);
        return r >= 0;
    }

    // 변환된 샘플을 버퍼에 넣음 (가득 차면 기다림)
    bool pushSamples(const float* data, std::size_t frames)
    {
        while (frames > 0) {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this] { return stopRequested.load() || count < kBufferFrames; });
            if (stopRequested)
                return false;
            const std::size_t space = kBufferFrames - count;
            std::size_t n = std::min(space, frames);
            std::size_t writePos = (readPos + count) % kBufferFrames;
            while (n > 0) {
                const std::size_t chunk = std::min(n, kBufferFrames - writePos);
                std::memcpy(&ring[writePos * kChannels], data, chunk * kChannels * sizeof(float));
                data += chunk * kChannels;
                frames -= chunk;
                count += chunk;
                n -= chunk;
                writePos = (writePos + chunk) % kBufferFrames;
            }
        }
        return true;
    }

    void run()
    {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        std::vector<float> converted;
        bool draining = false;
        bool firstAfterSeek = startTime > 0.0;
        int framesThisLoop = 0;

        while (!stopRequested && packet && frame) {
            if (!draining) {
                const int r = av_read_frame(format, packet);
                if (r < 0) {
                    avcodec_send_packet(codec, nullptr);
                    draining = true;
                } else {
                    if (packet->stream_index == streamIndex)
                        avcodec_send_packet(codec, packet);
                    av_packet_unref(packet);
                }
            }
            bool reachedEnd = false;
            while (!stopRequested) {
                const int r = avcodec_receive_frame(codec, frame);
                if (r == AVERROR(EAGAIN) && !draining)
                    break;
                if (r < 0) {
                    reachedEnd = draining;
                    break;
                }
                // 시작 위치 앞부분 버림
                std::size_t skipOut = 0;
                if (firstAfterSeek && frame->pts != AV_NOPTS_VALUE && frame->sample_rate > 0) {
                    const double mediaTime = (frame->pts - streamStart) * timeBase;
                    const double frameEnd = mediaTime + double(frame->nb_samples) / frame->sample_rate;
                    if (frameEnd <= startTime) {
                        av_frame_unref(frame);
                        continue;
                    }
                    if (mediaTime < startTime)
                        skipOut = static_cast<std::size_t>(std::llround((startTime - mediaTime) * kSampleRate));
                    firstAfterSeek = false;
                }
                const int maxOut = swr_get_out_samples(swr, frame->nb_samples);
                converted.resize(static_cast<std::size_t>(std::max(0, maxOut)) * kChannels);
                uint8_t* outPtr = reinterpret_cast<uint8_t*>(converted.data());
                const int got = swr_convert(swr, &outPtr, maxOut, const_cast<const uint8_t**>(frame->extended_data),
                                            frame->nb_samples);
                av_frame_unref(frame);
                if (got <= 0)
                    continue;
                ++framesThisLoop;
                const std::size_t n = static_cast<std::size_t>(got);
                if (skipOut < n && !pushSamples(converted.data() + skipOut * kChannels, n - skipOut))
                    break;
            }
            if (reachedEnd) {
                if (loop && framesThisLoop > 0 && seekToStart()) {
                    draining = false;
                    firstAfterSeek = startTime > 0.0;
                    framesThisLoop = 0;
                    continue;
                }
                break;
            }
        }
        av_packet_free(&packet);
        av_frame_free(&frame);
        std::lock_guard<std::mutex> lock(mutex);
        finished = true;
        cv.notify_all();
    }
};

AudioSource::AudioSource(std::string path, double startTime, bool loop)
    : d(std::make_unique<Impl>())
{
    d->path = std::move(path);
    d->startTime = std::max(0.0, startTime);
    d->loop = loop;
}

AudioSource::~AudioSource()
{
    stop();
}

bool AudioSource::open(std::string& error)
{
    return d->open(error);
}

void AudioSource::stop()
{
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->stopRequested = true;
        d->cv.notify_all();
    }
    if (d->thread.joinable())
        d->thread.join();
}

std::size_t AudioSource::read(float* out, std::size_t frames)
{
    std::lock_guard<std::mutex> lock(d->mutex);
    const std::size_t n = std::min(frames, d->count);
    std::size_t done = 0;
    while (done < n) {
        const std::size_t chunk = std::min(n - done, kBufferFrames - d->readPos);
        std::memcpy(out + done * kChannels, &d->ring[d->readPos * kChannels], chunk * kChannels * sizeof(float));
        d->readPos = (d->readPos + chunk) % kBufferFrames;
        d->count -= chunk;
        done += chunk;
    }
    d->consumed += n;
    if (n > 0)
        d->cv.notify_all();
    return n;
}

void AudioSource::skip(double seconds)
{
    if (seconds <= 0.0)
        return;
    std::lock_guard<std::mutex> lock(d->mutex);
    const std::size_t want = static_cast<std::size_t>(seconds * kSampleRate);
    const std::size_t n = std::min(want, d->count);
    d->readPos = (d->readPos + n) % kBufferFrames;
    d->count -= n;
    d->consumed += n;
    if (n > 0)
        d->cv.notify_all();
}

double AudioSource::position() const
{
    return static_cast<double>(d->consumed.load()) / kSampleRate;
}

bool AudioSource::finished() const
{
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->finished && d->count == 0;
}

const std::string& AudioSource::path() const
{
    return d->path;
}

} // namespace led
