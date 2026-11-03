#pragma once

// 재생 엔진 -> 출력 창으로 전달되는 데이터 (표준 C++만 사용)

#include "core/Geometry.h"
#include "media/Frame.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace led {

// 캔버스 좌표계 기준 레이어 1개
struct SceneLayer {
    std::uint64_t key = 0;           // 영역마다 고정된 값 (텍스처 재사용용)
    RectF dst;                       // 캔버스 좌표
    RectF uv;                        // 콘텐츠 범위 0~1
    float opacity = 1.0f;
    std::shared_ptr<const Frame> frame;
};

// 출력 창 기준 그리기 항목 1개
struct DrawItem {
    std::uint64_t key = 0;
    RectF dst;                       // 출력 창 픽셀 좌표
    RectF uv;
    float opacity = 1.0f;
    std::shared_ptr<const Frame> frame;
};

class SceneProvider {
public:
    virtual ~SceneProvider() = default;
    // 여러 출력 스레드에서 동시에 호출됨. out은 뒤->앞 순서
    virtual void collect(std::vector<SceneLayer>& out) = 0;
};

// 캔버스 레이어 중 출력 범위에 걸치는 것만 출력 창 좌표로 변환
void buildDrawItems(const std::vector<SceneLayer>& layers, const RectI& outputCanvasRect,
                    std::vector<DrawItem>& out);

} // namespace led
