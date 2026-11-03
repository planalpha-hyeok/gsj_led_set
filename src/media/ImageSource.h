#pragma once

// 정지 이미지 소스 (PNG, JPG, BMP 등)

#include "media/MediaSource.h"

namespace led {

class ImageSource final : public MediaSource {
public:
    explicit ImageSource(std::string path);

    bool open(std::string& error) override;
    std::shared_ptr<const Frame> frameAt(double seconds) override;
    bool waitForFirstFrame(int timeoutMs) override;
    void stop() override {}
    const std::string& path() const override { return m_path; }
    bool isStill() const override { return true; }

private:
    std::string m_path;
    std::shared_ptr<const Frame> m_frame;
};

} // namespace led
