#include "ui/MainWindow.h"

#include "core/Log.h"
#include "output/OutputManager.h"
#include "playback/PlaybackEngine.h"
#include "ui/CanvasPanel.h"
#include "ui/CanvasView.h"
#include "ui/Icons.h"
#include "ui/LogPanel.h"
#include "ui/OutputPanel.h"
#include "ui/ProjectController.h"
#include "ui/RegionPanel.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>

namespace led {

namespace {

const char kProjectFilter[] = "LED 프로젝트 (*.ledproj);;JSON (*.json);;모든 파일 (*.*)";
constexpr int kOutputRestartDelayMs = 300;
constexpr int kMaxErrorsInDialog = 12;

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_controller(new ProjectController(this))
    , m_engine(std::make_unique<PlaybackEngine>())
    , m_outputs(new OutputManager(m_engine.get(), this))
    , m_canvasView(new CanvasView(m_controller, this))
{
    setCentralWidget(m_canvasView);
    setMinimumSize(1000, 640);
    resize(1440, 880);

    createDocks();
    createActions();

    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);

    m_outputRestartTimer.setSingleShot(true);
    m_outputRestartTimer.setInterval(kOutputRestartDelayMs);
    connect(&m_outputRestartTimer, &QTimer::timeout, this, &MainWindow::restartOutputs);

    connect(m_controller, &ProjectController::regionsChanged, this, &MainWindow::onRegionsChanged);
    connect(m_controller, &ProjectController::outputsChanged, this, &MainWindow::onOutputsChanged);
    connect(m_controller, &ProjectController::canvasChanged, this, &MainWindow::updateStatus);
    connect(m_controller, &ProjectController::dirtyChanged, this, &MainWindow::updateTitle);
    connect(m_controller, &ProjectController::projectReset, this, [this] {
        updateTitle();
        updateStatus();
    });

    updateTitle();
    updateStatus();
}

MainWindow::~MainWindow()
{
    // 출력 스레드가 재생 엔진을 참조하므로 출력을 먼저 정지
    m_outputs->stop();
    m_engine->stop();
}

void MainWindow::createDocks()
{
    auto* tabs = new QTabWidget(this);
    tabs->addTab(new RegionPanel(m_controller, tabs), QStringLiteral("영역"));
    m_outputPanel = new OutputPanel(m_controller, tabs);
    tabs->addTab(m_outputPanel, QStringLiteral("출력"));
    tabs->addTab(new CanvasPanel(m_controller, tabs), QStringLiteral("캔버스"));

    auto* propertiesDock = new QDockWidget(QStringLiteral("설정"), this);
    propertiesDock->setObjectName("propertiesDock");
    propertiesDock->setWidget(tabs);
    propertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    propertiesDock->setMinimumWidth(400);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    m_logPanel = new LogPanel(this);
    auto* logDock = new QDockWidget(QStringLiteral("기록"), this);
    logDock->setObjectName("logDock");
    logDock->setWidget(m_logPanel);
    logDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                         QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
    resizeDocks({logDock}, {150}, Qt::Vertical);
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
    m_playAction->setToolTip(QStringLiteral("재생 (F5): 모든 영역을 처음부터 동시에 재생"));
    connect(m_playAction, &QAction::triggered, this, &MainWindow::play);

    m_stopAction = new QAction(icons::stop(), QStringLiteral("정지"), this);
    m_stopAction->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));
    m_stopAction->setToolTip(QStringLiteral("정지 (Shift+F5): 출력 화면은 검은색"));
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopPlayback);

    m_outputAction = new QAction(icons::output(), QStringLiteral("출력 켜기"), this);
    m_outputAction->setCheckable(true);
    m_outputAction->setShortcut(QKeySequence(Qt::Key_F6));
    m_outputAction->setToolTip(QStringLiteral("출력 켜기/끄기 (F6)"));
    connect(m_outputAction, &QAction::toggled, this, &MainWindow::setOutputsEnabled);

    m_testPatternAction =
        new QAction(icons::testPattern(), QStringLiteral("테스트 패턴"), this);
    m_testPatternAction->setCheckable(true);
    m_testPatternAction->setShortcut(QKeySequence(Qt::Key_F7));
    m_testPatternAction->setToolTip(QStringLiteral("테스트 패턴 (F7): 출력마다 격자, 좌표, 영역 위치 표시"));
    connect(m_testPatternAction, &QAction::toggled, this, &MainWindow::setTestPattern);

    auto* fitAction = new QAction(icons::fit(), QStringLiteral("화면 맞춤"), this);
    fitAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(fitAction, &QAction::triggered, m_canvasView, &CanvasView::fitCanvas);

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

    QMenu* outputMenu = menuBar()->addMenu(QStringLiteral("출력"));
    outputMenu->addAction(m_outputAction);
    outputMenu->addAction(m_testPatternAction);
    outputMenu->addSeparator();
    outputMenu->addAction(refreshDisplaysAction);

    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("보기"));
    viewMenu->addAction(fitAction);
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
    toolbar->addSeparator();
    toolbar->addAction(m_outputAction);
    toolbar->addAction(m_testPatternAction);
    toolbar->addSeparator();
    toolbar->addAction(fitAction);
}

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

