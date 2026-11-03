#pragma once

// 흐르는 자막 위치 계산 (표준 C++만 사용)
// 자막 띠(strip)는 [문구 + 간격]을 이어 붙인 것이며, 끊김 없이 반복됨

#include "core/Project.h"

#include <vector>

namespace led {

// distance: 시작 후 이동한 거리(픽셀)
// regionStart/regionLength: 흐르는 방향 축의 영역 시작 좌표와 길이
// stripLength: 띠 한 개의 길이
// out: 화면에 보이는 띠 사본들의 시작 좌표 (흐르는 방향 축)
// 처음에는 영역이 비어 있고 문구가 영역 끝에서 들어옴
void tickerCopyPositions(double distance, double regionStart, double regionLength, double stripLength,
                         TickerDirection direction, std::vector<double>& out);

inline bool isVerticalTicker(TickerDirection d)
{
    return d == TickerDirection::Up || d == TickerDirection::Down;
}

} // namespace led
