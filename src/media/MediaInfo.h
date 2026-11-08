#pragma once

// 파일 정보 조회 (타임라인에 클립을 넣을 때 기본 길이 결정용)

#include <string>

namespace led {

struct MediaInfo {
    bool ok = false;
    bool still = false;      // 정지 이미지
    bool hasVideo = false;
    bool hasAudio = false;
    double duration = 0.0;   // 초 (모르면 0)
    int width = 0;
    int height = 0;
    std::string error;
};

MediaInfo probeMedia(const std::string& path);

// 같은 파일(경로+수정 시각)은 한 번만 조회
MediaInfo probeMediaCached(const std::string& path);

} // namespace led
