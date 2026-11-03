#pragma once

// 캔버스 편집 화면: 영역/출력 범위를 그림으로 보여주고, 영역을 마우스로 이동

#include <QGraphicsView>
#include <QHash>
#include <QTimer>

namespace led {

class ProjectController;
class RegionItem;

class CanvasView : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(ProjectController* controller, QWidget* parent = nullptr);

    void regionDragFinished(const QString& id, const QPoint& position);
    void regionPressed(const QString& id);

public slots:
    void fitCanvas();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void scheduleRebuild();
    void rebuild();
    void applySelection(const QString& id);

private:
    ProjectController* m_controller;
    QGraphicsScene* m_scene;
    QHash<QString, RegionItem*> m_regionItems;
    QTimer m_rebuildTimer;
    bool m_fitPending = true;
    bool m_panning = false;
    QPoint m_panStart;
};

} // namespace led
