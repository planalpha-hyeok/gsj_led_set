#pragma once

// LED 매핑 확인용 테스트 패턴 생성
// 출력 1개 크기의 이미지에 격자, 테두리, 대각선, 영역 위치, 좌표를 그림

#include "core/Project.h"

#include <QImage>

namespace led {

QImage makeTestPattern(const Project& project, const OutputConfig& output, int outputIndex);

} // namespace led
