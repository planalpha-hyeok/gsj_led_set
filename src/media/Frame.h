#pragma once

// 디코딩된 한 장의 이미지 (BGRA 8비트, 메모리 순서 B,G,R,A)

#include <cstdint>
#include <vector>

namespace led {

struct Frame {
    int width = 0;
    int height = 0;
    int stride = 0;              // 한 줄의 바이트 수
    bool premultiplied = true;   // 알파가 미리 곱해졌는지 여부
    std::uint64_t serial = 0;    // 프레임마다 고유한 번호 (텍스처 갱신 판단용)
    std::vector<std::uint8_t> pixels;
};

std::uint64_t nextFrameSerial();

} // namespace led
