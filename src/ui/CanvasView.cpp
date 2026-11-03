#include "ui/CanvasView.h"

#include "display/DisplayInfo.h"
#include "ui/ProjectController.h"

#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace led {

namespace {

const QColor kRegionColors[] = {QColor(235, 87, 87),  QColor(76, 194, 120), QColor(74, 144, 226),
                                QColor(242, 184, 56), QColor(186, 104, 230), QColor(64, 200, 200)};
const QColor kOutputColor(255, 255, 255);

QGraphicsSimpleTextItem* makeLabel(QGraphicsItem* parent, const QString& text, const QColor& color)
{
    auto* label = new QGraphicsSimpleTextItem(text, parent);
    label->setBrush(color);
    label->setFlag(QGraphicsItem::ItemIgnoresTransformations, true); // 확대/축소와 무관하게 같은 글자 크기
    label->setAcceptedMouseButtons(Qt::NoButton);
    return label;
}

} // namespace

// 영역 1개를 나타내는 사각형. 드래그로 이동 (정수 픽셀 단위로 맞춤)
class RegionItem : public QGraphicsRectItem {
public:
    RegionItem(CanvasView* view, const QString& id, const QColor& color)
        : m_view(view)
        , m_id(id)
        , m_color(color)
    {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        setCursor(Qt::SizeAllCursor);
    }

    void setLabel(QGraphicsSimpleTextItem* label) { m_label = label; }

    void updateLabel(const QString& name)
    {
        if (!m_label)
            return;
        const QPoint p = pos().toPoint();
        m_label->setText(QStringLiteral("%1\n%2, %3  %4×%5")
                             .arg(name)
                             .arg(p.x())
                             .arg(p.y())
                             .arg(qRound(rect().width()))
                             .arg(qRound(rect().height())));
    }

    void setName(const QString& name) { m_name = name; }

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
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override
    {
        if (change == ItemPositionChange) {
            const QPointF p = value.toPointF();
            return QPointF(std::round(p.x()), std::round(p.y()));
        }
        if (change == ItemPositionHasChanged)
            updateLabel(m_name);
        return QGraphicsRectItem::itemChange(change, value);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        m_pressPos = pos();
        QGraphicsRectItem::mousePressEvent(event);
        if (event->button() == Qt::LeftButton)
            m_view->regionPressed(m_id);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        QGraphicsRectItem::mouseReleaseEvent(event);
        if (pos() != m_pressPos)
            m_view->regionDragFinished(m_id, pos().toPoint());
    }

private:
    CanvasView* m_view;
    QString m_id;
    QString m_name;
    QColor m_color;
    QPointF m_pressPos;
    QGraphicsSimpleTextItem* m_label = nullptr;
};

CanvasView::CanvasView(ProjectController* controller, QWidget* parent)
    : QGraphicsView(parent)
    , m_controller(controller)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::TextAntialiasing, true);
    setBackgroundBrush(QColor(24, 25, 28));
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setFrameShape(QFrame::NoFrame);

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

void CanvasView::scheduleRebuild()
{
    // 마우스 이벤트 처리 중 항목이 삭제되지 않도록 이벤트 루프로 미룸
    m_rebuildTimer.start();
}

void CanvasView::rebuild()
{
    const Project& project = m_controller->project();
    m_scene->clear();
    m_regionItems.clear();

    const QRectF canvasRect(0, 0, project.canvas.width, project.canvas.height);
    auto* canvas = m_scene->addRect(canvasRect, QPen(Qt::NoPen), QBrush(QColor(8, 8, 8)));
    canvas->setZValue(-100);
    QPen canvasPen(QColor(110, 110, 110));
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
        item->setRect(0, 0, r.rect.w, r.rect.h);
        item->setPos(r.rect.x, r.rect.y);
        item->setZValue(colorIndex);
        item->setOpacity(r.visible ? 1.0 : 0.35);
        auto* label = makeLabel(item, QString(), QColor(240, 240, 240));
        label->setPos(0, 0);
        label->setTransform(QTransform::fromTranslate(4, 3)); // 테두리와 겹치지 않게 화면 기준 4px 띄움
        item->setLabel(label);
        item->setName(QString::fromStdString(r.name));
        item->updateLabel(QString::fromStdString(r.name));
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
        // 라벨을 출력 범위 위쪽 바깥에 표시
        label->setTransform(QTransform::fromTranslate(0, -label->boundingRect().height() - 2));
        bounds = bounds.united(rect);
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
}

void CanvasView::regionDragFinished(const QString& id, const QPoint& position)
{
    const Region* r = m_controller->project().findRegion(id.toStdString());
    if (!r)
        return;
    Region updated = *r;
    updated.rect.x = position.x();
    updated.rect.y = position.y();
    m_controller->updateRegion(updated);
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

void CanvasView::wheelEvent(QWheelEvent* event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    const double current = transform().m11();
    const double next = current * factor;
    if (next < 0.002 || next > 40.0)
        return;
    scale(factor, factor);
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panStart = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
    // 빈 곳 클릭 시 선택 해제하지 않고 유지 (속성 창이 비지 않도록)
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
