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

} // namespace led
