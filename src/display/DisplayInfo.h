#pragma once

// 연결된 디스플레이(그래픽카드 출력) 정보 조회

#include <string>
#include <vector>

namespace led {

struct DisplayInfo {
    std::string id;          // Windows: "\\.\DISPLAY1"
    std::string name;        // 모니터/장치 이름
    std::string adapter;     // 그래픽카드 이름
    int x = 0;               // 바탕화면 좌표 (물리 픽셀)
    int y = 0;
    int width = 0;
    int height = 0;
    int refreshHz = 0;
    bool primary = false;
};

// 바탕화면 좌표 순서(왼쪽->오른쪽, 위->아래)로 정렬해 반환
std::vector<DisplayInfo> enumerateDisplays();

const DisplayInfo* findDisplay(const std::vector<DisplayInfo>& displays, const std::string& id);

} // namespace led
