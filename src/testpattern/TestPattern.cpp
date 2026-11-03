#include "testpattern/TestPattern.h"

#include <QFont>
#include <QPainter>

#include <algorithm>
#include <iterator>

namespace led {

namespace {

const QColor kRegionColors[] = {QColor(255, 80, 80),  QColor(80, 220, 120), QColor(80, 160, 255),
                                QColor(255, 200, 60), QColor(220, 90, 255), QColor(60, 220, 220)};

} // namespace

QImage makeTestPattern(const Project& project, const OutputConfig& output, int outputIndex)
{
    const RectI& c = output.canvasRect;
    QImage image(c.w, c.h, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return image;
    image.fill(Qt::black);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, false);

    // 격자: 캔버스 좌표 기준 64px 간격 (얇은 선), 256px 간격 (밝은 선)
    const int minor = 64;
    const int major = 256;
    auto firstLine = [](int start, int step) { return ((start + step - 1) / step) * step; };
    for (int gx = firstLine(std::max(c.x, 0), minor); gx < c.x + c.w; gx += minor) {
        p.setPen(QColor(gx % major == 0 ? 110 : 45, gx % major == 0 ? 110 : 45, gx % major == 0 ? 110 : 45));
        p.drawLine(gx - c.x, 0, gx - c.x, c.h - 1);
    }
    for (int gy = firstLine(std::max(c.y, 0), minor); gy < c.y + c.h; gy += minor) {
        p.setPen(QColor(gy % major == 0 ? 110 : 45, gy % major == 0 ? 110 : 45, gy % major == 0 ? 110 : 45));
        p.drawLine(0, gy - c.y, c.w - 1, gy - c.y);
    }

    // 영역 표시
    QFont font(QStringLiteral("Malgun Gothic"));
    font.setPixelSize(std::clamp(c.h / 30, 12, 48));
    p.setFont(font);
    int colorIndex = 0;
    for (const Region& r : project.regions) {
        const QColor color = kRegionColors[colorIndex++ % int(std::size(kRegionColors))];
        const QRect rect(r.rect.x - c.x, r.rect.y - c.y, r.rect.w, r.rect.h);
        if (!rect.intersects(image.rect()))
            continue;
        QColor fill = color;
        fill.setAlpha(50);
        p.fillRect(rect, fill);
        p.setPen(QPen(color, 2));
        p.drawRect(rect.adjusted(1, 1, -1, -1));
        p.drawText(rect.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("%1\n%2, %3  %4×%5")
                       .arg(QString::fromStdString(r.name))
                       .arg(r.rect.x)
                       .arg(r.rect.y)
                       .arg(r.rect.w)
                       .arg(r.rect.h));
    }

    // 출력 테두리(1px 흰색)와 대각선
    p.setPen(QPen(Qt::white, 1));
    p.drawRect(0, 0, c.w - 1, c.h - 1);
    p.setPen(QPen(QColor(255, 255, 255, 90), 1));
    p.drawLine(0, 0, c.w - 1, c.h - 1);
    p.drawLine(c.w - 1, 0, 0, c.h - 1);

    // 중앙 정보
    QFont big = font;
    big.setPixelSize(std::clamp(c.h / 12, 16, 160));
    big.setBold(true);
    p.setFont(big);
    p.setPen(Qt::white);
    const QString info = QStringLiteral("출력 %1: %2\n캔버스 %3, %4  /  %5 × %6")
                             .arg(outputIndex + 1)
                             .arg(QString::fromStdString(output.name))
                             .arg(c.x)
                             .arg(c.y)
                             .arg(c.w)
                             .arg(c.h);
    p.drawText(image.rect(), Qt::AlignCenter, info);
    p.end();
    return image;
}

} // namespace led
