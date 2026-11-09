#include "ui/timeline/TimelineEditor.h"

#include "core/ControlCommand.h"
#include "core/LayerContent.h"
#include "core/TimelineEdit.h"
#include "media/MediaInfo.h"
#include "ui/ProjectController.h"
#include "ui/timeline/TimelinePropertyPanel.h"
#include "ui/timeline/TimelineView.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace led {

namespace {

const char kMediaFilter[] =
    "미디어 파일 (*.mp4 *.mov *.avi *.mkv *.wmv *.m4v *.webm *.mpg *.mpeg *.ts *.mxf *.gif "
    "*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.mp3 *.wav *.aac *.m4a *.flac *.ogg *.wma);;"
    "영상 (*.mp4 *.mov *.avi *.mkv *.wmv *.m4v *.webm *.mpg *.mpeg *.ts *.mxf *.gif);;"
    "이미지 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;"
    "소리 (*.mp3 *.wav *.aac *.m4a *.flac *.ogg *.wma);;"
    "모든 파일 (*.*)";

constexpr double kDefaultStillDuration = 5.0;

QString lengthText(const Timeline& tl)
{
    return tl.length() > 0.0 ? QString::fromStdString(formatTimeText(tl.length())) : QStringLiteral("끝없음");
}

} // namespace

