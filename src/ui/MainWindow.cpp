#include "ui/MainWindow.h"

#include "core/Log.h"
#include "display/DisplayInfo.h"
#include "output/OutputManager.h"
#include "playback/PlaybackEngine.h"
#include "schedule/ScheduleRunner.h"
#include "ui/CanvasPanel.h"
#include "ui/CanvasView.h"
#include "ui/EmergencyHotkey.h"
#include "ui/Icons.h"
#include "ui/InteractivePanel.h"
#include "ui/LogPanel.h"
#include "ui/OutputPanel.h"
#include "ui/ProjectController.h"
#include "ui/RegionPanel.h"
#include "ui/ScenePanel.h"
#include "ui/SchedulePanel.h"
#include "ui/timeline/TimelineEditor.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>

namespace led {

namespace {

const char kProjectFilter[] = "LED 프로젝트 (*.ledproj);;JSON (*.json);;모든 파일 (*.*)";
constexpr int kOutputRestartDelayMs = 300;
constexpr int kMaxErrorsInDialog = 12;

void setCheckedSilently(QAction* action, bool checked)
{
    const QSignalBlocker blocker(action);
    action->setChecked(checked);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_controller(new ProjectController(this))
    , m_engine(std::make_unique<PlaybackEngine>())
    , m_outputs(new OutputManager(m_engine.get(), this))
    , m_schedule(new ScheduleRunner([this]() -> const Project& { return m_controller->project(); }, this))
    , m_canvasView(new CanvasView(m_controller, this))
{
    setCentralWidget(m_canvasView);
    setMinimumSize(1100, 700);
    resize(1500, 920);

    createDocks();
    createActions();

    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);

    m_outputRestartTimer.setSingleShot(true);
    m_outputRestartTimer.setInterval(kOutputRestartDelayMs);
    connect(&m_outputRestartTimer, &QTimer::timeout, this, &MainWindow::restartOutputs);

    connect(m_controller, &ProjectController::regionsChanged, this, &MainWindow::onRegionsChanged);
    connect(m_controller, &ProjectController::contentChanged, this, &MainWindow::onPlaybackContentChanged);
    connect(m_controller, &ProjectController::scenesChanged, this, &MainWindow::onPlaybackContentChanged);
    connect(m_controller, &ProjectController::playbackSettingsChanged, this, &MainWindow::onPlaybackContentChanged);
    connect(m_controller, &ProjectController::timelineChanged, this, &MainWindow::onPlaybackContentChanged);
    connect(m_controller, &ProjectController::outputsChanged, this, &MainWindow::onOutputsChanged);
    connect(m_controller, &ProjectController::canvasChanged, this, &MainWindow::updateStatus);
    connect(m_controller, &ProjectController::dirtyChanged, this, &MainWindow::updateTitle);
    connect(m_controller, &ProjectController::scheduleChanged, this, [this] {
        m_schedule->reevaluate();
        updateStatus();
    });
    connect(m_controller, &ProjectController::projectReset, this, [this] {
        updateTitle();
        updateStatus();
    });

    connect(m_schedule, &ScheduleRunner::sceneRequested, this, &MainWindow::onScheduledScene);
    connect(m_schedule, &ScheduleRunner::statusChanged, m_schedulePanel, &SchedulePanel::setStatusText);
    connect(m_schedule, &ScheduleRunner::statusChanged, this, &MainWindow::updateStatus);
    connect(m_scenePanel, &ScenePanel::playSceneRequested, this, &MainWindow::playScene);
    m_schedulePanel->setStatusText(QString());

    // 출력 화면이 편집 화면을 가려도 쓸 수 있는 비상 단축키
    setupInteractive();
    setupTimeline();

    m_hotkey = new EmergencyHotkey(this, this);
    connect(m_hotkey, &EmergencyHotkey::triggered, this, &MainWindow::emergencyStop);

    updateTitle();
    updateStatus();
}

MainWindow::~MainWindow()
{
    // 출력 스레드가 재생 엔진을 참조하므로 출력을 먼저 정지
    m_schedule->setActive(false);
    m_outputs->stop();
    m_engine->stop();
}

void MainWindow::createDocks()
{
    auto* tabs = new QTabWidget(this);
    tabs->addTab(new RegionPanel(m_controller, tabs), QStringLiteral("영역"));
    m_scenePanel = new ScenePanel(m_controller, tabs);
    tabs->addTab(m_scenePanel, QStringLiteral("장면"));
    m_schedulePanel = new SchedulePanel(m_controller, tabs);
    tabs->addTab(m_schedulePanel, QStringLiteral("스케줄"));
    m_outputPanel = new OutputPanel(m_controller, tabs);
    tabs->addTab(m_outputPanel, QStringLiteral("출력"));
    tabs->addTab(new CanvasPanel(m_controller, tabs), QStringLiteral("캔버스"));
    m_interactivePanel = new InteractivePanel(m_controller, tabs);
    tabs->addTab(m_interactivePanel, QStringLiteral("인터랙티브"));

    auto* propertiesDock = new QDockWidget(QStringLiteral("설정"), this);
    propertiesDock->setObjectName("propertiesDock");
    propertiesDock->setWidget(tabs);
    propertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    propertiesDock->setMinimumWidth(460);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    m_logPanel = new LogPanel(this);
    auto* logDock = new QDockWidget(QStringLiteral("기록"), this);
    logDock->setObjectName("logDock");
    logDock->setWidget(m_logPanel);
    logDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                         QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

    m_timelineEditor = new TimelineEditor(m_controller, this);
    m_timelineDock = new QDockWidget(QStringLiteral("타임라인"), this);
    m_timelineDock->setObjectName("timelineDock");
    m_timelineDock->setWidget(m_timelineEditor);
    m_timelineDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                                QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::BottomDockWidgetArea, m_timelineDock);
    tabifyDockWidget(logDock, m_timelineDock);
    logDock->raise();
    resizeDocks({logDock}, {230}, Qt::Vertical);
}

void MainWindow::createActions()
{
    auto* newAction = new QAction(icons::newProject(), QStringLiteral("새 프로젝트"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newProject);

    auto* openAction = new QAction(icons::open(), QStringLiteral("열기"), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openProject);

    auto* saveAction = new QAction(icons::save(), QStringLiteral("저장"), this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveProject);

    auto* saveAsAction = new QAction(QStringLiteral("다른 이름으로 저장..."), this);
    saveAsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveProjectAs);

    auto* quitAction = new QAction(QStringLiteral("종료"), this);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    m_playAction = new QAction(icons::play(), QStringLiteral("재생"), this);
    m_playAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_playAction->setToolTip(QStringLiteral("재생 (F5): 편집 중인 장면을 처음부터 재생"));
    connect(m_playAction, &QAction::triggered, this, &MainWindow::play);

    m_stopAction = new QAction(icons::stop(), QStringLiteral("정지"), this);
    m_stopAction->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));
    m_stopAction->setToolTip(QStringLiteral("정지 (Shift+F5): 출력 화면은 검은색. 스케줄 운영도 끔"));
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

    m_scheduleAction = new QAction(icons::schedule(), QStringLiteral("스케줄 운영"), this);
    m_scheduleAction->setCheckable(true);
    m_scheduleAction->setShortcut(QKeySequence(Qt::Key_F8));
    m_scheduleAction->setToolTip(QStringLiteral("스케줄 운영 (F8): 시각에 맞춰 장면을 자동 전환"));
    connect(m_scheduleAction, &QAction::toggled, this, &MainWindow::setScheduleMode);

    m_outputAction = new QAction(icons::output(), QStringLiteral("출력 켜기"), this);
    m_outputAction->setCheckable(true);
    m_outputAction->setShortcut(QKeySequence(Qt::Key_F6));
    m_outputAction->setToolTip(QStringLiteral("출력 켜기/끄기 (F6)\n출력 화면에 가려 조작할 수 없으면 Ctrl+Alt+F12"));
    connect(m_outputAction, &QAction::toggled, this, &MainWindow::setOutputsEnabled);

    m_testPatternAction = new QAction(icons::testPattern(), QStringLiteral("테스트 패턴"), this);
    m_testPatternAction->setCheckable(true);
    m_testPatternAction->setShortcut(QKeySequence(Qt::Key_F7));
    m_testPatternAction->setToolTip(QStringLiteral("테스트 패턴 (F7): 출력마다 격자, 좌표, 영역 위치 표시\n끌 수 없으면 Ctrl+Alt+F12"));
    connect(m_testPatternAction, &QAction::toggled, this, &MainWindow::setTestPattern);

    auto* snapAction = new QAction(icons::magnet(), QStringLiteral("자석"), this);
    snapAction->setCheckable(true);
    snapAction->setChecked(m_canvasView->snapEnabled());
    snapAction->setToolTip(QStringLiteral("자석: 영역을 옮기거나 크기를 바꿀 때 캔버스/출력/다른 영역 테두리에 붙음\n"
                                          "드래그 중 Alt를 누르고 있으면 일시 해제"));
    connect(snapAction, &QAction::toggled, m_canvasView, &CanvasView::setSnapEnabled);

    auto* fitAction = new QAction(icons::fit(), QStringLiteral("화면 맞춤"), this);
    fitAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(fitAction, &QAction::triggered, m_canvasView, &CanvasView::fitCanvas);

    auto* emergencyAction = new QAction(QStringLiteral("모든 출력 끄기 (비상: Ctrl+Alt+F12)"), this);
    connect(emergencyAction, &QAction::triggered, this, &MainWindow::emergencyStop);

    auto* refreshDisplaysAction = new QAction(QStringLiteral("디스플레이 다시 찾기"), this);
    connect(refreshDisplaysAction, &QAction::triggered, m_outputPanel, &OutputPanel::refreshDisplays);

    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("파일"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);

    QMenu* playMenu = menuBar()->addMenu(QStringLiteral("재생"));
    playMenu->addAction(m_playAction);
    playMenu->addAction(m_stopAction);
    playMenu->addSeparator();
    playMenu->addAction(m_scheduleAction);

    QMenu* outputMenu = menuBar()->addMenu(QStringLiteral("출력"));
    outputMenu->addAction(m_outputAction);
    outputMenu->addAction(m_testPatternAction);
    outputMenu->addAction(emergencyAction);
    outputMenu->addSeparator();
    outputMenu->addAction(refreshDisplaysAction);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("보기"));
    viewMenu->addAction(fitAction);
    viewMenu->addAction(snapAction);
    for (QDockWidget* dock : findChildren<QDockWidget*>())
        viewMenu->addAction(dock->toggleViewAction());

    QToolBar* toolbar = addToolBar(QStringLiteral("도구"));
    toolbar->setObjectName("mainToolbar");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->addAction(newAction);
    toolbar->addAction(openAction);
    toolbar->addAction(saveAction);
    toolbar->addSeparator();
    toolbar->addAction(m_playAction);
    toolbar->addAction(m_stopAction);
    toolbar->addAction(m_scheduleAction);
    toolbar->addSeparator();
    toolbar->addAction(m_outputAction);
    toolbar->addAction(m_testPatternAction);
    toolbar->addSeparator();
    toolbar->addAction(snapAction);
    toolbar->addAction(fitAction);
}

