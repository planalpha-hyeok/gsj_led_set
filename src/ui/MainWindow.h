#pragma once

// 메인 편집 창: 메뉴/도구모음, 캔버스 화면, 설정 패널, 기록을 배치하고 재생/출력/스케줄을 제어

#include "core/Project.h"
#include "playback/RuntimeOverrides.h"

#include <QMainWindow>
#include <QTimer>

#include <memory>
#include <string>
#include <vector>

class QAction;
class QDockWidget;
class QLabel;
class QStackedWidget;
class QToolBar;

namespace led {

class CanvasView;
class EmergencyHotkey;
class InteractiveHub;
class InteractivePanel;
class TimelineEditor;
class LogPanel;
class OutputManager;
class OperatorPanel;
class OutputPanel;
class PlaybackEngine;
class ProjectController;
class ScenePanel;
class SchedulePanel;
class ScheduleRunner;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openProjectFile(const QString& path);
    LogPanel* logPanel() const { return m_logPanel; }

    // 프로젝트의 '자동 실행'이 켜져 있으면 출력 켜기 + 재생(또는 스케줄 운영)
    void startAutomaticallyIfEnabled();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    void play();
    void playScene(const QString& sceneId);
    void stopPlayback();
    void setOutputsEnabled(bool enabled);
    void setTestPattern(bool enabled);
    void setScheduleMode(bool enabled);
    void onScheduledScene(const QString& sceneId);
    void onRegionsChanged();
    void onPlaybackContentChanged();
    void onOutputsChanged();
    void restartOutputs();
    void updateTitle();
    void updateStatus();
    void emergencyStop();

    // 인터랙티브 (MainWindowInteractive.cpp)
    QString executeCommand(const QString& line, const QString& source);
    void onTrigger(const QString& trigger, const QString& source);
    void onCanvasClicked(int x, int y);
    void applyInteractiveConfig();

    // 운영 모드 (MainWindowOperator.cpp)
    void setOperatorMode(bool enabled);

private:
    void createActions();
    void createDocks();
    bool maybeSave();
    void reportErrors(const QString& title, const QStringList& errors, bool showDialog = true);
    void reportErrors(const QString& title, const std::vector<std::string>& errors, bool showDialog = true);
    void startOutputs(bool showDialog = true);
    void rememberProject(const QString& path);
    void startScene(const QString& sceneId, bool showDialog);
    void warnIfPrimaryOutput();
    void setupInteractive();
    void setupTimeline();
    void setupOperator();
    void refreshPlaybackIndicators();   // 도구모음 시계 + 운영 화면 상태 (초당 20번)
    void operatorPlayScene(const QString& sceneId);
    // 재생 엔진에 넘길 프로젝트 (외부 명령으로 바꾼 자막/표시 적용)
    Project playbackProject() const;
    void refreshPlaybackFromOverrides();

    ProjectController* m_controller;
    std::unique_ptr<PlaybackEngine> m_engine;
    OutputManager* m_outputs;
    ScheduleRunner* m_schedule;
    CanvasView* m_canvasView;
    OutputPanel* m_outputPanel = nullptr;
    ScenePanel* m_scenePanel = nullptr;
    SchedulePanel* m_schedulePanel = nullptr;
    LogPanel* m_logPanel = nullptr;

    QAction* m_playAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_outputAction = nullptr;
    QAction* m_testPatternAction = nullptr;
    QAction* m_scheduleAction = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTimer m_outputRestartTimer;
    EmergencyHotkey* m_hotkey = nullptr;
    bool m_primaryWarningShown = false;

    TimelineEditor* m_timelineEditor = nullptr;
    QDockWidget* m_timelineDock = nullptr;
    QTimer m_timelineStatusTimer;
    InteractiveHub* m_hub = nullptr;
    InteractivePanel* m_interactivePanel = nullptr;
    RuntimeOverrides m_overrides;
    InteractiveConfig m_appliedInteractive;
    bool m_interactiveApplied = false;
    int m_triggerDepth = 0;

    QToolBar* m_toolbar = nullptr;
    QLabel* m_clockLabel = nullptr;
    QStackedWidget* m_centerStack = nullptr;
    OperatorPanel* m_operatorPanel = nullptr;
    QAction* m_operatorAction = nullptr;
    QByteArray m_editLayoutState;       // 운영 모드로 바꾸기 전 편집 화면 배치
};

} // namespace led
