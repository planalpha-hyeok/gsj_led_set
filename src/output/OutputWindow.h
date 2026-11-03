#pragma once

// 출력 창 공통 인터페이스 (표준 C++만 사용)

#include "core/Geometry.h"
#include "media/Frame.h"
#include "render/Scene.h"

#include <memory>
#include <string>

namespace led {

struct OutputSpec {
    std::string name;
    int windowX = 0;       // 바탕화면 좌표 (물리 픽셀)
    int windowY = 0;
    RectI canvasRect;      // 내보낼 캔버스 범위. 창 크기 = canvasRect.w x canvasRect.h
    bool vsync = true;
    bool topmost = true;
};

class OutputWindow {
public:
    virtual ~OutputWindow() = default;

    // 창을 띄우고 렌더링 시작. 실패 시 error에 사유 기록
    virtual bool start(const OutputSpec& spec, SceneProvider* scene, std::string& error) = 0;
    virtual void stop() = 0;

    // 테스트 패턴 등 전체 화면 덮어쓰기 이미지. nullptr이면 해제
    virtual void setOverlay(std::shared_ptr<const Frame> overlay) = 0;

    // 렌더링에 사용 중인 그래픽카드 이름
    virtual std::string adapterName() const = 0;
};

std::unique_ptr<OutputWindow> createOutputWindow();

} // namespace led
