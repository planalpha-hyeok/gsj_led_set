#include "ui/CanvasView.h"

#include "core/Snap.h"
#include "ui/Palette.h"
#include "display/DisplayInfo.h"
#include "ui/ProjectController.h"

#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSettings>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace led {

namespace {

const QColor kRegionColors[] = {QColor(76, 141, 255), QColor(245, 165, 36), QColor(160, 120, 230),
                                QColor(79, 195, 138), QColor(63, 208, 201), QColor(255, 107, 99)};
const QColor kOutputColor(255, 255, 255);
const QColor kGuideColor(255, 64, 200);
constexpr double kHandleScreenPx = 7.0;  // 테두리 잡기 범위 (화면 픽셀)
constexpr double kSnapScreenPx = 8.0;    // 자석 거리 (화면 픽셀)

QGraphicsSimpleTextItem* makeLabel(QGraphicsItem* parent, const QString& text, const QColor& color)
{
    auto* label = new QGraphicsSimpleTextItem(text, parent);
    label->setBrush(color);
    label->setFlag(QGraphicsItem::ItemIgnoresTransformations, true); // 확대/축소와 무관하게 같은 글자 크기
    label->setAcceptedMouseButtons(Qt::NoButton);
    return label;
}

Qt::CursorShape cursorFor(int edges, bool move)
{
    if (move)
        return Qt::SizeAllCursor;
    const bool l = edges & SnapLeft, r = edges & SnapRight, t = edges & SnapTop, b = edges & SnapBottom;
    if ((l && t) || (r && b))
        return Qt::SizeFDiagCursor;
    if ((r && t) || (l && b))
        return Qt::SizeBDiagCursor;
    if (l || r)
        return Qt::SizeHorCursor;
    return Qt::SizeVerCursor;
}

} // namespace

// 영역 1개를 나타내는 사각형. 안쪽을 끌면 이동, 테두리/모서리를 끌면 크기 변경
class RegionItem : public QGraphicsRectItem {
public:
    RegionItem(CanvasView* view, const QString& id, const QColor& color)
        : m_view(view)
        , m_id(id)
        , m_color(color)
    {
        setFlags(ItemIsSelectable);
        setAcceptHoverEvents(true);
        setCursor(Qt::SizeAllCursor);
    }

    void setLabel(QGraphicsSimpleTextItem* label) { m_label = label; }
    void setName(const QString& name) { m_name = name; }

    RectI geometry() const
    {
        return RectI{qRound(pos().x()), qRound(pos().y()), qRound(rect().width()), qRound(rect().height())};
    }

    void setGeometry(const RectI& r)
    {
        prepareGeometryChange();
        setPos(r.x, r.y);
        setRect(0, 0, r.w, r.h);
        updateLabel();
    }

