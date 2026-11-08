#include "ui/timeline/TimelinePropertyPanel.h"

#include "core/ControlCommand.h"
#include "core/TimelineEdit.h"
#include "media/MediaInfo.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace led {

namespace {

QDoubleSpinBox* seconds(QWidget* parent, double max = 86400.0)
{
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(0.0, max);
    box->setDecimals(2);
    box->setSingleStep(0.1);
    box->setSuffix(QStringLiteral(" 초"));
    box->setKeyboardTracking(false);
    return box;
}

QSpinBox* percent(QWidget* parent)
{
    auto* box = new QSpinBox(parent);
    box->setRange(0, 100);
    box->setSuffix(QStringLiteral(" %"));
    box->setKeyboardTracking(false);
    return box;
}

const char kMediaFilter[] =
    "미디어 파일 (*.mp4 *.mov *.avi *.mkv *.wmv *.m4v *.webm *.mpg *.mpeg *.ts *.mxf *.gif "
    "*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.mp3 *.wav *.aac *.m4a *.flac *.ogg *.wma);;"
    "모든 파일 (*.*)";

} // namespace

TimelinePropertyPanel::TimelinePropertyPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack);

    // 0: 안내
    auto* help = new QLabel(QStringLiteral("클립·레이어·큐를 선택하면 속성이 표시됩니다.\n\n"
                                           "• 클립 끌기: 이동 (위아래로 끌면 다른 레이어)\n"
                                           "• 클립 양 끝 끌기: 길이 자르기\n"
                                           "• Ctrl+클릭 / 빈 곳 끌기: 여러 개 선택\n"
                                           "• 시간 눈금 클릭: 재생 위치 이동\n"
                                           "• Ctrl+휠: 확대/축소, Shift+휠: 가로 이동\n"
                                           "• Delete: 삭제, Space: 재생/일시정지\n"
                                           "• 파일을 끌어다 놓으면 그 위치에 추가\n"
                                           "• Alt를 누르고 끌면 자석 일시 해제"),
                            m_stack);
    help->setObjectName("hintLabel");
    help->setWordWrap(true);
    help->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_stack->addWidget(help);

    // 1: 클립
    auto* clipPage = new QWidget(m_stack);
    auto* cf = new QFormLayout(clipPage);
    cf->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_clipTitle = new QLabel(clipPage);
    m_clipTitle->setObjectName("sectionLabel");
    cf->addRow(m_clipTitle);
    auto* srcRow = new QHBoxLayout;
    m_source = new QLineEdit(clipPage);
    m_source->setReadOnly(true);
    m_browse = new QPushButton(QStringLiteral("바꾸기..."), clipPage);
    srcRow->addWidget(m_source, 1);
    srcRow->addWidget(m_browse);
    cf->addRow(QStringLiteral("파일"), srcRow);
    m_mediaInfo = new QLabel(clipPage);
    m_mediaInfo->setObjectName("hintLabel");
    cf->addRow(QString(), m_mediaInfo);
    m_start = seconds(clipPage);
    m_duration = seconds(clipPage);
    m_duration->setMinimum(kMinClipDuration);
    m_inPoint = seconds(clipPage);
    cf->addRow(QStringLiteral("시작"), m_start);
    cf->addRow(QStringLiteral("길이"), m_duration);
    cf->addRow(QStringLiteral("자르기(파일 시작점)"), m_inPoint);
    auto* fadeRow = new QHBoxLayout;
    m_fadeIn = seconds(clipPage, 3600.0);
    m_fadeOut = seconds(clipPage, 3600.0);
    fadeRow->addWidget(new QLabel(QStringLiteral("인"), clipPage));
    fadeRow->addWidget(m_fadeIn);
    fadeRow->addWidget(new QLabel(QStringLiteral("아웃"), clipPage));
    fadeRow->addWidget(m_fadeOut);
    cf->addRow(QStringLiteral("페이드"), fadeRow);
    auto* mixRow = new QHBoxLayout;
    m_opacity = percent(clipPage);
    m_volume = percent(clipPage);
    mixRow->addWidget(new QLabel(QStringLiteral("불투명도"), clipPage));
    mixRow->addWidget(m_opacity);
    mixRow->addWidget(new QLabel(QStringLiteral("소리"), clipPage));
    mixRow->addWidget(m_volume);
    cf->addRow(QString(), mixRow);
    m_loop = new QCheckBox(QStringLiteral("파일이 클립보다 짧으면 반복"), clipPage);
    cf->addRow(QString(), m_loop);
    m_stack->addWidget(clipPage);

    // 2: 레이어
    auto* layerPage = new QWidget(m_stack);
    auto* lf = new QFormLayout(layerPage);
    lf->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* layerTitle = new QLabel(QStringLiteral("레이어"), layerPage);
    layerTitle->setObjectName("sectionLabel");
    lf->addRow(layerTitle);
    m_layerName = new QLineEdit(layerPage);
    lf->addRow(QStringLiteral("이름"), m_layerName);
    m_layerRegion = new QComboBox(layerPage);
    lf->addRow(QStringLiteral("표시 영역"), m_layerRegion);
    m_layerVisible = new QCheckBox(QStringLiteral("화면에 표시"), layerPage);
    m_layerSound = new QCheckBox(QStringLiteral("소리 재생"), layerPage);
    m_layerLocked = new QCheckBox(QStringLiteral("편집 잠금"), layerPage);
    lf->addRow(QString(), m_layerVisible);
    lf->addRow(QString(), m_layerSound);
    lf->addRow(QString(), m_layerLocked);
    auto* layerHint = new QLabel(QStringLiteral("위쪽 레이어가 화면 앞에 그려집니다.\n같은 영역에 여러 레이어를 겹칠 수 있습니다."),
                                 layerPage);
    layerHint->setObjectName("hintLabel");
    layerHint->setWordWrap(true);
    lf->addRow(QString(), layerHint);
    m_stack->addWidget(layerPage);

    // 3: 큐
    auto* cuePage = new QWidget(m_stack);
    auto* qf = new QFormLayout(cuePage);
    auto* cueTitle = new QLabel(QStringLiteral("큐 지점"), cuePage);
    cueTitle->setObjectName("sectionLabel");
    qf->addRow(cueTitle);
    m_cueName = new QLineEdit(cuePage);
    m_cueTime = seconds(cuePage);
    qf->addRow(QStringLiteral("이름"), m_cueName);
    qf->addRow(QStringLiteral("시각"), m_cueTime);
    auto* cueHint = new QLabel(QStringLiteral("재생 중 이 지점에 오면 멈추고 GO(계속)를 기다립니다."), cuePage);
    cueHint->setObjectName("hintLabel");
    cueHint->setWordWrap(true);
    qf->addRow(QString(), cueHint);
    m_stack->addWidget(cuePage);

    // 연결
    for (QDoubleSpinBox* b : {m_start, m_duration, m_inPoint, m_fadeIn, m_fadeOut})
        connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TimelinePropertyPanel::commitClips);
    for (QSpinBox* b : {m_opacity, m_volume})
        connect(b, QOverload<int>::of(&QSpinBox::valueChanged), this, &TimelinePropertyPanel::commitClips);
    connect(m_loop, &QCheckBox::toggled, this, &TimelinePropertyPanel::commitClips);
    connect(m_browse, &QPushButton::clicked, this, &TimelinePropertyPanel::browseSource);
    connect(m_layerName, &QLineEdit::editingFinished, this, &TimelinePropertyPanel::commitLayer);
    connect(m_layerRegion, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TimelinePropertyPanel::commitLayer);
    for (QCheckBox* c : {m_layerVisible, m_layerSound, m_layerLocked})
        connect(c, &QCheckBox::toggled, this, &TimelinePropertyPanel::commitLayer);
    connect(m_cueName, &QLineEdit::editingFinished, this, &TimelinePropertyPanel::commitCue);
    connect(m_cueTime, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TimelinePropertyPanel::commitCue);
}