// ---------------------------------------------------------------- 파일

bool MainWindow::maybeSave()
{
    if (!m_controller->isDirty())
        return true;
    const auto answer = QMessageBox::question(
        this, QStringLiteral("저장 확인"), QStringLiteral("변경 내용을 저장하시겠습니까?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return saveProject();
    return answer == QMessageBox::Discard;
}

void MainWindow::rememberProject(const QString& path)
{
    QSettings settings;
    settings.setValue(QStringLiteral("lastProject"), QFileInfo(path).absoluteFilePath());
}

void MainWindow::newProject()
{
    if (!maybeSave())
        return;
    setCheckedSilently(m_scheduleAction, false);
    m_schedule->setActive(false);
    stopPlayback();
    m_controller->newProject();
    if (m_outputs->isRunning())
        restartOutputs();
    logInfo("새 프로젝트");
}

void MainWindow::openProject()
{
    if (!maybeSave())
        return;
    const QString start = m_controller->filePath().isEmpty() ? QString() : QFileInfo(m_controller->filePath()).path();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("프로젝트 열기"), start,
                                                      QString::fromUtf8(kProjectFilter));
    if (!path.isEmpty())
        openProjectFile(path);
}

bool MainWindow::openProjectFile(const QString& path)
{
    QString error;
    setCheckedSilently(m_scheduleAction, false);
    m_schedule->setActive(false);
    stopPlayback();
    if (!m_controller->load(path, &error)) {
        logError("프로젝트 열기 실패: " + error.toStdString());
        QMessageBox::warning(this, QStringLiteral("열기 실패"), error);
        return false;
    }
    logInfo("프로젝트 열기: " + path.toStdString());
    rememberProject(path);
    if (m_outputs->isRunning())
        restartOutputs();
    return true;
}

bool MainWindow::saveProject()
{
    if (m_controller->filePath().isEmpty())
        return saveProjectAs();
    QString error;
    if (!m_controller->save(m_controller->filePath(), &error)) {
        QMessageBox::warning(this, QStringLiteral("저장 실패"), error);
        return false;
    }
    rememberProject(m_controller->filePath());
    statusBar()->showMessage(QStringLiteral("저장됨: %1").arg(m_controller->filePath()), 3000);
    updateTitle();
    return true;
}

bool MainWindow::saveProjectAs()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("프로젝트 저장"),
                                                m_controller->filePath().isEmpty() ? QStringLiteral("project.ledproj")
                                                                                   : m_controller->filePath(),
                                                QString::fromUtf8(kProjectFilter));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".ledproj");
    QString error;
    if (!m_controller->save(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("저장 실패"), error);
        return false;
    }
    rememberProject(path);
    statusBar()->showMessage(QStringLiteral("저장됨: %1").arg(path), 3000);
    updateTitle();
    return true;
}

