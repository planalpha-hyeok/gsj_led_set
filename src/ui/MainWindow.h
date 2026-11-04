#pragma once

// 메인 편집 창: 메뉴/도구모음, 캔버스 화면, 설정 패널, 기록을 배치하고 재생/출력/스케줄을 제어

#include <QMainWindow>
#include <QTimer>

#include <memory>
#include <string>
#include <vector>

class QAction;
class QLabel;

namespace led {

class CanvasView;
class EmergencyHotkey;
class LogPanel;
class OutputManager;
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
};

} // namespace led