void TimelinePropertyPanel::display(const Timeline& timeline, const std::set<std::string>& clips, int layer,
                                    const std::string& cue,
                                    const std::vector<std::pair<std::string, std::string>>& regions)
{
    m_tl = timeline;
    m_clips = clips;
    m_layer = layer;
    m_cue = cue;
    m_loading = true;

    if (!clips.empty()) {
        const TimelineClip* first = clipById(timeline, *clips.begin());
        if (first) {
            const bool single = clips.size() == 1;
            m_clipTitle->setText(single ? QStringLiteral("클립")
                                        : QStringLiteral("클립 %1개 (바꾼 값이 모두에 적용)").arg(clips.size()));
            m_source->setText(single ? QFileInfo(QString::fromStdString(first->source)).fileName()
                                     : QStringLiteral("(여러 개)"));
            m_source->setToolTip(QString::fromStdString(first->source));
            m_browse->setEnabled(single);
            m_inPoint->setEnabled(single);
            m_start->setValue(first->start);
            m_duration->setValue(first->duration);
            m_inPoint->setValue(first->inPoint);
            m_fadeIn->setValue(first->fadeIn);
            m_fadeOut->setValue(first->fadeOut);
            m_opacity->setValue(qRound(first->opacity * 100.0));
            m_volume->setValue(qRound(first->volume * 100.0));
            m_loop->setChecked(first->loop);
            QString info;
            if (single) {
                const MediaInfo mi = probeMediaCached(first->source);
                if (!mi.ok)
                    info = QStringLiteral("파일을 읽을 수 없음");
                else if (mi.still)
                    info = QStringLiteral("이미지 %1×%2").arg(mi.width).arg(mi.height);
                else
                    info = QStringLiteral("%1%2 · 길이 %3")
                               .arg(mi.hasVideo ? QStringLiteral("영상 %1×%2").arg(mi.width).arg(mi.height)
                                                : QStringLiteral("소리"))
                               .arg(mi.hasVideo && mi.hasAudio ? QStringLiteral(" + 소리") : QString())
                               .arg(QString::fromStdString(formatTimeText(mi.duration)));
            }
            m_mediaInfo->setText(info);
            m_stack->setCurrentIndex(1);
        }
    } else if (!cue.empty()) {
        for (const auto& c : timeline.cues)
            if (c.id == cue) {
                m_cueName->setText(QString::fromStdString(c.name));
                m_cueTime->setValue(c.time);
            }
        m_stack->setCurrentIndex(3);
    } else if (layer >= 0 && layer < static_cast<int>(timeline.layers.size())) {
        const TimelineLayer& l = timeline.layers[static_cast<std::size_t>(layer)];
        m_layerName->setText(QString::fromStdString(l.name));
        m_layerRegion->clear();
        m_layerRegion->addItem(QStringLiteral("(영역 없음 - 소리만)"), QString());
        for (const auto& [id, name] : regions)
            m_layerRegion->addItem(QString::fromStdString(name), QString::fromStdString(id));
        m_layerRegion->setCurrentIndex(std::max(0, m_layerRegion->findData(QString::fromStdString(l.regionId))));
        m_layerVisible->setChecked(l.visible);
        m_layerSound->setChecked(!l.muted);
        m_layerLocked->setChecked(l.locked);
        m_stack->setCurrentIndex(2);
    } else {
        m_stack->setCurrentIndex(0);
    }
    m_loading = false;
}

