#pragma once

// FFmpeg 기반 영상 재생 소스
// 별도 스레드에서 디코딩하고, 재생 시간에 맞는 프레임을 꺼내 줌

#include "media/MediaSource.h"

#include <memory>
#include <string>

namespace led {

class VideoSource final : public MediaSource {
public:
    VideoSource(std::string path, bool loop);
    ~VideoSource() override;

    VideoSource(const VideoSource&) = delete;
    VideoSource& operator=(const VideoSource&) = delete;

    bool open(std::string& error) override;
    std::shared_ptr<const Frame> frameAt(double seconds) override;
    bool waitForFirstFrame(int timeoutMs) override;
    void stop() override;
    const std::string& path() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace led
