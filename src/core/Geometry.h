#pragma once

// 좌표/사각형 계산 모듈 (표준 C++만 사용)

namespace led {

struct RectI {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool isEmpty() const { return w <= 0 || h <= 0; }
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    bool operator==(const RectI& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
    bool operator!=(const RectI& o) const { return !(*this == o); }
};

struct RectF {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    bool isEmpty() const { return w <= 0.0 || h <= 0.0; }
};

// 콘텐츠를 영역에 맞추는 방식
enum class FitMode {
    Stretch,   // 영역 크기에 맞게 늘리기 (비율 무시)
    Fit,       // 비율 유지, 영역 안에 전부 표시 (여백 생김)
    Fill,      // 비율 유지, 영역을 가득 채움 (잘림)
    Original   // 원본 픽셀 1:1, 영역 좌상단 기준 (넘치는 부분 잘림)
};

// dst: 캔버스 좌표계에서 그릴 위치, uv: 콘텐츠에서 가져올 범위(0~1)
struct Placement {
    RectF dst;
    RectF uv;
    bool visible = false;
};

Placement computePlacement(const RectI& region, int contentWidth, int contentHeight, FitMode mode);

bool intersects(const RectF& a, const RectF& b);

// dst를 clip 범위로 자르고 uv도 같은 비율로 줄임. 남는 부분이 없으면 false
bool clipPlacement(RectF& dst, RectF& uv, const RectF& clip);

} // namespace led