TimelineEditor::TimelineEditor(ProjectController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    outer->addWidget(m_stack);

    auto* notTimeline = new QLabel(QStringLiteral("편집 중인 장면이 없습니다."), m_stack);
    notTimeline->setAlignment(Qt::AlignCenter);
    notTimeline->setObjectName("hintLabel");
    m_stack->addWidget(notTimeline);

    auto* body = new QWidget(m_stack);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // 도구 줄
    auto* bar = new QHBoxLayout;
    m_play = new QPushButton(QStringLiteral("▶ 재생"), body);
    auto* toStart = new QPushButton(QStringLiteral("⏮ 처음"), body);
    m_go = new QPushButton(QStringLiteral("GO"), body);
    m_go->setToolTip(QStringLiteral("큐에서 멈춘 상태를 이어서 재생"));
    m_time = new QLabel(QStringLiteral("0:00.0"), body);
    m_time->setMinimumWidth(170);
    QFont tf = m_time->font();
    tf.setPointSize(tf.pointSize() + 2);
    tf.setBold(true);
    m_time->setFont(tf);
    auto* addLayerBtn = new QPushButton(QStringLiteral("레이어 추가"), body);
    addLayerBtn->setToolTip(QStringLiteral("선택한 레이어의 영역 그룹 맨 아래(화면 뒤)에 추가"));
    auto* removeLayerBtn = new QPushButton(QStringLiteral("레이어 삭제"), body);
    auto* upBtn = new QPushButton(QStringLiteral("▲"), body);
    auto* downBtn = new QPushButton(QStringLiteral("▼"), body);
    upBtn->setToolTip(QStringLiteral("레이어를 같은 영역 안에서 위로 (화면 앞으로). 맨 위 레이어 = 영역 탭의 플레이리스트"));
    downBtn->setToolTip(QStringLiteral("레이어를 같은 영역 안에서 아래로 (화면 뒤로)"));
    auto* addFilesBtn = new QPushButton(QStringLiteral("파일 추가..."), body);
    addFilesBtn->setToolTip(QStringLiteral("선택한 레이어의 재생 위치에 영상/이미지/소리 추가"));
    auto* addCueBtn = new QPushButton(QStringLiteral("큐 추가"), body);
    m_endAction = new QComboBox(body);
    m_endAction->addItem(QStringLiteral("끝: 마지막 화면 유지"), int(TimelineEnd::Hold));
    m_endAction->addItem(QStringLiteral("끝: 처음부터 반복"), int(TimelineEnd::Loop));
    m_endAction->addItem(QStringLiteral("끝: 정지(검은 화면)"), int(TimelineEnd::Stop));
    m_snap = new QCheckBox(QStringLiteral("자석"), body);
    m_snap->setChecked(QSettings().value(QStringLiteral("timelineSnap"), true).toBool());
    auto* foldBtn = new QPushButton(QStringLiteral("모두 접기"), body);
    auto* unfoldBtn = new QPushButton(QStringLiteral("모두 펴기"), body);
    auto* zoomIn = new QPushButton(QStringLiteral("＋"), body);
    auto* zoomOut = new QPushButton(QStringLiteral("－"), body);
    zoomIn->setToolTip(QStringLiteral("확대 (Ctrl+휠)"));
    zoomOut->setToolTip(QStringLiteral("축소 (Ctrl+휠)"));
    for (QWidget* w : std::initializer_list<QWidget*>{m_play, toStart, m_go})
        bar->addWidget(w);
    bar->addWidget(m_time);
    bar->addSpacing(12);
    for (QWidget* w : std::initializer_list<QWidget*>{addLayerBtn, removeLayerBtn, upBtn, downBtn, addFilesBtn, addCueBtn})
        bar->addWidget(w);
    bar->addStretch(1);
    bar->addWidget(m_endAction);
    bar->addWidget(m_snap);
    bar->addWidget(foldBtn);
    bar->addWidget(unfoldBtn);
    bar->addWidget(zoomOut);
    bar->addWidget(zoomIn);
    layout->addLayout(bar);

    auto* split = new QSplitter(Qt::Horizontal, body);
    m_view = new TimelineView(split);
    m_panel = new TimelinePropertyPanel(split);
    m_panel->setMinimumWidth(300);
    split->addWidget(m_view);
    split->addWidget(m_panel);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    layout->addWidget(split, 1);
    m_stack->addWidget(body);

    m_view->setSnapEnabled(m_snap->isChecked());

    connect(m_play, &QPushButton::clicked, this, &TimelineEditor::onPlayToggle);
    connect(toStart, &QPushButton::clicked, this, [this] { onSeek(0.0); });
    connect(m_go, &QPushButton::clicked, this, &TimelineEditor::goRequested);
    connect(addLayerBtn, &QPushButton::clicked, this, &TimelineEditor::addLayer);
    connect(removeLayerBtn, &QPushButton::clicked, this, &TimelineEditor::removeLayer);
    connect(upBtn, &QPushButton::clicked, this, [this] { moveLayer(-1); });
    connect(downBtn, &QPushButton::clicked, this, [this] { moveLayer(+1); });
    connect(addFilesBtn, &QPushButton::clicked, this, [this] { addFiles(m_view->selectedLayer(), m_view->playhead()); });
    connect(addCueBtn, &QPushButton::clicked, this, [this] { addCue(m_view->playhead()); });
    connect(m_endAction, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        Timeline tl = m_view->timeline();
        tl.endAction = static_cast<TimelineEnd>(m_endAction->currentData().toInt());
        applyEdit(tl);
    });
    connect(m_snap, &QCheckBox::toggled, this, [this](bool on) {
        m_view->setSnapEnabled(on);
        QSettings().setValue(QStringLiteral("timelineSnap"), on);
    });
    connect(foldBtn, &QPushButton::clicked, this, [this] { m_view->setAllCollapsed(true); });
    connect(unfoldBtn, &QPushButton::clicked, this, [this] { m_view->setAllCollapsed(false); });
    connect(zoomIn, &QPushButton::clicked, this, [this] { m_view->zoomBy(1.5); });
    connect(zoomOut, &QPushButton::clicked, this, [this] { m_view->zoomBy(1.0 / 1.5); });

    connect(m_view, &TimelineView::timelineEdited, this, &TimelineEditor::applyEdit);
    connect(m_view, &TimelineView::selectionChanged, this, &TimelineEditor::updatePanel);
    connect(m_view, &TimelineView::seekRequested, this, &TimelineEditor::onSeek);
    connect(m_view, &TimelineView::playToggleRequested, this, &TimelineEditor::onPlayToggle);
    connect(m_view, &TimelineView::addFilesRequested, this, &TimelineEditor::addFiles);
    connect(m_view, &TimelineView::addCueRequested, this, &TimelineEditor::addCue);
    connect(m_view, &TimelineView::filesDropped, this, &TimelineEditor::insertFiles);
    connect(m_view, &TimelineView::addLayerRequested, this, &TimelineEditor::addLayerTo);
    connect(m_panel, &TimelinePropertyPanel::edited, this, &TimelineEditor::applyEdit);

    connect(controller, &ProjectController::projectReset, this, &TimelineEditor::reload);
    connect(controller, &ProjectController::editingSceneChanged, this, &TimelineEditor::reload);
    connect(controller, &ProjectController::scenesChanged, this, &TimelineEditor::reload);
    connect(controller, &ProjectController::regionsChanged, this, &TimelineEditor::reload);
    connect(controller, &ProjectController::timelineChanged, this, [this](const QString& id) {
        if (id == sceneId())
            reload();
    });
    reload();
}

QString TimelineEditor::sceneId() const
{
    const Scene* scene = m_controller->editingScene();
    return scene ? QString::fromStdString(scene->id) : QString();
}

