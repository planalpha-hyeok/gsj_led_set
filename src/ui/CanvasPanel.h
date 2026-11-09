#pragma once

// 전체 캔버스 크기 설정

#include <QWidget>

class QCheckBox;
class QComboBox;
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
    void refreshAudioDevices();   // 장치 목록 다시 조회
    void selectAudioDevice();     // 목록에서 프로젝트 설정 선택 (조회 없음)

private:
    ProjectController* m_controller;
    QSpinBox* m_width;
    QSpinBox* m_height;
    QLabel* m_summary;
    QCheckBox* m_hardwareDecoding;
    QComboBox* m_audioDevice;
    bool m_loading = false;
};

} // namespace led
