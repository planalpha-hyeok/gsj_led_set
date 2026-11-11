#pragma once

// 운영 모드 화면 (시안 C: 현장 송출용)
//  위: ON AIR 표시, 현재 장면, 재생 시각, 출력/스케줄 상태, 편집 화면 / 비상 정지
//  가운데: 송출 미리보기, 다음 큐, 재생 제어(처음/일시정지/GO), 진행 막대, 장면 버튼
//  오른쪽: 큐 목록, 영역 빠른 제어
// 이 화면은 상태를 표시하고 요청 신호만 보냄 (실제 처리는 MainWindow)

#include "playback/TimelinePlayer.h"

#include <QString>
#include <QWidget>

#include <vector>

class QLabel;
class QListWidget;
class QPushButton;

namespace led {

class CueProgressBar;
class OperatorRegionControls;
class OperatorSceneGrid;
class ProgramMonitor;
class ProjectController;
class SceneProvider;

struct OperatorState {
    bool playing = false;
    QString sceneId;              // 재생 중인 장면
    TimelineStatus timeline;
    int outputsRunning = 0;
    int outputsTotal = 0;
    bool testPattern = false;
    bool scheduleActive = false;
    QString scheduleText;
};

class OperatorPanel : public QWidget {
    Q_OBJECT

public:
    OperatorPanel(ProjectController* controller, SceneProvider* program, QWidget* parent = nullptr);

    void setState(const OperatorState& state);
    void setActive(bool active);   // 화면이 보일 때만 미리보기 갱신
    void showMessage(const QString& text, bool error);

signals:
    void playSceneRequested(const QString& sceneId);
    void commandRequested(const QString& line);
    void pauseToggleRequested();
    void goRequested();
    void restartRequested();
    void emergencyRequested();
    void editModeRequested();

private:
    QWidget* buildHeader();
    QWidget* buildTransport();
    void refreshCueList();
    void updateCueStatus(double time);

    ProjectController* m_controller;
    OperatorState m_state;
    QString m_cueSceneId;          // 큐 목록에 표시 중인 장면
    std::vector<double> m_cueTimes;

    QLabel* m_onAir;
    QLabel* m_sceneName;
    QLabel* m_clock;
    QLabel* m_status;
    ProgramMonitor* m_monitor;
    QLabel* m_programTitle;
    QLabel* m_nextCue;
    QPushButton* m_restart;
    QPushButton* m_pause;
    QPushButton* m_go;
    QLabel* m_cueWait;
    CueProgressBar* m_progress;
    OperatorSceneGrid* m_scenes;
    QListWidget* m_cueList;
    OperatorRegionControls* m_regions;
    QLabel* m_message;
};

} // namespace led
