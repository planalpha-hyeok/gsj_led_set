#pragma once

// 재생 제어 스레드 -> 출력 스레드로 넘기는 "지금 영역에 보여줄 것" 정보
// 실제 프레임 선택과 투명도/자막 위치 계산은 출력 스레드에서 시각에 맞춰 수행 (부드러운 움직임)

#include "core/Geometry.h"
#include "core/Project.h"
#include "media/MediaSource.h"
#include "media/TickerStrip.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace led {

using PlaybackClock = std::chrono::steady_clock;

// 플레이리스트 항목 1개의 재생 상태
struct ClipView {
    std::shared_ptr<MediaSource> source;
    PlaybackClock::time_point start;      // 항목 재생 시작 시각
    PlaybackClock::time_point fadeStart;  // 페이드 인 시작 시각
    double fadeSeconds = 0.0;             // 0 = 즉시 표시
    std::uint64_t key = 0;                // 텍스처 구분용 고유 번호
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
    std::vector<ClipView> clips;          // 아래 -> 위 순서 (페이드 중에는 이전 항목 + 새 항목)
    std::shared_ptr<const TickerView> ticker;
};

std::uint64_t nextRenderKey();

} // namespace led
