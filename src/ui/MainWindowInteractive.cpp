// MainWindow의 인터랙티브(외부 명령/트리거/터치) 처리 부분

#include "ui/MainWindow.h"

#include "core/ControlCommand.h"
#include "core/Log.h"
#include "interactive/InteractiveHub.h"
#include "media/AudioOutput.h"
#include "output/OutputManager.h"
#include "playback/PlaybackEngine.h"
#include "schedule/ScheduleRunner.h"
#include "ui/InteractivePanel.h"
#include "ui/ProjectController.h"
#include "ui/timeline/TimelineEditor.h"

#include <QDockWidget>

#include <QAction>
#include <QSignalBlocker>

namespace led {

namespace {

// 이름이 같으면 그 항목, 없으면 1부터 시작하는 번호로 찾음
template <typename T>
const T* findByNameOrNumber(const std::vector<T>& list, const std::string& key)
{
    for (const T& item : list)
        if (item.name == key)
            return &item;
    bool ok = false;
    const int number = QString::fromStdString(key).toInt(&ok);
    if (ok && number >= 1 && number <= static_cast<int>(list.size()))
        return &list[static_cast<std::size_t>(number - 1)];
    return nullptr;
}

const TriggerMapping* findMapping(const InteractiveConfig& config, const QString& trigger)
{
    for (const TriggerMapping& m : config.mappings)
        if (QString::fromStdString(m.trigger).trimmed().compare(trigger, Qt::CaseInsensitive) == 0)
            return &m;
    return nullptr;
}

} // namespace

void MainWindow::setupInteractive()
{
    m_hub = new InteractiveHub(this);
    m_hub->setHandler([this](const QString& command, const QString& source) { return executeCommand(command, source); });
    connect(m_hub, &InteractiveHub::triggerReceived, this, &MainWindow::onTrigger);
    connect(m_hub, &InteractiveHub::statusChanged, m_interactivePanel, &InteractivePanel::setStatusText);
    connect(m_outputs, &OutputManager::canvasClicked, this, &MainWindow::onCanvasClicked);
    connect(m_interactivePanel, &InteractivePanel::commandRequested, this, [this](const QString& command) {
        const QString error = executeCommand(command, QStringLiteral("시험"));
        m_interactivePanel->setCommandResult(error.isEmpty() ? QStringLiteral("실행됨: %1").arg(command)
                                                             : QStringLiteral("실패: %1").arg(error));
    });
    connect(m_controller, &ProjectController::interactiveChanged, this, &MainWindow::applyInteractiveConfig);
    connect(m_controller, &ProjectController::projectReset, this, [this] {
        m_overrides.clear();
        applyInteractiveConfig();
    });
    applyInteractiveConfig();
}

void MainWindow::applyInteractiveConfig()
{
    InteractiveConfig wanted = m_controller->project().interactive;
    // 트리거 연결표만 바뀐 경우에는 수신을 다시 시작하지 않음
    InteractiveConfig a = wanted;
    InteractiveConfig b = m_appliedInteractive;
    a.mappings.clear();
    b.mappings.clear();
    const bool same = a.udpEnabled == b.udpEnabled && a.udpPort == b.udpPort && a.tcpEnabled == b.tcpEnabled &&
                      a.tcpPort == b.tcpPort && a.oscEnabled == b.oscEnabled && a.oscPort == b.oscPort &&
                      a.serialEnabled == b.serialEnabled && a.serialPort == b.serialPort &&
                      a.serialBaud == b.serialBaud && a.midiEnabled == b.midiEnabled && a.midiDevice == b.midiDevice;
    m_appliedInteractive = wanted;
    if (same && m_interactiveApplied)
        return;
    m_interactiveApplied = true;
    m_hub->apply(wanted);
}

Project MainWindow::playbackProject() const
{
    return m_overrides.apply(m_controller->project());
}

void MainWindow::refreshPlaybackFromOverrides()
{
    if (!m_engine->isPlaying())
        return;
    std::vector<std::string> errors;
    m_engine->update(playbackProject(), errors);
    for (const auto& e : errors)
        logError(e);
}

QString MainWindow::executeCommand(const QString& line, const QString& source)
{
    logInfo("명령 [" + source.toStdString() + "] " + line.toStdString());
    ControlCommand cmd;
    std::string parseError;
    if (!parseCommand(line.toStdString(), cmd, parseError)) {
        logWarning("명령 해석 실패: " + parseError);
        return QString::fromStdString(parseError);
    }

    const Project& project = m_controller->project();
    auto fail = [](const std::string& msg) {
        logWarning("명령 실패: " + msg);
        return QString::fromStdString(msg);
    };
    auto stopSchedule = [this] {
        if (m_schedule->isActive()) {
            const QSignalBlocker blocker(m_scheduleAction);
            m_scheduleAction->setChecked(false);
            m_schedule->setActive(false);
            logInfo("외부 명령으로 스케줄 운영을 끔");
        }
    };

    switch (cmd.type) {
    case CommandType::Scene: {
        const Scene* scene = findByNameOrNumber(project.scenes, cmd.target);
        if (!scene)
            return fail("장면을 찾을 수 없음: " + cmd.target);
        stopSchedule();
        startScene(QString::fromStdString(scene->id), false);
        return {};
    }
    case CommandType::Play:
        if (m_engine->isPlaying()) {
            // 타임라인이 멈춰 있으면 이어서 재생
            const TimelineStatus st = m_engine->timelineStatus();
            if (st.active && !st.running)
                m_engine->timelineResume();
            return {};
        }
        stopSchedule();
        startScene(m_controller->editingSceneId(), false);
        return {};
    case CommandType::Stop:
        stopPlayback();
        return {};
    case CommandType::Next:
    case CommandType::Prev: {
        const Region* region = findByNameOrNumber(project.regions, cmd.target);
        if (!region)
            return fail("영역을 찾을 수 없음: " + cmd.target);
        if (!m_engine->skip(region->id, cmd.type == CommandType::Next ? +1 : -1))
            return fail("재생 중인 플레이리스트 영역이 아님: " + cmd.target);
        return {};
    }
    case CommandType::Text: {
        const Region* region = findByNameOrNumber(project.regions, cmd.target);
        if (!region)
            return fail("영역을 찾을 수 없음: " + cmd.target);
        m_overrides.setTickerLines(region->id, cmd.lines);
        refreshPlaybackFromOverrides();
        return {};
    }
    case CommandType::Show:
    case CommandType::Hide: {
        const Region* region = findByNameOrNumber(project.regions, cmd.target);
        if (!region)
            return fail("영역을 찾을 수 없음: " + cmd.target);
        m_overrides.setVisible(region->id, cmd.type == CommandType::Show);
        refreshPlaybackFromOverrides();
        return {};
    }
    case CommandType::Trigger: {
        if (m_triggerDepth > 0)
            return fail("트리거 연결 명령 안에서는 trigger 명령을 쓸 수 없음");
        const TriggerMapping* m = findMapping(project.interactive, QString::fromStdString(cmd.target));
        if (!m || m->command.empty())
            return fail("연결되지 않은 트리거: " + cmd.target);
        const QString command = QString::fromStdString(m->command);
        ++m_triggerDepth;
        const QString result = executeCommand(command, source + QStringLiteral(" / 트리거 ") +
                                                           QString::fromStdString(cmd.target));
        --m_triggerDepth;
        return result;
    }
    case CommandType::Pause:
        if (!m_engine->timelinePause())
            return fail("타임라인 장면이 재생 중이 아님");
        return {};
    case CommandType::Go:
        if (!m_engine->timelineResume())
            return fail("타임라인 장면이 재생 중이 아님");
        return {};
    case CommandType::Seek:
        if (!m_engine->timelineSeek(cmd.seconds))
            return fail("타임라인 장면이 재생 중이 아님");
        return {};
    case CommandType::Reset:
        m_overrides.clear();
        refreshPlaybackFromOverrides();
        return {};
    }
    return fail("처리하지 않은 명령");
}

void MainWindow::onTrigger(const QString& trigger, const QString& source)
{
    m_interactivePanel->setLastTrigger(trigger, source);
    const TriggerMapping* m = findMapping(m_controller->project().interactive, trigger);
    if (!m || m->command.empty()) {
        logInfo("트리거 수신 (연결 없음): " + trigger.toStdString() + " [" + source.toStdString() + "]");
        return;
    }
    const QString command = QString::fromStdString(m->command);
    ++m_triggerDepth;
    executeCommand(command, source + QStringLiteral(" / 트리거 ") + trigger);
    --m_triggerDepth;
}

void MainWindow::onCanvasClicked(int x, int y)
{
    if (!m_controller->project().interactive.touchEnabled)
        return;
    // 화면에 보이는 영역 중 가장 앞(목록 뒤쪽)의 영역
    const Project p = playbackProject();
    for (auto it = p.regions.rbegin(); it != p.regions.rend(); ++it) {
        const RectI& r = it->rect;
        if (it->visible && x >= r.x && x < r.right() && y >= r.y && y < r.bottom()) {
            onTrigger(QStringLiteral("touch:") + QString::fromStdString(it->name),
                      QStringLiteral("터치 (%1, %2)").arg(x).arg(y));
            return;
        }
    }
    logInfo("터치 (" + std::to_string(x) + ", " + std::to_string(y) + "): 영역 없음");
}

// ---------------------------------------------------------------- 타임라인

void MainWindow::setupTimeline()
{
    // 소리 출력 장치: 프로젝트 설정 적용
    auto applyAudioDevice = [this] { AudioOutput::instance().setDevice(m_controller->project().audioDevice); };
    connect(m_controller, &ProjectController::playbackSettingsChanged, this, applyAudioDevice);
    connect(m_controller, &ProjectController::projectReset, this, applyAudioDevice);
    applyAudioDevice();

    connect(m_timelineEditor, &TimelineEditor::playPauseRequested, this, [this](const QString& sceneId, double from) {
        if (m_engine->isPlaying() && QString::fromStdString(m_engine->sceneId()) == sceneId) {
            const TimelineStatus st = m_engine->timelineStatus();
            if (st.running)
                m_engine->timelinePause();
            else
                m_engine->timelineResume();
            return;
        }
        playScene(sceneId);
        if (from > 0.01)
            m_engine->timelineSeek(from);
    });
    connect(m_timelineEditor, &TimelineEditor::seekRequested, this, [this](double t) { m_engine->timelineSeek(t); });
    connect(m_timelineEditor, &TimelineEditor::goRequested, this, [this] { m_engine->timelineResume(); });

    // 편집 장면이 타임라인이면 타임라인 창을 앞으로
    auto raiseIfTimeline = [this] {
        if (!m_timelineEditor->sceneId().isEmpty())
            m_timelineDock->raise();
    };
    connect(m_controller, &ProjectController::editingSceneChanged, this, raiseIfTimeline);
    connect(m_controller, &ProjectController::scenesChanged, this, raiseIfTimeline);

    // 재생 위치 표시 (초당 20번)
    m_timelineStatusTimer.setInterval(50);
    connect(&m_timelineStatusTimer, &QTimer::timeout, this, [this] {
        const QString id = m_timelineEditor->sceneId();
        const bool playingThis =
            !id.isEmpty() && m_engine->isPlaying() && QString::fromStdString(m_engine->sceneId()) == id;
        m_timelineEditor->setPlaybackStatus(playingThis ? m_engine->timelineStatus() : TimelineStatus{}, playingThis);
    });
    m_timelineStatusTimer.start();
}

} // namespace led