void MainWindow::newProject()
{
    if (!maybeSave())
        return;
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
    stopPlayback();
    if (!m_controller->load(path, &error)) {
        logError("프로젝트 열기 실패: " + error.toStdString());
        QMessageBox::warning(this, QStringLiteral("열기 실패"), error);
        return false;
    }
    logInfo("프로젝트 열기: " + path.toStdString());
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
    statusBar()->showMessage(QStringLiteral("저장됨: %1").arg(path), 3000);
    updateTitle();
    return true;
}

void MainWindow::play()
{
    std::vector<std::string> errors;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_engine->play(m_controller->project(), errors);
    QApplication::restoreOverrideCursor();

    QStringList list;
    for (const auto& e : errors)
        list << QString::fromStdString(e);
    if (!list.isEmpty())
        reportErrors(QStringLiteral("일부 콘텐츠를 재생할 수 없음"), list);

    if (!m_outputs->isRunning())
        startOutputs();
    updateStatus();
}

void MainWindow::stopPlayback()
{
    m_engine->stop();
    updateStatus();
}

void MainWindow::setOutputsEnabled(bool enabled)
{
    if (enabled)
        startOutputs();
    else
        m_outputs->stop();
    updateStatus();
}

void MainWindow::startOutputs()
{
    QStringList errors;
    m_outputs->start(m_controller->project(), errors);
    if (!errors.isEmpty())
        reportErrors(QStringLiteral("일부 출력을 시작할 수 없음"), errors);
    {
        const QSignalBlocker blocker(m_outputAction);
        m_outputAction->setChecked(m_outputs->isRunning());
    }
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

void MainWindow::onRegionsChanged()
{
    if (m_engine->isPlaying()) {
        std::vector<std::string> errors;
        m_engine->update(m_controller->project(), errors);
        QStringList list;
        for (const auto& e : errors)
            list << QString::fromStdString(e);
        if (!list.isEmpty())
            reportErrors(QStringLiteral("콘텐츠를 재생할 수 없음"), list);
    }
    if (m_outputs->testPatternEnabled())
        m_outputs->setTestPattern(true, m_controller->project());
    updateStatus();
}

void MainWindow::onOutputsChanged()
{
    if (m_outputs->isRunning())
        m_outputRestartTimer.start();
    updateStatus();
}

void MainWindow::reportErrors(const QString& title, const QStringList& errors)
{
    for (const QString& e : errors)
        logError(e.toStdString());
    QStringList shown = errors.mid(0, kMaxErrorsInDialog);
    if (errors.size() > kMaxErrorsInDialog)
        shown << QStringLiteral("... 외 %1건 (아래 기록 창 참고)").arg(errors.size() - kMaxErrorsInDialog);
    QMessageBox::warning(this, title, shown.join(QStringLiteral("\n")));
}

void MainWindow::updateTitle()
{
    const QString file = m_controller->filePath().isEmpty() ? QStringLiteral("새 프로젝트")
                                                            : QFileInfo(m_controller->filePath()).fileName();
    setWindowTitle(QStringLiteral("%1%2 - GSJ LED 플레이어").arg(file, m_controller->isDirty() ? QStringLiteral("*") : QString()));
}

void MainWindow::updateStatus()
{
    const Project& p = m_controller->project();
    const QString playState = m_engine->isPlaying() ? QStringLiteral("▶ 재생 중") : QStringLiteral("■ 정지");
    const QString outputState = m_outputs->isRunning()
                                    ? QStringLiteral("출력 %1개 동작").arg(m_outputs->runningCount())
                                    : QStringLiteral("출력 꺼짐");
    m_statusLabel->setText(QStringLiteral("%1   |   %2%3   |   캔버스 %4 × %5")
                               .arg(playState, outputState,
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
    m_outputs->stop();
    m_engine->stop();
    event->accept();
}

} // namespace led
