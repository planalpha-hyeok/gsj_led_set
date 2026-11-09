#pragma once

// 재생 제어 스레드 -> 출력 스레드로 넘기는 "지금 영역(레이어)에 보여줄 것" 정보
// 실제 프레임 선택과 투명도/자막 위치 계산은 출력 스레드에서 시각에 맞춰 수행 (부드러운 움직임)

#include "core/Geometry.h"
#include "core/Project.h"
#include "media/MediaSource.h"
#include "media/TickerStrip.h"
#include "playback/TimelineClock.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace led {

using PlaybackClock = std::chrono::steady_clock;

// 타임라인 클립의 페이드 정보 (출력 스레드에서 타임라인 시계로 투명도 계산)
struct ClipFade {
    std::shared_ptr<const TimelineClock> timeline;
    double clipStart = 0.0;
    double clipEnd = 0.0;                 // 화면 표시 끝 (계속 표시 = 매우 큰 값)
    double fadeIn = 0.0;
    double fadeOut = 0.0;
    double clipOpacity = 1.0;

    // 현재 투명도 (클립 밖이면 0)
    double alphaAt(double timelineTime) const;
};

// 클립 1개의 재생 상태
struct ClipView {
    std::shared_ptr<MediaSource> source;
    std::uint64_t key = 0;                // 텍스처 구분용 고유 번호
    ClipFade fade;
    double sourceZero = 0.0;              // 소스 시각 0에 해당하는 타임라인 시각
};

struct TickerView {
    std::shared_ptr<const TickerStrip> strip;
    std::shared_ptr<const Frame> background;   // 배경 없으면 nullptr
    PlaybackClock::time_point start;
    double speed = 0.0;
    TickerDirection direction = TickerDirection::Left;
    std::uint64_t key = 0;
};

struct RegionView {
    RectI rect;
    FitMode fit = FitMode::Stretch;
    float opacity = 1.0f;
    std::vector<ClipView> clips;          // 아래 -> 위 순서 (페이드 중에는 이전 클립 + 새 클립)
    std::shared_ptr<const TickerView> ticker;
    ClipFade tickerFade;                  // 자막 클립의 페이드
};

std::uint64_t nextRenderKey();

} // namespace led
