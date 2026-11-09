#pragma once

// 파일 정보 조회 (타임라인에 클립을 넣을 때 기본 길이 결정용)

#include "core/Project.h"

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

// 자동 길이 클립의 실제 길이 (packLayer 의 DurationResolver 로 사용)
//   파일: 영상/소리 = 파일 길이 - 자르기 지점, 이미지 = 10초 / 라이브 1시간 / 자막 60초 / 알 수 없음 = 0
double resolveClipDuration(const TimelineClip& clip);

} // namespace led
