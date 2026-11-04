#pragma once

// FFmpeg 기반 영상 재생 소스
// 별도 스레드에서 디코딩하고, 재생 시간에 맞는 프레임을 꺼내 줌

#include "media/MediaSource.h"

#include <cstdint>
#include <memory>
#include <string>

namespace led {

class VideoSource final : public MediaSource {
public:
    // hardware: 그래픽카드 디코딩 시도 (불가능하면 자동으로 CPU)
    VideoSource(std::string path, bool loop, bool hardware = false);
    ~VideoSource() override;

    VideoSource(const VideoSource&) = delete;
    VideoSource& operator=(const VideoSource&) = delete;

    bool open(std::string& error) override;
    std::shared_ptr<const Frame> frameAt(double seconds) override;
    bool waitForFirstFrame(int timeoutMs) override;
    void stop() override;
    const std::string& path() const override;
    double durationHint() const override;
    double endTime() const override;

    // 디코딩이 늦어 건너뛴 프레임 수 (검사/진단용)
    std::uint64_t droppedFrames() const;
    // 실제 사용 중인 디코더 ("CPU" 또는 하드웨어 종류)
    std::string decoderName() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace led
