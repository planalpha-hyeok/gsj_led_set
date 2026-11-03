#pragma once

// 속성 편집 화면 공통 위젯 생성 함수

#include <QSpinBox>

namespace led {

// 좌표/크기 입력용 정수 칸. Enter 또는 포커스 이동 시에만 값 반영
inline QSpinBox* makePixelSpinBox(int minimum, int maximum, QWidget* parent = nullptr)
{
    auto* box = new QSpinBox(parent);
    box->setRange(minimum, maximum);
    box->setKeyboardTracking(false);
    box->setAccelerated(true);
    box->setSuffix(QStringLiteral(" px"));
    box->setMinimumWidth(90);
    return box;
}

} // namespace led