std::vector<std::pair<std::string, std::string>> TimelineEditor::regionList() const
{
    std::vector<std::pair<std::string, std::string>> list;
    for (const Region& r : m_controller->project().regions)
        list.emplace_back(r.id, r.name);
    return list;
}

void TimelineEditor::reload()
{
    const Scene* scene = m_controller->editingScene();
    if (!scene) {
        m_stack->setCurrentIndex(0);
        return;
    }
    m_stack->setCurrentIndex(1);
    if (m_view->isDragging())
        return; // 끄는 중에는 화면 유지 (놓을 때 반영됨)
    m_view->setTimeline(scene->timeline, regionList());
    {
        const QSignalBlocker blocker(m_endAction);
        m_endAction->setCurrentIndex(std::max(0, m_endAction->findData(int(scene->timeline.endAction))));
    }
    updatePanel();
}

void TimelineEditor::applyEdit(const Timeline& timeline)
{
    const QString id = sceneId();
    if (!id.isEmpty())
        m_controller->setTimeline(id, timeline);
}

void TimelineEditor::updatePanel()
{
    m_panel->display(m_view->timeline(), m_view->selectedClips(), m_view->selectedLayer(), m_view->selectedCue(),
                     regionList());
}

void TimelineEditor::addLayer()
{
    // 선택한 레이어의 영역 그룹, 선택이 없으면 '영역' 탭에서 선택한 영역
    const int selected = m_view->selectedLayer();
    std::string regionId = selected >= 0 ? m_view->groupRegionOf(selected)
                                         : m_controller->selectedRegionId().toStdString();
    if (selected < 0 && !m_controller->project().findRegion(regionId))
        regionId = m_controller->project().regions.empty() ? std::string() : m_controller->project().regions.front().id;
    addLayerTo(QString::fromStdString(regionId));
}

void TimelineEditor::addLayerTo(const QString& regionIdText)
{
    Timeline tl = m_view->timeline();
    const std::string regionId = regionIdText.toStdString();
    const Region* region = m_controller->project().findRegion(regionId);
    // 같은 그룹의 레이어 수
    int count = 0;
    int insertAt = static_cast<int>(tl.layers.size());
    for (int i = 0; i < static_cast<int>(tl.layers.size()); ++i) {
        const TimelineLayer& l = tl.layers[static_cast<std::size_t>(i)];
        const bool same = region ? l.regionId == regionId : !m_controller->project().findRegion(l.regionId);
        if (same) {
            ++count;
            insertAt = i + 1; // 그룹 맨 아래(화면 뒤) 다음
        }
    }
    TimelineLayer layer;
    layer.id = generateId();
    layer.regionId = region ? regionId : std::string();
    layer.name = region ? (count == 0 ? region->name : region->name + " " + std::to_string(count + 1))
                        : "소리 " + std::to_string(count + 1);
    tl.layers.insert(tl.layers.begin() + insertAt, layer);
    applyEdit(tl);
    m_view->selectLayer(insertAt);
}

void TimelineEditor::removeLayer()
{
    const int index = m_view->selectedLayer();
    Timeline tl = m_view->timeline();
    if (index < 0 || index >= static_cast<int>(tl.layers.size()))
        return;
    const TimelineLayer& layer = tl.layers[static_cast<std::size_t>(index)];
    if (!layer.clips.empty() &&
        QMessageBox::question(this, QStringLiteral("레이어 삭제"),
                              QStringLiteral("'%1' 레이어와 클립 %2개를 삭제하시겠습니까?")
                                  .arg(QString::fromStdString(layer.name))
                                  .arg(layer.clips.size())) != QMessageBox::Yes)
        return;
    tl.layers.erase(tl.layers.begin() + index);
    applyEdit(tl);
    m_view->selectLayer(tl.layers.empty() ? -1 : std::min(index, static_cast<int>(tl.layers.size()) - 1));
}

void TimelineEditor::moveLayer(int delta)
{
    // 같은 영역 그룹 안에서만 순서 변경
    const int index = m_view->selectedLayer();
    Timeline tl = m_view->timeline();
    if (index < 0 || index >= static_cast<int>(tl.layers.size()))
        return;
    const std::string group = m_view->groupRegionOf(index);
    int target = index + delta;
    while (target >= 0 && target < static_cast<int>(tl.layers.size()) && m_view->groupRegionOf(target) != group)
        target += delta;
    if (target < 0 || target >= static_cast<int>(tl.layers.size()))
        return;
    std::swap(tl.layers[static_cast<std::size_t>(index)], tl.layers[static_cast<std::size_t>(target)]);
    applyEdit(tl);
    m_view->selectLayer(target);
}

