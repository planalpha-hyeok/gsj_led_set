#pragma once

// 자석(스냅) 계산: 영역을 옮기거나 크기를 바꿀 때 가까운 기준선(캔버스/출력/다른 영역의 테두리와 가운데선)에 붙임
// 이동할 때는 영역의 왼쪽/가운데/오른쪽(위/가운데/아래) 중 가장 가까운 것이 붙음
// 표준 C++만 사용

#include "core/Geometry.h"

#include <vector>

namespace led {

enum SnapEdge : int {
    SnapLeft = 1,
    SnapRight = 2,
    SnapTop = 4,
    SnapBottom = 8,
};

struct SnapResult {
    RectI rect;              // 정수 픽셀로 맞춘 결과
    bool snappedX = false;
    bool snappedY = false;
    double guideX = 0.0;     // 붙은 세로 기준선 위치
    double guideY = 0.0;     // 붙은 가로 기준선 위치
};

// rect: 마우스로 계산한 위치/크기 (실수)
// move: true = 이동 (사각형 전체가 같이 움직임), false = edges에 해당하는 테두리만 움직임
// xs/ys: 기준선 좌표, threshold: 붙는 거리 (캔버스 픽셀). 0 이하이면 붙이지 않음
SnapResult snapRect(const RectF& rect, bool move, int edges, const std::vector<double>& xs,
                    const std::vector<double>& ys, double threshold);

} // namespace led
