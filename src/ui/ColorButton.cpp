#include "ui/ColorButton.h"

#include <QColorDialog>
#include <QPainter>
#include <QPixmap>

namespace led {

ColorButton::ColorButton(QWidget* parent)
    : QPushButton(parent)
{
    setMinimumWidth(110);
    connect(this, &QPushButton::clicked, this, &ColorButton::pick);
    refresh();
}

void ColorButton::setColor(std::uint32_t argb)
{
    m_color = argb;
    refresh();
}

void ColorButton::pick()
{
    const QColor chosen = QColorDialog::getColor(QColor::fromRgba(m_color), this, QStringLiteral("색 선택"),
                                                 QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid())
        return;
    const std::uint32_t value = chosen.rgba();
    if (value == m_color)
        return;
    setColor(value);
    emit colorChanged(value);
}

void ColorButton::refresh()
{
    // 투명도가 보이도록 체크무늬 위에 색을 칠한 견본
    QPixmap swatch(28, 16);
    QPainter p(&swatch);
    for (int y = 0; y < 16; y += 4)
        for (int x = 0; x < 28; x += 4)
            p.fillRect(x, y, 4, 4, ((x + y) / 4) % 2 ? QColor(90, 90, 90) : QColor(150, 150, 150));
    p.fillRect(swatch.rect(), QColor::fromRgba(m_color));
    p.setPen(QColor(200, 200, 200));
    p.drawRect(swatch.rect().adjusted(0, 0, -1, -1));
    p.end();
    setIcon(QIcon(swatch));
    setIconSize(swatch.size());

    const QColor c = QColor::fromRgba(m_color);
    setText(c.alpha() == 0 ? QStringLiteral("투명")
                           : QStringLiteral("%1 (%2%)").arg(c.name().toUpper()).arg(qRound(c.alphaF() * 100)));
}

} // namespace led
