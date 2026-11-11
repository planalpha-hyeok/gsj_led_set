#include "ui/operator/OperatorPanel.h"

#include "core/ControlCommand.h"
#include "ui/Palette.h"
#include "ui/ProjectController.h"
#include "ui/operator/CueProgressBar.h"
#include "ui/operator/OperatorRegionControls.h"
#include "ui/operator/OperatorSceneGrid.h"
#include "ui/operator/ProgramMonitor.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace led {

namespace {

QString operatorStyleSheet()
{
    using namespace palette;
    QString s = QStringLiteral(R"(
#operatorPanel { background: #000000; }
#opHeader { background: @panel; border-bottom: 1px solid @border; }
QLabel#onAir { font-weight: 900; letter-spacing: 1px; padding: 0 18px; color: #000000; background: @dim; }
QLabel#onAir[live="true"] { background: @live; }
QLabel#opScene { font-size: 18px; font-weight: bold; color: @text; }
QLabel#opClock { font-family: @mono; font-size: 24px; font-weight: bold; color: @text; padding: 0 16px; }
QLabel#opStatus { color: @textdim; padding: 0 12px; }
QLabel#opSection { color: @textdim; font-weight: bold; letter-spacing: 1px; }
QLabel#opNextCue { background: #0d0d10; border: 2px solid @select; color: @text; font-size: 16px; padding: 12px; }
QLabel#opCueWait { color: @text; }
QLabel#opCueWait[waiting="true"] { color: @live; font-weight: bold; }
QFrame#opTransport { background: #0d0d10; border: 1px solid @border; border-radius: 8px; }
QPushButton#transportButton { min-width: 56px; min-height: 52px; font-size: 18px; }
QPushButton#goButton { min-width: 180px; min-height: 52px; font-size: 22px; font-weight: 900; color: #000000;
                       background: @select; border: none; border-radius: 8px; }
QPushButton#goButton:disabled { background: @border2; color: @textoff; }
QPushButton#emergencyButton { background: @dangerbg; color: @danger; border: 1px solid #5a2a2a; font-weight: bold; padding: 8px 16px; }
QPushButton#emergencyButton:hover { background: #3a1414; }
QPushButton#editButton { padding: 8px 16px; }
QPushButton#sceneButton { min-height: 72px; font-size: 15px; font-weight: bold; text-align: left; padding: 8px 12px;
                          background: #121216; border: 1px solid #3a3a40; border-radius: 8px; color: @text; }
QPushButton#sceneButton:hover { background: #1a1a20; }
QPushButton#sceneButton[live="true"] { border: 3px solid @live; background: #1f1608; }
QListWidget#opCueList { background: #0a0a0c; border: none; }
QListWidget#opCueList::item { padding: 9px 10px; margin: 2px 0; border-radius: 6px; background: #16161a; border-left: none; }
QWidget#regionRow { background: #16161a; border-radius: 6px; }
QLabel#regionName { color: @text; font-weight: bold; }
QPushButton#visibleToggle { min-width: 56px; }
QPushButton#visibleToggle:checked { background: @text; color: #000000; border-color: @text; }
QPushButton#applyButton { background: @select; color: #000000; border: none; font-weight: bold; }
QLabel#opMessage { color: @textdim; }
QLabel#opMessage[error="true"] { color: @danger; }
)");
    const struct {
        const char* key;
        QColor color;
    } colors[] = {{"@panel", kPanel},  {"@border2", kBorderStrong}, {"@border", kBorder},     {"@textdim", kTextDim},
                  {"@textoff", kTextDisabled}, {"@text", kText},    {"@live", kLive},         {"@select", kSelect},
                  {"@dangerbg", kDangerBg},    {"@danger", kDanger}, {"@dim", kTextDim}};
    for (const auto& c : colors)
        s.replace(QLatin1String(c.key), css(c.color));
    s.replace(QLatin1String("@mono"), monoFontFamily());
    return s;
}

void setFlag(QWidget* w, const char* name, bool value)
{
    if (w->property(name).toBool() == value)
        return;
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QLabel* section(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setObjectName("opSection");
    return l;
}

} // namespace

