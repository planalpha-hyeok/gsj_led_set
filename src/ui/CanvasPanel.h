#pragma once

// 전체 캔버스 크기 설정

#include <QWidget>

class QLabel;
class QSpinBox;

namespace led {

class ProjectController;

class CanvasPanel : public QWidget {
    Q_OBJECT

public:
    explicit CanvasPanel(ProjectController* controller, QWidget* parent = nullptr);

private slots:
    void load();
    void commit();
    void fitToOutputs();
    void fitToRegions();

private:
    ProjectController* m_controller;
    QSpinBox* m_width;
    QSpinBox* m_height;
    QLabel* m_summary;
    bool m_loading = false;
};

} // namespace led
