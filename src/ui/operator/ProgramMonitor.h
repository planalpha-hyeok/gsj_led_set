#pragma once

// 운영 화면의 송출(PROGRAM) 미리보기
//  - 재생 엔진이 출력 창에 보내는 것과 같은 레이어를 받아 캔버스 전체를 작게 그림
//  - 켜져 있을 때만 초당 10번 갱신 (편집 화면에서는 부하 없음)

#include "render/Scene.h"

#include <QColor>
#include <QTimer>
#include <QWidget>

#include <vector>

namespace led {

class ProgramMonitor : public QWidget {
    Q_OBJECT

public:
    explicit ProgramMonitor(QWidget* parent = nullptr);

    void setSource(SceneProvider* source) { m_source = source; }
    void setCanvasSize(int width, int height);
    void setActive(bool active);           // 갱신 켜기/끄기
    void setFrameColor(const QColor& color, int width);

    QSize sizeHint() const override { return QSize(640, 200); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QRectF canvasArea() const;

    SceneProvider* m_source = nullptr;
    QTimer m_timer;
    std::vector<SceneLayer> m_layers;
    int m_canvasW = 1920;
    int m_canvasH = 1080;
    QColor m_frameColor;
    int m_frameWidth = 1;
};

} // namespace led
