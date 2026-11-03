#include "ui/Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <functional>

namespace led::icons {

namespace {

const QColor kLine(214, 217, 223);
const QColor kDisabled(105, 108, 115);

// 24x24 좌표계에 그리는 함수로 여러 크기의 아이콘 생성
QIcon make(const std::function<void(QPainter&, const QColor&)>& draw)
{
    QIcon icon;
    for (int size : {16, 24, 32, 48}) {
        for (bool enabled : {true, false}) {
            QPixmap pm(size, size);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.scale(size / 24.0, size / 24.0);
            draw(p, enabled ? kLine : kDisabled);
            p.end();
            icon.addPixmap(pm, enabled ? QIcon::Normal : QIcon::Disabled);
        }
    }
    return icon;
}

QPen pen(const QColor& c, double width = 1.8)
{
    QPen p(c, width);
    p.setCapStyle(Qt::RoundCap);
    p.setJoinStyle(Qt::RoundJoin);
    return p;
}

} // namespace

QIcon newProject()
{
    return make([](QPainter& p, const QColor& c) {
        p.setPen(pen(c));
        QPainterPath path;
        path.moveTo(6, 3);
        path.lineTo(14, 3);
        path.lineTo(19, 8);
        path.lineTo(19, 21);
        path.lineTo(6, 21);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(14, 3), QPointF(14, 8));
        p.drawLine(QPointF(14, 8), QPointF(19, 8));
    });
}

QIcon open()
{
    return make([](QPainter& p, const QColor& c) {
        p.setPen(pen(c));
        QPainterPath path;
        path.moveTo(3, 6);
        path.lineTo(9, 6);
        path.lineTo(11, 8);
        path.lineTo(20, 8);
        path.lineTo(20, 19);
        path.lineTo(3, 19);
        path.closeSubpath();
        p.drawPath(path);
    });
}

QIcon save()
{
    return make([](QPainter& p, const QColor& c) {
        p.setPen(pen(c));
        QPainterPath path;
        path.moveTo(4, 4);
        path.lineTo(17, 4);
        path.lineTo(20, 7);
        path.lineTo(20, 20);
        path.lineTo(4, 20);
        path.closeSubpath();
        p.drawPath(path);
        p.drawRect(QRectF(8, 4, 7, 5));
        p.drawRect(QRectF(7.5, 13, 9, 7));
    });
}

QIcon play()
{
    return make([](QPainter& p, const QColor& c) {
        const QColor fill = c == kDisabled ? kDisabled : QColor(76, 194, 122);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        QPainterPath path;
        path.moveTo(7, 4.5);
        path.lineTo(19.5, 12);
        path.lineTo(7, 19.5);
        path.closeSubpath();
        p.drawPath(path);
    });
}

QIcon stop()
{
    return make([](QPainter& p, const QColor& c) {
        const QColor fill = c == kDisabled ? kDisabled : QColor(235, 95, 95);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(QRectF(6, 6, 12, 12), 2, 2);
    });
}

QIcon output()
{
    return make([](QPainter& p, const QColor& c) {
        p.setPen(pen(c));
        p.drawRoundedRect(QRectF(3, 4, 18, 12), 1.5, 1.5);
        p.drawLine(QPointF(12, 16), QPointF(12, 20));
        p.drawLine(QPointF(8, 20), QPointF(16, 20));
    });
}

QIcon testPattern()
{
    return make([](QPainter& p, const QColor& c) {
        p.setPen(pen(c, 1.5));
        p.drawRect(QRectF(3, 4, 18, 16));
        p.drawLine(QPointF(9, 4), QPointF(9, 20));
        p.drawLine(QPointF(15, 4), QPointF(15, 20));
        p.drawLine(QPointF(3, 9.33), QPointF(21, 9.33));
        p.drawLine(QPointF(3, 14.66), QPointF(21, 14.66));
    });
}

QIcon fit()
{
    return make([](QPainter& p, const QColor& c) {
        p.setPen(pen(c));
        // 네 모서리 꺾쇠
        p.drawPolyline(QPolygonF({QPointF(4, 9), QPointF(4, 4), QPointF(9, 4)}));
        p.drawPolyline(QPolygonF({QPointF(15, 4), QPointF(20, 4), QPointF(20, 9)}));
        p.drawPolyline(QPolygonF({QPointF(20, 15), QPointF(20, 20), QPointF(15, 20)}));
        p.drawPolyline(QPolygonF({QPointF(9, 20), QPointF(4, 20), QPointF(4, 15)}));
    });
}

QIcon schedule()
{
    return make([](QPainter& p, const QColor& c) {
        const QColor accent = c == kDisabled ? kDisabled : QColor(242, 184, 56);
        p.setPen(pen(accent));
        p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
        p.drawLine(QPointF(12, 12), QPointF(12, 7));
        p.drawLine(QPointF(12, 12), QPointF(15.5, 14));
    });
}

} // namespace led::icons