// ---------------------------------------------------------------- 재생

void MainWindow::startScene(const QString& sceneId, bool showDialog)
{
    std::vector<std::string> errors;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_engine->play(playbackProject(), sceneId.toStdString(), errors);
    QApplication::restoreOverrideCursor();
    if (!errors.empty())
        reportErrors(QStringLiteral("일부 콘텐츠를 재생할 수 없음"), errors, showDialog);
    if (!m_outputs->isRunning())
        startOutputs(showDialog);
    updateStatus();
}

void MainWindow::play()
{
    playScene(m_controller->editingSceneId());
}

void MainWindow::playScene(const QString& sceneId)
{
    // 직접 재생하면 스케줄 운영은 끔
    if (m_schedule->isActive()) {
        setCheckedSilently(m_scheduleAction, false);
        m_schedule->setActive(false);
    }
    startScene(sceneId, true);
}

void MainWindow::stopPlayback()
{
    if (m_schedule->isActive()) {
        setCheckedSilently(m_scheduleAction, false);
        m_schedule->setActive(false);
    }
    m_engine->stop();
    updateStatus();
}

void MainWindow::setScheduleMode(bool enabled)
{
    if (enabled && m_controller->project().schedule.entries.empty() &&
        m_controller->project().schedule.outside == OutsideBehavior::Black) {
        QMessageBox::information(this, QStringLiteral("스케줄 운영"),
                                 QStringLiteral("등록된 일정이 없습니다. '스케줄' 탭에서 일정을 추가하세요."));
    }
    if (enabled && !m_outputs->isRunning())
        startOutputs();
    m_schedule->setActive(enabled);
    updateStatus();
}

