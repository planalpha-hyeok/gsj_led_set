#include "output/qt/QtOutputWindow.h"

#include "core/Log.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QScreen>

#include <vector>

namespace led {

namespace {

QImage wrapFrame(const Frame& frame)
{
    return QImage(frame.pixels.data(), frame.width, frame.height, frame.stride,
                  frame.premultiplied ? QImage::Format_ARGB32_Premultiplied : QImage::Format_ARGB32);
}

} // namespace

std::unique_ptr<OutputWindow> createOutputWindow()
{
    return std::make_unique<QtOutputWindow>();
}

QtOutputSurface::QtOutputSurface(const OutputSpec& spec, SceneProvider* scene)
    : m_spec(spec)
    , m_scene(scene)
{
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus;
    if (spec.topmost)
        flags |= Qt::WindowStaysOnTopHint;
    setFlags(flags);
    setTitle(QStringLiteral("LED 출력 - %1").arg(QString::fromStdString(spec.name)));

    const qreal dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    setGeometry(QRect(qRound(spec.windowX / dpr), qRound(spec.windowY / dpr), qRound(spec.canvasRect.w / dpr),
                      qRound(spec.canvasRect.h / dpr)));

    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this] { update(); });
    m_timer.start();
}

void QtOutputSurface::setOverlay(std::shared_ptr<const Frame> overlay)
{
    m_overlay = std::move(overlay);
    update();
}

void QtOutputSurface::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(QRect(QPoint(0, 0), size()), Qt::black);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // 물리 픽셀 좌표 -> 논리 좌표
    const qreal sx = qreal(width()) / m_spec.canvasRect.w;
    const qreal sy = qreal(height()) / m_spec.canvasRect.h;
    painter.scale(sx, sy);

    if (m_overlay) {
        painter.drawImage(QRectF(0, 0, m_spec.canvasRect.w, m_spec.canvasRect.h), wrapFrame(*m_overlay));
        return;
    }
    if (!m_scene)
        return;

    std::vector<SceneLayer> layers;
    std::vector<DrawItem> items;
    m_scene->collect(layers);
    buildDrawItems(layers, m_spec.canvasRect, items);
    for (const DrawItem& item : items) {
        const Frame& f = *item.frame;
        painter.setOpacity(item.opacity);
        const QRectF src(item.uv.x * f.width, item.uv.y * f.height, item.uv.w * f.width, item.uv.h * f.height);
        painter.drawImage(QRectF(item.dst.x, item.dst.y, item.dst.w, item.dst.h), wrapFrame(f), src);
    }
}

QtOutputWindow::~QtOutputWindow()
{
    stop();
}

bool QtOutputWindow::start(const OutputSpec& spec, SceneProvider* scene, std::string& error)
{
    stop();
    if (spec.canvasRect.isEmpty()) {
        error = "출력 크기가 0";
        return false;
    }
    m_surface = std::make_unique<QtOutputSurface>(spec, scene);
    m_surface->show();
    logInfo("출력 시작(개발용 창): " + spec.name);
    return true;
}

void QtOutputWindow::stop()
{
    m_surface.reset();
}

void QtOutputWindow::setOverlay(std::shared_ptr<const Frame> overlay)
{
    if (m_surface)
        m_surface->setOverlay(std::move(overlay));
}

} // namespace led
