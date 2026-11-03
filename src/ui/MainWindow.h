#pragma once

// 메인 편집 창: 메뉴/도구모음, 캔버스 화면, 속성 패널, 로그를 배치하고 재생/출력을 제어

#include <QMainWindow>
#include <QTimer>

#include <memory>

class QAction;
class QLabel;

namespace led {

class CanvasView;
class LogPanel;
class OutputManager;
class OutputPanel;
class PlaybackEngine;
class ProjectController;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openProjectFile(const QString& path);
    LogPanel* logPanel() const { return m_logPanel; }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    void play();
    void stopPlayback();
    void setOutputsEnabled(bool enabled);
    void setTestPattern(bool enabled);
    void onRegionsChanged();
    void onOutputsChanged();
    void restartOutputs();
    void updateTitle();
    void updateStatus();

private:
    void createActions();
    void createDocks();
    bool maybeSave();
    void reportErrors(const QString& title, const QStringList& errors);
    void startOutputs();

    ProjectController* m_controller;
    std::unique_ptr<PlaybackEngine> m_engine;
    OutputManager* m_outputs;
    CanvasView* m_canvasView;
    OutputPanel* m_outputPanel = nullptr;
    LogPanel* m_logPanel = nullptr;

    QAction* m_playAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_outputAction = nullptr;
    QAction* m_testPatternAction = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTimer m_outputRestartTimer;
};

} // namespace led
