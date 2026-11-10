#pragma once

// 캔버스 편집 화면
//  - 영역 이동: 영역 안쪽을 끌기
//  - 영역 크기: 테두리/모서리를 끌기
//  - 자석: 캔버스/출력/다른 영역 테두리에 붙음 (Alt를 누르고 있으면 일시 해제)
//  - 방향키: 1px 이동 (Shift: 10px), Ctrl+방향키: 크기 변경

#include "core/Geometry.h"
#include "core/Snap.h"

#include <QGraphicsView>
#include <QHash>
#include <QTimer>

#include <vector>

class QGraphicsLineItem;

namespace led {

class ProjectController;
class RegionItem;

class CanvasView : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(ProjectController* controller, QWidget* parent = nullptr);

    bool snapEnabled() const { return m_snapEnabled; }

    // RegionItem에서 호출
    void regionPressed(const QString& id);
    void regionGeometryFinished(const QString& id, const RectI& rect);
    // 자석 기준선: [0] 캔버스 테두리/가운데 (우선), [1] 출력과 다른 영역
    std::vector<SnapLines> snapTargets(const QString& excludeId) const;
    double snapThreshold() const;   // 화면 8px을 캔버스 픽셀로 환산
    void showGuides(bool showX, double x, bool showY, double y);
    void hideGuides();

public slots:
    void fitCanvas();
    void setSnapEnabled(bool enabled);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void scheduleRebuild();
    void rebuild();
    void applySelection(const QString& id);

private:
    ProjectController* m_controller;
    QGraphicsScene* m_scene;
    QHash<QString, RegionItem*> m_regionItems;
    QGraphicsLineItem* m_guideX = nullptr;
    QGraphicsLineItem* m_guideY = nullptr;
    QTimer m_rebuildTimer;
    bool m_fitPending = true;
    bool m_panning = false;
    bool m_snapEnabled = true;
    QPoint m_panStart;
};

} // namespace led