    void updateLabel()
    {
        if (!m_label)
            return;
        const RectI g = geometry();
        m_label->setText(QStringLiteral("%1\n%2, %3  %4×%5").arg(m_name).arg(g.x).arg(g.y).arg(g.w).arg(g.h));
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        QColor fill = m_color;
        fill.setAlpha(isSelected() ? 90 : 55);
        painter->fillRect(rect(), fill);
        QPen pen(isSelected() ? QColor(255, 255, 255) : m_color);
        pen.setCosmetic(true);
        pen.setWidth(isSelected() ? 2 : 1);
        painter->setPen(pen);
        painter->drawRect(rect());

        // 선택된 영역: 8개 조절점 (화면 기준 크기)
        if (isSelected()) {
            // 화면 기준 4px (확대/축소와 무관)
            const double hs = 4.0 / std::max(1e-6, painter->worldTransform().m11());
            const QRectF r = rect();
            const QPointF points[] = {r.topLeft(),    QPointF(r.center().x(), r.top()),    r.topRight(),
                                      QPointF(r.right(), r.center().y()), r.bottomRight(),
                                      QPointF(r.center().x(), r.bottom()), r.bottomLeft(),
                                      QPointF(r.left(), r.center().y())};
            painter->setBrush(QColor(255, 255, 255));
            QPen hp(QColor(40, 40, 40));
            hp.setCosmetic(true);
            painter->setPen(hp);
            for (const QPointF& p : points)
                painter->drawRect(QRectF(p.x() - hs, p.y() - hs, hs * 2, hs * 2));
        }
    }

protected:
    // 마우스 위치가 어느 테두리 근처인지 (0 = 안쪽: 이동)
    int hitEdges(const QPointF& local) const
    {
        const double viewScale = std::max(1e-6, m_view->transform().m11());
        const double t = std::min(kHandleScreenPx / viewScale, std::min(rect().width(), rect().height()) / 3.0);
        int edges = 0;
        if (local.x() <= t)
            edges |= SnapLeft;
        else if (local.x() >= rect().width() - t)
            edges |= SnapRight;
        if (local.y() <= t)
            edges |= SnapTop;
        else if (local.y() >= rect().height() - t)
            edges |= SnapBottom;
        return edges;
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override
    {
        const int edges = hitEdges(event->pos());
        setCursor(cursorFor(edges, edges == 0));
        QGraphicsRectItem::hoverMoveEvent(event);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            event->ignore();
            return;
        }
        m_edges = hitEdges(event->pos());
        m_dragging = true;
        m_pressScene = event->scenePos();
        m_start = geometry();
        m_view->regionPressed(m_id);
        setSelected(true);
        event->accept();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (!m_dragging)
            return;
        const QPointF d = event->scenePos() - m_pressScene;
        const bool move = m_edges == 0;

        RectF r{double(m_start.x), double(m_start.y), double(m_start.w), double(m_start.h)};
        if (move) {
            r.x += d.x();
            r.y += d.y();
        } else {
            if (m_edges & SnapLeft) {
                r.x += d.x();
                r.w -= d.x();
            }
            if (m_edges & SnapRight)
                r.w += d.x();
            if (m_edges & SnapTop) {
                r.y += d.y();
                r.h -= d.y();
            }
            if (m_edges & SnapBottom)
                r.h += d.y();
        }

        // 자석: Alt를 누르고 있으면 일시 해제
        const bool snap = m_view->snapEnabled() && !(event->modifiers() & Qt::AltModifier);
        std::vector<SnapLines> lines;
        if (snap)
            lines = m_view->snapTargets(m_id);
        const SnapResult result = snapRect(r, move, m_edges, lines, snap ? m_view->snapThreshold() : 0.0);
        setGeometry(result.rect);
        m_view->showGuides(result.snappedX, result.guideX, result.snappedY, result.guideY);
        event->accept();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        if (!m_dragging)
            return;
        m_dragging = false;
        m_view->hideGuides();
        const RectI g = geometry();
        if (g != m_start)
            m_view->regionGeometryFinished(m_id, g);
        event->accept();
    }

private:
    CanvasView* m_view;
    QString m_id;
    QString m_name;
    QColor m_color;
    QGraphicsSimpleTextItem* m_label = nullptr;
    bool m_dragging = false;
    int m_edges = 0;
    QPointF m_pressScene;
    RectI m_start;
};

CanvasView::CanvasView(ProjectController* controller, QWidget* parent)
    : QGraphicsView(parent)
    , m_controller(controller)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::TextAntialiasing, true);
    setBackgroundBrush(palette::kBackground);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus); // 방향키 조작

    m_snapEnabled = QSettings().value(QStringLiteral("snapEnabled"), true).toBool();

    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(0);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &CanvasView::rebuild);

    connect(controller, &ProjectController::projectReset, this, [this] {
        m_fitPending = true;
        scheduleRebuild();
    });
    connect(controller, &ProjectController::regionsChanged, this, &CanvasView::scheduleRebuild);
    connect(controller, &ProjectController::outputsChanged, this, &CanvasView::scheduleRebuild);
    connect(controller, &ProjectController::canvasChanged, this, &CanvasView::scheduleRebuild);
    connect(controller, &ProjectController::regionSelected, this, &CanvasView::applySelection);

    scheduleRebuild();
}

void CanvasView::setSnapEnabled(bool enabled)
{
    m_snapEnabled = enabled;
    QSettings().setValue(QStringLiteral("snapEnabled"), enabled);
}

void CanvasView::scheduleRebuild()
{
    // 마우스 이벤트 처리 중 항목이 삭제되지 않도록 이벤트 루프로 미룸
    m_rebuildTimer.start();
}