void MainWindow::onScheduledScene(const QString& sceneId)
{
    if (sceneId.isEmpty()) {
        m_engine->stop();
        updateStatus();
        return;
    }
    if (m_engine->isPlaying() && QString::fromStdString(m_engine->sceneId()) == sceneId)
        return;
    // 무인 운영 중에는 대화상자 없이 기록만 남김
    startScene(sceneId, false);
}

void MainWindow::startAutomaticallyIfEnabled()
{
    const Project& p = m_controller->project();
    if (!p.autoStart)
        return;
    logInfo("자동 실행");
    startOutputs(false);
    if (p.schedule.enabled) {
        setCheckedSilently(m_scheduleAction, true);
        m_schedule->setActive(true);
    } else if (!p.scenes.empty()) {
        startScene(QString::fromStdString(p.scenes.front().id), false);
    }
    updateStatus();
}

// ---------------------------------------------------------------- 출력

void MainWindow::setOutputsEnabled(bool enabled)
{
    if (enabled)
        startOutputs();
    else
        m_outputs->stop();
    updateStatus();
}

void MainWindow::warnIfPrimaryOutput()
{
    if (m_primaryWarningShown)
        return;
    QSettings settings;
    if (settings.value(QStringLiteral("hidePrimaryOutputWarning"), false).toBool())
        return;
    const std::vector<DisplayInfo> displays = enumerateDisplays();
    bool onPrimary = false;
    for (const OutputConfig& o : m_controller->project().outputs) {
        const DisplayInfo* d = findDisplay(displays, o.displayId);
        if (o.enabled && d && d->primary)
            onPrimary = true;
    }
    if (!onPrimary)
        return;
    m_primaryWarningShown = true;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("주 모니터 출력"));
    box.setText(QStringLiteral("편집 화면이 있는 주 모니터에도 출력합니다.\n"
                               "출력 화면이 편집 화면을 가리면 아래 방법으로 끌 수 있습니다."));
    box.setInformativeText(QStringLiteral("• Ctrl+Alt+F12 : 모든 출력과 테스트 패턴 끄기 (어느 화면에서나 동작)\n"
                                          "• Alt+Tab 으로 편집 화면으로 돌아가 F6 (출력 끄기) / F7 (테스트 패턴)"));
    auto* dontShow = new QCheckBox(QStringLiteral("다시 보지 않기"), &box);
    box.setCheckBox(dontShow);
    box.exec();
    if (dontShow->isChecked())
        settings.setValue(QStringLiteral("hidePrimaryOutputWarning"), true);
}

