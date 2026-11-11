// MainWindow: 운영 모드 (시안 C) 연결과 재생 상태 표시

#include "ui/MainWindow.h"

#include "core/ControlCommand.h"
#include "core/Log.h"
#include "output/OutputManager.h"
#include "playback/PlaybackEngine.h"
#include "schedule/ScheduleRunner.h"
#include "ui/CanvasView.h"
#include "ui/ProjectController.h"
#include "ui/operator/OperatorPanel.h"

#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolBar>

namespace led {

void MainWindow::setupOperator()
{
    // 가운데: 편집(캔버스) / 운영 화면 전환
    QWidget* canvas = takeCentralWidget();
    m_centerStack = new QStackedWidget(this);
    m_centerStack->addWidget(canvas);
    m_operatorPanel = new OperatorPanel(m_controller, m_engine.get(), m_centerStack);
    m_centerStack->addWidget(m_operatorPanel);
    setCentralWidget(m_centerStack);

    m_operatorAction = new QAction(QStringLiteral("운영 모드"), this);
    m_operatorAction->setCheckable(true);
    m_operatorAction->setShortcut(QKeySequence(Qt::Key_F9));
    m_operatorAction->setToolTip(QStringLiteral("운영 모드 (F9): 행사 중 송출 화면 (송출 미리보기, GO, 장면 버튼, 큐 목록)"));
    connect(m_operatorAction, &QAction::toggled, this, &MainWindow::setOperatorMode);
    if (m_toolbar) {
        m_toolbar->addSeparator();
        m_toolbar->addAction(m_operatorAction);
    }
    for (QMenu* menu : menuBar()->findChildren<QMenu*>())
        if (menu->title() == QStringLiteral("보기")) {
            menu->insertAction(menu->actions().isEmpty() ? nullptr : menu->actions().first(), m_operatorAction);
            break;
        }

    connect(m_operatorPanel, &OperatorPanel::playSceneRequested, this, &MainWindow::operatorPlayScene);
    connect(m_operatorPanel, &OperatorPanel::commandRequested, this, [this](const QString& line) {
        const QString error = executeCommand(line, QStringLiteral("운영 화면"));
        m_operatorPanel->showMessage(error.isEmpty() ? QStringLiteral("실행: %1").arg(line) : error, !error.isEmpty());
    });
    connect(m_operatorPanel, &OperatorPanel::pauseToggleRequested, this, [this] {
        if (!m_engine->isPlaying()) {
            operatorPlayScene(m_controller->editingSceneId());
            return;
        }
        const TimelineStatus st = m_engine->timelineStatus();
        if (st.running)
            m_engine->timelinePause();
        else
            m_engine->timelineResume();
    });
    connect(m_operatorPanel, &OperatorPanel::goRequested, this, [this] { m_engine->timelineResume(); });
    connect(m_operatorPanel, &OperatorPanel::restartRequested, this, [this] { m_engine->timelineSeek(0.0); });
    connect(m_operatorPanel, &OperatorPanel::emergencyRequested, this, &MainWindow::emergencyStop);
    connect(m_operatorPanel, &OperatorPanel::editModeRequested, this, [this] { m_operatorAction->setChecked(false); });
}

void MainWindow::setOperatorMode(bool enabled)
{
    const bool showing = m_centerStack->currentWidget() == m_operatorPanel;
    if (enabled == showing)
        return;
    if (enabled) {
        // 편집 화면 배치를 기억하고 운영 화면만 표시
        m_editLayoutState = saveState();
        for (QDockWidget* dock : findChildren<QDockWidget*>())
            dock->hide();
        if (m_toolbar)
            m_toolbar->hide();
        m_centerStack->setCurrentWidget(m_operatorPanel);
        m_operatorPanel->setActive(true);
        m_operatorPanel->setFocus();
        logInfo("운영 모드");
    } else {
        m_operatorPanel->setActive(false);
        m_centerStack->setCurrentIndex(0);
        if (!m_editLayoutState.isEmpty())
            restoreState(m_editLayoutState);
        if (m_toolbar)
            m_toolbar->show();
        logInfo("편집 화면");
    }
    refreshPlaybackIndicators();
}

void MainWindow::operatorPlayScene(const QString& sceneId)
{
    // 운영 중에는 대화상자를 띄우지 않음 (오류는 기록 창과 운영 화면 메시지)
    if (m_schedule->isActive()) {
        const QSignalBlocker blocker(m_scheduleAction);
        m_scheduleAction->setChecked(false);
        m_schedule->setActive(false);
        logInfo("운영 화면에서 장면을 바꿔 스케줄 운영을 끔");
    }
    startScene(sceneId, false);
    const Scene* scene = m_controller->project().findScene(sceneId.toStdString());
    m_operatorPanel->showMessage(QStringLiteral("장면 전환: %1").arg(scene ? QString::fromStdString(scene->name) : sceneId),
                                 false);
}

void MainWindow::refreshPlaybackIndicators()
{
    const bool playing = m_engine->isPlaying();
    const TimelineStatus st = playing ? m_engine->timelineStatus() : TimelineStatus{};

    if (m_clockLabel) {
        QString text = QStringLiteral("■ 정지");
        if (playing) {
            const QString length = st.length > 0.0 ? QString::fromStdString(formatTimeText(st.length))
                                                   : QStringLiteral("끝없음");
            const QString mark = st.finished ? QStringLiteral("■") : st.running ? QStringLiteral("●") : QStringLiteral("❚❚");
            text = QStringLiteral("%1  %2 / %3").arg(mark, QString::fromStdString(formatTimeText(st.time)), length);
        }
        if (m_clockLabel->text() != text)
            m_clockLabel->setText(text);
    }

    if (m_operatorPanel && m_centerStack && m_centerStack->currentWidget() == m_operatorPanel) {
        OperatorState s;
        s.playing = playing;
        s.sceneId = playing ? QString::fromStdString(m_engine->sceneId()) : QString();
        s.timeline = st;
        s.outputsRunning = m_outputs->isRunning() ? m_outputs->runningCount() : 0;
        for (const OutputConfig& o : m_controller->project().outputs)
            if (o.enabled)
                ++s.outputsTotal;
        s.testPattern = m_outputs->testPatternEnabled();
        s.scheduleActive = m_schedule->isActive();
        s.scheduleText = m_schedule->statusText();
        m_operatorPanel->setState(s);
    }
}

} // namespace led
