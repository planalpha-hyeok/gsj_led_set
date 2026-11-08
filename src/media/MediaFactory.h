#pragma once

// 파일 확장자에 따라 알맞은 재생 소스 생성

#include "media/MediaSource.h"

#include <memory>
#include <string>

namespace led {

bool isStillImageFile(const std::string& path);

// open()은 호출하지 않은 상태로 반환
// hardware: 영상이면 그래픽카드 디코딩 시도 (불가능하면 자동으로 CPU)
std::shared_ptr<MediaSource> createMediaSource(const std::string& path, bool loop, bool hardware = false);

// 라이브 입력 장치. device가 "lavfi:"로 시작하면 FFmpeg 시험 신호 (예: "lavfi:testsrc2=size=1280x720:rate=30")
std::shared_ptr<MediaSource> createLiveSource(const std::string& device, const std::string& size, double fps);

} // namespace led