void CanvasView::rebuild()
{
    const Project& project = m_controller->project();
    m_guideX = nullptr;
    m_guideY = nullptr;
    m_scene->clear();
    m_regionItems.clear();

    const QRectF canvasRect(0, 0, project.canvas.width, project.canvas.height);
    auto* canvas = m_scene->addRect(canvasRect, QPen(Qt::NoPen), QBrush(QColor(5, 6, 8)));
    canvas->setZValue(-100);
    QPen canvasPen(QColor(58, 64, 76));
    canvasPen.setCosmetic(true);
    auto* canvasBorder = m_scene->addRect(canvasRect, canvasPen, Qt::NoBrush);
    canvasBorder->setZValue(-99);
    canvasBorder->setAcceptedMouseButtons(Qt::NoButton);
    auto* canvasLabel = makeLabel(canvasBorder,
                                  QStringLiteral("캔버스 %1 × %2").arg(project.canvas.width).arg(project.canvas.height),
                                  QColor(150, 150, 150));
    canvasLabel->setPos(0, project.canvas.height);

    QRectF bounds = canvasRect;

    int colorIndex = 0;
    for (const Region& r : project.regions) {
        const QString id = QString::fromStdString(r.id);
        const QColor color = kRegionColors[colorIndex++ % int(std::size(kRegionColors))];
        auto* item = new RegionItem(this, id, color);
        item->setZValue(colorIndex);
        item->setOpacity(r.visible ? 1.0 : 0.35);
        auto* label = makeLabel(item, QString(), QColor(240, 240, 240));
        label->setPos(0, 0);
        label->setTransform(QTransform::fromTranslate(4, 3)); // 테두리와 겹치지 않게 화면 기준 4px 띄움
        item->setLabel(label);
        item->setName(QString::fromStdString(r.name));
        item->setGeometry(r.rect);
        m_scene->addItem(item);
        m_regionItems.insert(id, item);
        bounds = bounds.united(QRectF(r.rect.x, r.rect.y, r.rect.w, r.rect.h));
    }

    // 출력 범위는 점선으로 맨 위에 표시 (마우스 입력은 받지 않음)
    const std::vector<DisplayInfo> displays = enumerateDisplays();
    int outputIndex = 0;
    for (const OutputConfig& o : project.outputs) {
        ++outputIndex;
        const QRectF rect(o.canvasRect.x, o.canvasRect.y, o.canvasRect.w, o.canvasRect.h);
        QPen pen(o.enabled ? kOutputColor : QColor(120, 120, 120));
        pen.setCosmetic(true);
        pen.setStyle(Qt::DashLine);
        pen.setWidth(2);
        auto* item = m_scene->addRect(rect, pen, Qt::NoBrush);
        item->setZValue(1000);
        item->setAcceptedMouseButtons(Qt::NoButton);

        const DisplayInfo* d = findDisplay(displays, o.displayId);
        QString text = QStringLiteral("출력 %1: %2  [%3]")
                           .arg(outputIndex)
                           .arg(QString::fromStdString(o.name))
                           .arg(d ? QString::fromStdString(d->id) : QStringLiteral("디스플레이 없음"));
        if (!o.enabled)
            text += QStringLiteral(" (사용 안 함)");
        auto* label = makeLabel(item, text, pen.color());
        label->setPos(rect.topLeft());
        label->setZValue(1001);
        label->setTransform(QTransform::fromTranslate(0, -label->boundingRect().height() - 2));
        bounds = bounds.united(rect);
    }

    // 자석 안내선 (드래그 중에만 표시)
    QPen guidePen(kGuideColor);
    guidePen.setCosmetic(true);
    guidePen.setStyle(Qt::DashLine);
    m_guideX = m_scene->addLine(QLineF(), guidePen);
    m_guideY = m_scene->addLine(QLineF(), guidePen);
    for (QGraphicsLineItem* g : {m_guideX, m_guideY}) {
        g->setZValue(2000);
        g->setAcceptedMouseButtons(Qt::NoButton);
        g->setVisible(false);
    }

    const double margin = std::max(bounds.width(), bounds.height()) * 0.05 + 50.0;
    m_scene->setSceneRect(bounds.adjusted(-margin, -margin, margin, margin));

    applySelection(m_controller->selectedRegionId());

    if (m_fitPending) {
        m_fitPending = false;
        fitCanvas();
    }
}