OperatorPanel::OperatorPanel(ProjectController* controller, SceneProvider* program, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    setObjectName("operatorPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(operatorStyleSheet());

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(buildHeader());

    auto* body = new QHBoxLayout;
    body->setContentsMargins(18, 16, 18, 16);
    body->setSpacing(18);

    // 가운데
    auto* center = new QVBoxLayout;
    center->setSpacing(14);
    auto* monitors = new QHBoxLayout;
    monitors->setSpacing(16);
    auto* programCol = new QVBoxLayout;
    programCol->setSpacing(6);
    m_programTitle = section(QStringLiteral("송출 (PROGRAM)"), this);
    programCol->addWidget(m_programTitle);
    m_monitor = new ProgramMonitor(this);
    m_monitor->setSource(program);
    programCol->addWidget(m_monitor, 1);
    monitors->addLayout(programCol, 3);
    auto* nextCol = new QVBoxLayout;
    nextCol->setSpacing(6);
    nextCol->addWidget(section(QStringLiteral("다음 큐 (NEXT)"), this));
    m_nextCue = new QLabel(this);
    m_nextCue->setObjectName("opNextCue");
    m_nextCue->setAlignment(Qt::AlignCenter);
    m_nextCue->setWordWrap(true);
    m_nextCue->setMinimumWidth(200);
    nextCol->addWidget(m_nextCue, 1);
    monitors->addLayout(nextCol, 1);
    center->addLayout(monitors, 1);
    center->addWidget(buildTransport());
    center->addWidget(section(QStringLiteral("장면 (누르면 바로 전환 · Ctrl+숫자)"), this));
    m_scenes = new OperatorSceneGrid(controller, this);
    connect(m_scenes, &OperatorSceneGrid::playSceneRequested, this, &OperatorPanel::playSceneRequested);
    center->addWidget(m_scenes);
    body->addLayout(center, 1);

    // 오른쪽
    auto* side = new QWidget(this);
    side->setFixedWidth(360);
    auto* sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(10);
    sideLayout->addWidget(section(QStringLiteral("큐 목록"), side));
    m_cueList = new QListWidget(side);
    m_cueList->setObjectName("opCueList");
    m_cueList->setSelectionMode(QAbstractItemView::NoSelection);
    m_cueList->setFocusPolicy(Qt::NoFocus);
    m_cueList->setMinimumHeight(140);
    sideLayout->addWidget(m_cueList, 1);
    sideLayout->addWidget(section(QStringLiteral("영역 빠른 제어"), side));
    auto* scroll = new QScrollArea(side);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    m_regions = new OperatorRegionControls(controller, scroll);
    scroll->setWidget(m_regions);
    connect(m_regions, &OperatorRegionControls::commandRequested, this, &OperatorPanel::commandRequested);
    sideLayout->addWidget(scroll, 2);
    m_message = new QLabel(side);
    m_message->setObjectName("opMessage");
    m_message->setWordWrap(true);
    sideLayout->addWidget(m_message);
    body->addWidget(side);

    outer->addLayout(body, 1);

    connect(controller, &ProjectController::canvasChanged, this, [this] {
        m_monitor->setCanvasSize(m_controller->project().canvas.width, m_controller->project().canvas.height);
    });
    connect(controller, &ProjectController::projectReset, this, [this] {
        m_monitor->setCanvasSize(m_controller->project().canvas.width, m_controller->project().canvas.height);
        m_cueSceneId.clear();
        refreshCueList();
    });
    connect(controller, &ProjectController::timelineChanged, this, [this](const QString& id) {
        if (id == m_cueSceneId)
            refreshCueList();
    });
    connect(controller, &ProjectController::editingSceneChanged, this, [this] {
        if (!m_state.playing)
            setState(m_state);
    });
    m_monitor->setCanvasSize(controller->project().canvas.width, controller->project().canvas.height);
    setState(OperatorState{});
}

QWidget* OperatorPanel::buildHeader()
{
    auto* header = new QWidget(this);
    header->setObjectName("opHeader");
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setMinimumHeight(56);
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(0, 0, 12, 0);
    h->setSpacing(0);
    m_onAir = new QLabel(QStringLiteral("● 대기"), header);
    m_onAir->setObjectName("onAir");
    m_onAir->setAlignment(Qt::AlignCenter);
    m_onAir->setMinimumWidth(120);
    m_onAir->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    h->addWidget(m_onAir);
    h->addSpacing(18);
    auto* sceneLabel = new QLabel(QStringLiteral("현재 장면"), header);
    sceneLabel->setObjectName("opStatus");
    h->addWidget(sceneLabel);
    m_sceneName = new QLabel(header);
    m_sceneName->setObjectName("opScene");
    h->addWidget(m_sceneName);
    m_clock = new QLabel(QStringLiteral("0:00.0"), header);
    m_clock->setObjectName("opClock");
    h->addWidget(m_clock);
    h->addStretch(1);
    m_status = new QLabel(header);
    m_status->setObjectName("opStatus");
    h->addWidget(m_status);
    auto* edit = new QPushButton(QStringLiteral("편집 화면 (F9)"), header);
    edit->setObjectName("editButton");
    connect(edit, &QPushButton::clicked, this, &OperatorPanel::editModeRequested);
    h->addWidget(edit);
    h->addSpacing(8);
    auto* emergency = new QPushButton(QStringLiteral("비상 정지"), header);
    emergency->setObjectName("emergencyButton");
    emergency->setToolTip(QStringLiteral("모든 출력과 테스트 패턴 끄기 (어느 화면에서나 Ctrl+Alt+F12)"));
    connect(emergency, &QPushButton::clicked, this, &OperatorPanel::emergencyRequested);
    h->addWidget(emergency);
    return header;
}

QWidget* OperatorPanel::buildTransport()
{
    auto* frame = new QFrame(this);
    frame->setObjectName("opTransport");
    auto* h = new QHBoxLayout(frame);
    h->setContentsMargins(14, 12, 14, 12);
    h->setSpacing(12);
    m_restart = new QPushButton(QStringLiteral("⏮"), frame);
    m_restart->setObjectName("transportButton");
    m_restart->setToolTip(QStringLiteral("처음으로"));
    m_restart->setAccessibleName(QStringLiteral("처음으로"));
    connect(m_restart, &QPushButton::clicked, this, &OperatorPanel::restartRequested);
    h->addWidget(m_restart);
    m_pause = new QPushButton(QStringLiteral("⏸"), frame);
    m_pause->setObjectName("transportButton");
    m_pause->setAccessibleName(QStringLiteral("일시정지 / 재생"));
    connect(m_pause, &QPushButton::clicked, this, &OperatorPanel::pauseToggleRequested);
    h->addWidget(m_pause);
    m_go = new QPushButton(QStringLiteral("GO ▶"), frame);
    m_go->setObjectName("goButton");
    m_go->setToolTip(QStringLiteral("큐에서 멈춘 상태 / 일시정지에서 이어서 재생"));
    connect(m_go, &QPushButton::clicked, this, &OperatorPanel::goRequested);
    h->addWidget(m_go);
    m_cueWait = new QLabel(frame);
    m_cueWait->setObjectName("opCueWait");
    m_cueWait->setMinimumWidth(180);
    m_cueWait->setWordWrap(true);
    h->addWidget(m_cueWait, 1);
    m_progress = new CueProgressBar(frame);
    m_progress->setMinimumWidth(220);
    h->addWidget(m_progress, 2);
    return frame;
}

void OperatorPanel::setActive(bool active)
{
    m_monitor->setActive(active);
}

void OperatorPanel::showMessage(const QString& text, bool error)
{
    m_message->setText(text);
    setFlag(m_message, "error", error);
}

void OperatorPanel::setState(const OperatorState& state)
{
    m_state = state;
    const Project& project = m_controller->project();
    // 표시할 장면: 재생 중이면 그 장면, 아니면 편집 중인 장면
    const QString sceneId = state.playing ? state.sceneId : m_controller->editingSceneId();
    const Scene* scene = project.findScene(sceneId.toStdString());

    const bool live = state.playing && !state.timeline.finished;
    setFlag(m_onAir, "live", live);
    m_onAir->setText(live ? QStringLiteral("● ON AIR") : QStringLiteral("● 대기"));
    m_sceneName->setText(scene ? QString::fromStdString(scene->name) : QStringLiteral("-"));
    m_clock->setText(QString::fromStdString(formatTimeText(state.playing ? state.timeline.time : 0.0)));

    QString status = state.outputsRunning > 0
                         ? QStringLiteral("출력 %1/%2").arg(state.outputsRunning).arg(state.outputsTotal)
                         : QStringLiteral("출력 꺼짐");
    if (state.testPattern)
        status += QStringLiteral(" (테스트 패턴)");
    status += state.scheduleActive ? QStringLiteral("   ·   스케줄 운영: %1").arg(state.scheduleText)
                                   : QStringLiteral("   ·   스케줄 꺼짐");
    m_status->setText(status);

    m_monitor->setFrameColor(live ? palette::kLive : palette::kBorderStrong, live ? 3 : 1);
    m_scenes->setLiveScene(state.playing ? state.sceneId : QString());
    m_regions->setSceneId(sceneId);

    if (sceneId != m_cueSceneId) {
        m_cueSceneId = sceneId;
        refreshCueList();
    }
    const double t = state.playing ? state.timeline.time : 0.0;
    updateCueStatus(t);

    // 재생 제어
    const bool running = state.playing && state.timeline.running;
    m_pause->setText(running ? QStringLiteral("⏸") : QStringLiteral("▶"));
    m_pause->setToolTip(running ? QStringLiteral("일시정지") : QStringLiteral("재생 (정지 중이면 이 장면을 처음부터)"));
    m_restart->setEnabled(state.playing);
    m_go->setEnabled(state.playing && !running);
    const bool waiting = state.playing && !state.timeline.waitingCue.empty();
    setFlag(m_cueWait, "waiting", waiting);
    if (waiting)
        m_cueWait->setText(QStringLiteral("큐 '%1'에서 대기 중\nGO로 계속").arg(QString::fromStdString(state.timeline.waitingCue)));
    else if (state.playing && state.timeline.finished)
        m_cueWait->setText(QStringLiteral("장면 끝 (정지)"));
    else if (state.playing && !running)
        m_cueWait->setText(QStringLiteral("일시정지"));
    else if (state.playing)
        m_cueWait->setText(QStringLiteral("재생 중"));
    else
        m_cueWait->setText(QStringLiteral("정지"));
}

void OperatorPanel::refreshCueList()
{
    m_cueList->clear();
    m_cueTimes.clear();
    const Scene* scene = m_controller->project().findScene(m_cueSceneId.toStdString());
    if (!scene || scene->timeline.cues.empty()) {
        auto* item = new QListWidgetItem(QStringLiteral("이 장면에 큐가 없습니다"), m_cueList);
        item->setForeground(palette::kTextDim);
        return;
    }
    std::vector<TimelineCue> cues = scene->timeline.cues;
    std::sort(cues.begin(), cues.end(), [](const TimelineCue& a, const TimelineCue& b) { return a.time < b.time; });
    for (const TimelineCue& cue : cues) {
        m_cueTimes.push_back(cue.time);
        auto* item = new QListWidgetItem(m_cueList);
        item->setData(Qt::UserRole, QString::fromStdString(cue.name.empty() ? "큐" : cue.name));
    }
}

void OperatorPanel::updateCueStatus(double t)
{
    double nextCue = -1.0;
    for (double c : m_cueTimes)
        if (c > t + 1e-3) {
            nextCue = c;
            break;
        }
    const std::string& waiting = m_state.timeline.waitingCue;
    for (int i = 0; i < static_cast<int>(m_cueTimes.size()) && i < m_cueList->count(); ++i) {
        QListWidgetItem* item = m_cueList->item(i);
        const double c = m_cueTimes[static_cast<std::size_t>(i)];
        const QString name = item->data(Qt::UserRole).toString();
        QString state;
        QColor color = palette::kText;
        const bool isWaiting = m_state.playing && !waiting.empty() && std::fabs(c - t) < 1e-3;
        if (isWaiting) {
            state = QStringLiteral("대기 중");
            color = palette::kLive;
        } else if (m_state.playing && c <= t) {
            state = QStringLiteral("지남");
            color = palette::kTextDim;
        } else if (c == nextCue) {
            state = QStringLiteral("다음");
            color = palette::kSelect;
        }
        item->setText(QStringLiteral("%1   %2%3")
                          .arg(QString::fromStdString(formatTimeText(c)), name,
                               state.isEmpty() ? QString() : QStringLiteral("   · ") + state));
        item->setForeground(color);
    }

    if (nextCue >= 0.0) {
        auto it = std::find(m_cueTimes.begin(), m_cueTimes.end(), nextCue);
        const QString name = m_cueList->item(static_cast<int>(it - m_cueTimes.begin()))->data(Qt::UserRole).toString();
        m_nextCue->setText(QStringLiteral("%1\n%2").arg(name, QString::fromStdString(formatTimeText(nextCue))));
    } else {
        m_nextCue->setText(m_cueTimes.empty() ? QStringLiteral("큐 없음") : QStringLiteral("남은 큐 없음"));
    }

    const Scene* scene = m_controller->project().findScene(m_cueSceneId.toStdString());
    const double length = m_state.playing ? m_state.timeline.length : (scene ? scene->timeline.length() : 0.0);
    m_progress->setProgress(t, length, m_cueTimes, nextCue);
}

} // namespace led