void TimelineEditor::addFiles(int layer, double time)
{
    QSettings settings;
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("타임라인에 추가"), settings.value(QStringLiteral("lastMediaFolder")).toString(),
        QString::fromUtf8(kMediaFilter));
    if (files.isEmpty())
        return;
    settings.setValue(QStringLiteral("lastMediaFolder"), QFileInfo(files.first()).path());
    insertFiles(layer, time, files);
}

void TimelineEditor::insertFiles(int layer, double time, const QStringList& paths, const QString& regionId)
{
    if (layer < 0 || layer >= static_cast<int>(m_view->timeline().layers.size())) {
        // 레이어가 없음: 지정 영역(없으면 선택한 영역)에 새 레이어
        QString target = regionId;
        if (target.isEmpty() && !m_controller->project().findRegion(m_controller->selectedRegionId().toStdString()) &&
            !m_controller->project().regions.empty())
            target = QString::fromStdString(m_controller->project().regions.front().id);
        else if (target.isEmpty())
            target = m_controller->selectedRegionId();
        addLayerTo(target);
        layer = m_view->selectedLayer();
        if (layer < 0)
            return;
    }
    Timeline tl = m_view->timeline();
    if (tl.layers[static_cast<std::size_t>(layer)].locked) {
        QMessageBox::information(this, QStringLiteral("타임라인"), QStringLiteral("잠긴 레이어에는 추가할 수 없습니다."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList failed;
    std::set<std::string> added;
    double t = std::max(0.0, time);
    for (const QString& path : paths) {
        const MediaInfo info = probeMediaCached(path.toStdString());
        if (!info.ok) {
            failed << QFileInfo(path).fileName();
            continue;
        }
        TimelineClip clip;
        clip.id = generateId();
        clip.source = path.toStdString();
        clip.start = t;
        clip.duration = info.still || info.duration <= 0.0 ? kDefaultStillDuration : info.duration;
        addClip(tl, layer, clip);
        added.insert(clip.id);
        t += clip.duration; // 여러 파일은 이어서 배치
    }
    QApplication::restoreOverrideCursor();
    if (!added.empty())
        applyEdit(tl);
    if (!failed.isEmpty())
        QMessageBox::warning(this, QStringLiteral("타임라인"),
                             QStringLiteral("읽을 수 없는 파일:\n%1").arg(failed.join(QLatin1Char('\n'))));
}

void TimelineEditor::addCue(double time)
{
    Timeline tl = m_view->timeline();
    TimelineCue cue;
    cue.id = generateId();
    cue.name = "큐 " + std::to_string(tl.cues.size() + 1);
    cue.time = std::max(0.0, time);
    tl.cues.push_back(cue);
    std::sort(tl.cues.begin(), tl.cues.end(), [](const TimelineCue& a, const TimelineCue& b) { return a.time < b.time; });
    applyEdit(tl);
    m_view->selectCue(cue.id);
}

void TimelineEditor::onSeek(double seconds)
{
    m_view->setPlayhead(seconds);
    if (m_playingThis)
        emit seekRequested(seconds);
    else
        m_time->setText(QString::fromStdString(formatTimeText(seconds)) + QStringLiteral(" / ") +
                        lengthText(m_view->timeline()));
}

void TimelineEditor::onPlayToggle()
{
    const QString id = sceneId();
    if (id.isEmpty())
        return;
    emit playPauseRequested(id, m_view->playhead());
}

void TimelineEditor::setPlaybackStatus(const TimelineStatus& status, bool playingThisScene)
{
    m_playingThis = playingThisScene && status.active;
    m_running = m_playingThis && status.running;
    if (m_playingThis && !m_view->isDragging())
        m_view->setPlayhead(status.time);
    QString text = QString::fromStdString(formatTimeText(m_playingThis ? status.time : m_view->playhead())) +
                   QStringLiteral(" / ") + lengthText(m_view->timeline());
    if (m_playingThis && !status.waitingCue.empty())
        text += QStringLiteral("  ⏸ 큐 '%1' 대기").arg(QString::fromStdString(status.waitingCue));
    else if (m_playingThis && status.finished)
        text += QStringLiteral("  ■ 끝");
    m_time->setText(text);
    m_play->setText(m_running ? QStringLiteral("⏸ 일시정지") : QStringLiteral("▶ 재생"));
    m_go->setEnabled(m_playingThis && !status.running);
}

} // namespace led
