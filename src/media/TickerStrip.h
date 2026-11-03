#pragma once

// 흐르는 자막 이미지(띠) 생성
// 긴 문구는 그래픽카드 텍스처 한계를 넘지 않도록 여러 조각(최대 4096px)으로 나눔

#include "core/Project.h"
#include "media/Frame.h"

#include <memory>
#include <string>
#include <vector>

namespace led {

struct TickerChunk {
    std::shared_ptr<const Frame> frame;
    int offset = 0;              // 띠 시작부터 이 조각까지의 거리 (흐르는 방향 축)
};

struct TickerStrip {
    std::vector<TickerChunk> chunks;
    int length = 0;              // 띠 길이 (문구 + 간격, 흐르는 방향 축)
    int thickness = 0;           // 띠 두께 (가로 흐름: 영역 높이, 세로 흐름: 영역 너비)
    bool vertical = false;
};

constexpr int kTickerChunkSize = 4096;

// 자막에 표시할 문구 목록 (textFile 지정 시 파일에서, 아니면 lines). 빈 줄 제외
std::vector<std::string> tickerLines(const TickerConfig& config, std::string* error = nullptr);

// 문구가 없으면 nullptr
std::shared_ptr<const TickerStrip> buildTickerStrip(const TickerConfig& config, const std::vector<std::string>& lines,
                                                    int regionWidth, int regionHeight);

// 단색 1x1 프레임 (자막 배경용)
std::shared_ptr<const Frame> solidColorFrame(std::uint32_t argb);

} // namespace led
