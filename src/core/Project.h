#pragma once

// 프로젝트 데이터 모델 (캔버스 / 영역 / 출력)
// 문자열은 모두 UTF-8 std::string

#include "core/Geometry.h"

#include <string>
#include <vector>

namespace led {

struct CanvasConfig {
    int width = 1920;
    int height = 1080;
};

// 캔버스 위의 재생 영역. LED 매핑 좌표와 동일한 픽셀 좌표 사용
struct Region {
    std::string id;
    std::string name;
    RectI rect;              // 캔버스 좌표
    std::string source;      // 콘텐츠 파일 경로 (비어 있으면 표시 안 함)
    FitMode fit = FitMode::Stretch;
    bool loop = true;
    double opacity = 1.0;    // 0.0 ~ 1.0
    bool visible = true;
};

// 그래픽카드 출력 1개 = 출력 창 1개
struct OutputConfig {
    std::string id;
    std::string name;
    std::string displayId;   // Windows: "\\.\DISPLAY2" 형식
    RectI canvasRect;        // 이 출력이 내보낼 캔버스 범위 (출력 창 크기 = w x h, 1:1 픽셀)
    int offsetX = 0;         // 디스플레이 좌상단 기준 출력 창 위치
    int offsetY = 0;
    bool enabled = true;
};

struct Project {
    CanvasConfig canvas;
    std::vector<Region> regions;      // 배열 순서 = 그리는 순서 (뒤쪽 요소가 앞에 보임)
    std::vector<OutputConfig> outputs;

    Region* findRegion(const std::string& id);
    const Region* findRegion(const std::string& id) const;
    OutputConfig* findOutput(const std::string& id);
    const OutputConfig* findOutput(const std::string& id) const;
};

std::string generateId();

const char* fitModeKey(FitMode mode);
bool fitModeFromKey(const std::string& key, FitMode& out);

} // namespace led
