#pragma once

// 자석(스냅) 계산: 영역을 옮기거나 크기를 바꿀 때 가까운 기준선(캔버스/출력/다른 영역의 테두리와 가운데선)에 붙임
// 이동할 때는 영역의 왼쪽/가운데/오른쪽(위/가운데/아래) 중 가장 가까운 것이 붙음
// 기준선 그룹 우선순위: 앞쪽 그룹(예: 캔버스)에서 붙을 수 있으면 뒤쪽 그룹은 보지 않음 (가로/세로 따로)
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

// 기준선 그룹 1개
struct SnapLines {
    std::vector<double> xs;  // 세로 기준선 (x 좌표)
    std::vector<double> ys;  // 가로 기준선 (y 좌표)
};

// rect: 마우스로 계산한 위치/크기 (실수)
// move: true = 이동 (사각형 전체가 같이 움직임), false = edges에 해당하는 테두리만 움직임
// groups: 우선순위 순서의 기준선 그룹, threshold: 붙는 거리 (캔버스 픽셀). 0 이하이면 붙이지 않음
// 크기 변경 중 붙으면 너비/높이가 1px 미만이 되는 경우에는 붙이지 않음
SnapResult snapRect(const RectF& rect, bool move, int edges, const std::vector<SnapLines>& groups, double threshold);

// 그룹 1개 (우선순위 없음)
SnapResult snapRect(const RectF& rect, bool move, int edges, const std::vector<double>& xs,
                    const std::vector<double>& ys, double threshold);

} // namespace led
