#include "ui/operator/ProgramMonitor.h"

#include "ui/Palette.h"

#include <QImage>
#include <QPainter>

#include <algorithm>

namespace led {

namespace {
constexpr int kRefreshMs = 100; // 초당 10번 (미리보기 용도, 출력 창 동기화와 무관)
}

ProgramMonitor::ProgramMonitor(QWidget* parent)
    : QWidget(parent)
    , m_frameColor(palette::kBorderStrong)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumHeight(120);
    m_timer.setInterval(kRefreshMs);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_source)
            m_source->collect(m_layers);
        else
            m_layers.clear();
        update();
    });
}

void ProgramMonitor::setCanvasSize(int width, int height)
{
    m_canvasW = std::max(1, width);
    m_canvasH = std::max(1, height);
    updateGeometry();
    update();
}

void ProgramMonitor::setActive(bool active)
{
    if (active && !m_timer.isActive()) {
        m_timer.start();
    } else if (!active && m_timer.isActive()) {
        m_timer.stop();
        m_layers.clear(); // 프레임 메모리를 붙잡고 있지 않음
        update();
    }
}

void ProgramMonitor::setFrameColor(const QColor& color, int width)
{
    if (m_frameColor == color && m_frameWidth == width)
        return;
    m_frameColor = color;
    m_frameWidth = width;
    update();
}

int ProgramMonitor::heightForWidth(int w) const
{
    return std::clamp(static_cast<int>(w * double(m_canvasH) / m_canvasW), 80, 420);
}

QRectF ProgramMonitor::canvasArea() const
{
    const QRectF box = QRectF(rect()).adjusted(4, 4, -4, -4);
    const double scale = std::min(box.width() / m_canvasW, box.height() / m_canvasH);
    const double w = m_canvasW * scale;
    const double h = m_canvasH * scale;
    return QRectF(box.center().x() - w / 2.0, box.center().y() - h / 2.0, w, h);
}

void ProgramMonitor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), palette::kBackground);
    const QRectF area = canvasArea();
    p.fillRect(area, QColor(5, 6, 8));

    const double scale = area.width() / m_canvasW;
    p.save();
    p.setClipRect(area);
    for (const SceneLayer& layer : m_layers) {
        const Frame* f = layer.frame.get();
        if (!f || f->width <= 0 || f->height <= 0 || f->pixels.empty())
            continue;
        // BGRA 메모리 순서 = 리틀엔디언 ARGB32
        const QImage image(f->pixels.data(), f->width, f->height, f->stride,
                           f->premultiplied ? QImage::Format_ARGB32_Premultiplied : QImage::Format_ARGB32);
        const QRectF target(area.x() + layer.dst.x * scale, area.y() + layer.dst.y * scale, layer.dst.w * scale,
                            layer.dst.h * scale);
        const QRectF source(layer.uv.x * f->width, layer.uv.y * f->height, layer.uv.w * f->width,
                            layer.uv.h * f->height);
        p.setOpacity(std::clamp(double(layer.opacity), 0.0, 1.0));
        p.drawImage(target, image, source);
    }
    p.restore();

    QPen pen(m_frameColor);
    pen.setWidth(m_frameWidth);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const double half = m_frameWidth / 2.0;
    p.drawRect(area.adjusted(-half, -half, half, half));

    if (m_layers.empty()) {
        p.setPen(palette::kTextDim);
        p.drawText(area, Qt::AlignCenter, QStringLiteral("송출 중인 화면 없음"));
    }
}

} // namespace led
