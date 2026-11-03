#pragma once

// 색상 선택 버튼 (투명도 포함). 누르면 색상 대화상자 표시

#include <QPushButton>

#include <cstdint>

namespace led {

class ColorButton : public QPushButton {
    Q_OBJECT

public:
    explicit ColorButton(QWidget* parent = nullptr);

    std::uint32_t color() const { return m_color; }
    void setColor(std::uint32_t argb); // 신호 없음

signals:
    void colorChanged(std::uint32_t argb);

private:
    void pick();
    void refresh();

    std::uint32_t m_color = 0xFFFFFFFFu;
};

} // namespace led