void MainWindow::emergencyStop()
{
    logWarning("비상 단축키: 모든 출력과 테스트 패턴을 끕니다");
    setCheckedSilently(m_testPatternAction, false);
    m_outputs->setTestPattern(false, m_controller->project());
    m_outputs->stop();
    setCheckedSilently(m_outputAction, false);
    // 편집 화면을 앞으로
    showNormal();
    raise();
    activateWindow();
    updateStatus();
}

void MainWindow::startOutputs(bool showDialog)
{
    if (showDialog)
        warnIfPrimaryOutput();
    QStringList errors;
    m_outputs->start(m_controller->project(), errors);
    if (!errors.isEmpty())
        reportErrors(QStringLiteral("일부 출력을 시작할 수 없음"), errors, showDialog);
    setCheckedSilently(m_outputAction, m_outputs->isRunning());
    updateStatus();
}

void MainWindow::restartOutputs()
{
    if (!m_outputs->isRunning() && !m_outputAction->isChecked())
        return;
    startOutputs();
}

void MainWindow::setTestPattern(bool enabled)
{
    m_outputs->setTestPattern(enabled, m_controller->project());
    if (enabled && !m_outputs->isRunning())
        startOutputs();
    updateStatus();
}

// ---------------------------------------------------------------- 변경 반영

void MainWindow::onRegionsChanged()
{
    onPlaybackContentChanged();
    if (m_outputs->testPatternEnabled())
        m_outputs->setTestPattern(true, m_controller->project());
}

void MainWindow::onPlaybackContentChanged()
{
    if (m_engine->isPlaying()) {
        std::vector<std::string> errors;
        m_engine->update(playbackProject(), errors);
        if (!errors.empty())
            reportErrors(QStringLiteral("콘텐츠를 재생할 수 없음"), errors, !m_schedule->isActive());
    }
    updateStatus();
}

void MainWindow::onOutputsChanged()
{
    if (m_outputs->isRunning())
        m_outputRestartTimer.start();
    updateStatus();
}

void MainWindow::reportErrors(const QString& title, const std::vector<std::string>& errors, bool showDialog)
{
    QStringList list;
    for (const auto& e : errors)
        list << QString::fromStdString(e);
    reportErrors(title, list, showDialog);
}

void MainWindow::reportErrors(const QString& title, const QStringList& errors, bool showDialog)
{
    for (const QString& e : errors)
        logError(e.toStdString());
    if (!showDialog)
        return;
    QStringList shown = errors.mid(0, kMaxErrorsInDialog);
    if (errors.size() > kMaxErrorsInDialog)
        shown << QStringLiteral("... 외 %1건 (아래 기록 창 참고)").arg(errors.size() - kMaxErrorsInDialog);
    QMessageBox::warning(this, title, shown.join(QStringLiteral("\n")));
}

void MainWindow::updateTitle()
{
    const QString file = m_controller->filePath().isEmpty() ? QStringLiteral("새 프로젝트")
                                                            : QFileInfo(m_controller->filePath()).fileName();
    setWindowTitle(QStringLiteral("%1%2 - GSJ LED 플레이어 %3")
                       .arg(file, m_controller->isDirty() ? QStringLiteral("*") : QString(),
                            QApplication::applicationVersion()));
}

void MainWindow::updateStatus()
{
    const Project& p = m_controller->project();
    QString playState = QStringLiteral("■ 정지");
    QString playingScene;
    if (m_engine->isPlaying()) {
        playingScene = QString::fromStdString(m_engine->sceneId());
        const Scene* scene = p.findScene(playingScene.toStdString());
        playState = QStringLiteral("▶ %1").arg(scene ? QString::fromStdString(scene->name) : QStringLiteral("재생 중"));
    }
    m_scenePanel->setPlayingScene(playingScene);

    const QString scheduleState =
        m_schedule->isActive() ? QStringLiteral("   |   ⏰ %1").arg(m_schedule->statusText()) : QString();
    const QString outputState = m_outputs->isRunning()
                                    ? QStringLiteral("출력 %1개 동작").arg(m_outputs->runningCount())
                                    : QStringLiteral("출력 꺼짐");
    m_statusLabel->setText(QStringLiteral("%1%2   |   %3%4   |   캔버스 %5 × %6")
                               .arg(playState, scheduleState, outputState,
                                    m_outputs->testPatternEnabled() ? QStringLiteral(" (테스트 패턴)") : QString())
                               .arg(p.canvas.width)
                               .arg(p.canvas.height));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    m_schedule->setActive(false);
    m_outputs->stop();
    m_engine->stop();
    event->accept();
}

} // namespace led