void CanvasView::applySelection(const QString& id)
{
    const QSignalBlocker blocker(m_scene);
    for (auto it = m_regionItems.begin(); it != m_regionItems.end(); ++it)
        it.value()->setSelected(it.key() == id);
}

void CanvasView::regionPressed(const QString& id)
{
    m_controller->selectRegion(id);
    applySelection(id);
}

void CanvasView::regionGeometryFinished(const QString& id, const RectI& rect)
{
    const Region* r = m_controller->project().findRegion(id.toStdString());
    if (!r)
        return;
    Region updated = *r;
    updated.rect = rect;
    m_controller->updateRegion(updated);
}

std::vector<SnapLines> CanvasView::snapTargets(const QString& excludeId) const
{
    const Project& p = m_controller->project();
    std::vector<SnapLines> groups(2);
    // 테두리와 가운데선
    auto add = [](SnapLines& g, const RectI& r) {
        g.xs.push_back(r.x);
        g.xs.push_back(r.x + r.w / 2.0);
        g.xs.push_back(r.right());
        g.ys.push_back(r.y);
        g.ys.push_back(r.y + r.h / 2.0);
        g.ys.push_back(r.bottom());
    };
    add(groups[0], RectI{0, 0, p.canvas.width, p.canvas.height}); // 캔버스 우선
    for (const OutputConfig& o : p.outputs)
        add(groups[1], o.canvasRect);
    for (const Region& r : p.regions)
        if (QString::fromStdString(r.id) != excludeId)
            add(groups[1], r.rect);
    return groups;
}

double CanvasView::snapThreshold() const
{
    return kSnapScreenPx / std::max(1e-6, transform().m11());
}

void CanvasView::showGuides(bool showX, double x, bool showY, double y)
{
    const QRectF area = m_scene->sceneRect();
    if (m_guideX) {
        m_guideX->setVisible(showX);
        if (showX)
            m_guideX->setLine(x, area.top(), x, area.bottom());
    }
    if (m_guideY) {
        m_guideY->setVisible(showY);
        if (showY)
            m_guideY->setLine(area.left(), y, area.right(), y);
    }
}

void CanvasView::hideGuides()
{
    showGuides(false, 0.0, false, 0.0);
}

void CanvasView::fitCanvas()
{
    const Project& project = m_controller->project();
    QRectF rect(0, 0, project.canvas.width, project.canvas.height);
    for (const OutputConfig& o : project.outputs)
        rect = rect.united(QRectF(o.canvasRect.x, o.canvasRect.y, o.canvasRect.w, o.canvasRect.h));
    const double margin = std::max(rect.width(), rect.height()) * 0.03;
    fitInView(rect.adjusted(-margin, -margin, margin, margin), Qt::KeepAspectRatio);
}

void CanvasView::keyPressEvent(QKeyEvent* event)
{
    const Region* r = m_controller->project().findRegion(m_controller->selectedRegionId().toStdString());
    int dx = 0;
    int dy = 0;
    switch (event->key()) {
    case Qt::Key_Left: dx = -1; break;
    case Qt::Key_Right: dx = 1; break;
    case Qt::Key_Up: dy = -1; break;
    case Qt::Key_Down: dy = 1; break;
    default:
        QGraphicsView::keyPressEvent(event);
        return;
    }
    if (!r) {
        event->ignore();
        return;
    }
    const int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    Region updated = *r;
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+방향키: 오른쪽/아래 테두리로 크기 변경
        updated.rect.w = std::max(1, updated.rect.w + dx * step);
        updated.rect.h = std::max(1, updated.rect.h + dy * step);
    } else {
        updated.rect.x += dx * step;
        updated.rect.y += dy * step;
    }
    m_controller->updateRegion(updated);
    event->accept();
}

void CanvasView::wheelEvent(QWheelEvent* event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const double next = transform().m11() * factor;
    if (next < 0.002 || next > 40.0)
        return;
    scale(factor, factor);
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panStart = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
    // 빈 곳 클릭 시 선택을 유지 (속성 창이 비지 않도록)
    if (event->button() == Qt::LeftButton && !itemAt(event->pos()))
        applySelection(m_controller->selectedRegionId());
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const QPoint delta = event->pos() - m_panStart;
        m_panStart = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        viewport()->unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

} // namespace led