void TimelinePropertyPanel::commitClips()
{
    if (m_loading || m_clips.empty())
        return;
    QObject* s = sender();
    Timeline tl = m_tl;
    const TimelineClip* first = clipById(m_tl, *m_clips.begin());
    if (!first)
        return;
    const double shift = m_start->value() - first->start;
    for (const std::string& id : m_clips) {
        TimelineClip* c = clipById(tl, id);
        const ClipLocation loc = findClip(tl, id);
        if (!c || tl.layers[static_cast<std::size_t>(loc.layer)].locked)
            continue;
        if (s == m_start)
            c->start = std::max(0.0, c->start + shift);
        else if (s == m_duration)
            c->duration = std::max(kMinClipDuration, m_duration->value());
        else if (s == m_inPoint)
            c->inPoint = m_inPoint->value();
        else if (s == m_fadeIn)
            c->fadeIn = m_fadeIn->value();
        else if (s == m_fadeOut)
            c->fadeOut = m_fadeOut->value();
        else if (s == m_opacity)
            c->opacity = m_opacity->value() / 100.0;
        else if (s == m_volume)
            c->volume = m_volume->value() / 100.0;
        else if (s == m_loop)
            c->loop = m_loop->isChecked();
    }
    if (tl != m_tl) {
        m_tl = tl;
        emit edited(tl);
    }
}

void TimelinePropertyPanel::browseSource()
{
    if (m_clips.size() != 1)
        return;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("파일 바꾸기"), QString(),
                                                      QString::fromUtf8(kMediaFilter));
    if (path.isEmpty())
        return;
    Timeline tl = m_tl;
    if (TimelineClip* c = clipById(tl, *m_clips.begin())) {
        c->source = path.toStdString();
        m_tl = tl;
        emit edited(tl);
    }
}

void TimelinePropertyPanel::commitLayer()
{
    if (m_loading || m_layer < 0 || m_layer >= static_cast<int>(m_tl.layers.size()))
        return;
    Timeline tl = m_tl;
    TimelineLayer& l = tl.layers[static_cast<std::size_t>(m_layer)];
    const std::string name = m_layerName->text().trimmed().toStdString();
    if (!name.empty())
        l.name = name;
    l.regionId = m_layerRegion->currentData().toString().toStdString();
    l.visible = m_layerVisible->isChecked();
    l.muted = !m_layerSound->isChecked();
    l.locked = m_layerLocked->isChecked();
    if (tl != m_tl) {
        m_tl = tl;
        emit edited(tl);
    }
}

void TimelinePropertyPanel::commitCue()
{
    if (m_loading || m_cue.empty())
        return;
    Timeline tl = m_tl;
    for (auto& c : tl.cues)
        if (c.id == m_cue) {
            const std::string name = m_cueName->text().trimmed().toStdString();
            if (!name.empty())
                c.name = name;
            c.time = m_cueTime->value();
        }
    if (tl != m_tl) {
        m_tl = tl;
        emit edited(tl);
    }
}

} // namespace led
