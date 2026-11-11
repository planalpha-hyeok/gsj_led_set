#include "ui/timeline/TimelineView.h"

#include "core/ControlCommand.h"
#include "core/LayerContent.h"
#include "core/TimelineEdit.h"
#include "media/MediaInfo.h"
#include "ui/Palette.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace led {

namespace {

constexpr int kHeaderWidth = 210;
constexpr int kRulerHeight = 28;
constexpr int kRowHeight = 54;
constexpr int kGroupHeight = 26;
const char kSoundGroup[] = "\x01sound";
constexpr int kEdgeGrab = 6;
constexpr double kMinPxPerSec = 2.0;
constexpr double kMaxPxPerSec = 800.0;

const QColor kBg = palette::kBackground;
const QColor kHeaderBg = palette::kPanel;
const QColor kRowA = palette::kBackground;
const QColor kRowB(15, 17, 21);
const QColor kGrid = palette::kBorder;
const QColor kText = palette::kText;
const QColor kDim = palette::kTextDim;
const QColor kPlayhead = palette::kLive;
const QColor kCue = palette::kSelect;
const QColor kSnap(255, 64, 200);
const QColor kGroupBg = palette::kRaised;

QColor clipColor(const TimelineClip& clip)
{
    if (clip.kind == ClipKind::Ticker)
        return QColor(140, 80, 170);      // 자막
    if (clip.kind == ClipKind::Live)
        return QColor(40, 140, 150);      // 라이브
    const MediaInfo info = probeMediaCached(clip.source);
    if (!info.ok)
        return QColor(120, 60, 60);
    if (info.still)
        return QColor(60, 140, 90);       // 이미지
    if (!info.hasVideo && info.hasAudio)
        return QColor(190, 120, 50);      // 소리
    return QColor(60, 110, 190);          // 영상
}

double niceStep(double minSeconds)
{
    static const double steps[] = {0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600};
    for (double s : steps)
        if (s >= minSeconds)
            return s;
    return 3600;
}

QString clipLabel(const TimelineClip& clip)
{
    switch (clip.kind) {
    case ClipKind::Ticker: {
        const TickerConfig& t = clip.ticker;
        if (!t.textFile.empty())
            return QStringLiteral("자막: ") + QFileInfo(QString::fromStdString(t.textFile)).fileName();
        return QStringLiteral("자막: ") + (t.lines.empty() ? QString() : QString::fromStdString(t.lines.front()));
    }
    case ClipKind::Live:
        return QStringLiteral("라이브: ") + QString::fromStdString(clip.source);
    case ClipKind::File:
        break;
    }
    return QFileInfo(QString::fromStdString(clip.source)).fileName();
}

QString shortTime(double s)
{
    QString t = QString::fromStdString(formatTimeText(s));
    if (t.endsWith(QLatin1String(".0")))
        t.chop(2);
    return t;
}

} // namespace

TimelineView::TimelineView(QWidget* parent)
    : QWidget(parent)
    , m_hbar(new QScrollBar(Qt::Horizontal, this))
    , m_vbar(new QScrollBar(Qt::Vertical, this))
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setMinimumHeight(kRulerHeight + kRowHeight * 2 + 20);
    connect(m_hbar, &QScrollBar::valueChanged, this, [this](int v) {
        m_scrollSec = v / 10.0;
        update();
    });
    connect(m_vbar, &QScrollBar::valueChanged, this, [this](int v) {
        m_scrollY = v;
        update();
    });
}

void TimelineView::setTimeline(const Timeline& timeline,
                               const std::vector<std::pair<std::string, std::string>>& regions)
{
    m_tl = timeline;
    m_regions = regions;
    // 없어진 클립/큐/레이어 선택 해제
    for (auto it = m_selectedClips.begin(); it != m_selectedClips.end();)
        it = clipById(m_tl, *it) ? std::next(it) : m_selectedClips.erase(it);
    if (m_selectedLayer >= static_cast<int>(m_tl.layers.size()))
        m_selectedLayer = m_tl.layers.empty() ? -1 : 0;
    if (!m_selectedCue.empty() &&
        std::none_of(m_tl.cues.begin(), m_tl.cues.end(), [&](const TimelineCue& c) { return c.id == m_selectedCue; }))
        m_selectedCue.clear();
    rebuildRows();
    updateScrollBars();
    update();
}

std::string TimelineView::groupKeyOf(const TimelineLayer& layer) const
{
    for (const auto& r : m_regions)
        if (r.first == layer.regionId)
            return r.first;
    return kSoundGroup;
}

std::string TimelineView::groupRegionOf(int layer) const
{
    if (layer < 0 || layer >= static_cast<int>(m_tl.layers.size()))
        return {};
    const std::string key = groupKeyOf(m_tl.layers[static_cast<std::size_t>(layer)]);
    return key == kSoundGroup ? std::string() : key;
}

std::vector<int> TimelineView::layersInGroup(const std::string& key) const
{
    std::vector<int> out;
    for (int i = 0; i < static_cast<int>(m_tl.layers.size()); ++i)
        if (groupKeyOf(m_tl.layers[static_cast<std::size_t>(i)]) == key)
            out.push_back(i);
    return out;
}

void TimelineView::rebuildRows()
{
    m_rows.clear();
    m_layerRow.assign(m_tl.layers.size(), -1);
    int y = 0;
    auto addGroup = [&](const std::string& key, const QString& title) {
        const std::vector<int> layers = layersInGroup(key);
        if (key == kSoundGroup && layers.empty())
            return; // 소리 그룹은 레이어가 있을 때만
        Row g;
        g.group = true;
        g.groupKey = key;
        g.title = title;
        g.top = y;
        g.height = kGroupHeight;
        m_rows.push_back(g);
        y += kGroupHeight;
        if (m_collapsed.count(key))
            return;
        for (int l : layers) {
            Row r;
            r.groupKey = key;
            r.layer = l;
            r.top = y;
            r.height = kRowHeight;
            m_layerRow[static_cast<std::size_t>(l)] = static_cast<int>(m_rows.size());
            m_rows.push_back(r);
            y += kRowHeight;
        }
    };
    for (const auto& r : m_regions)
        addGroup(r.first, QString::fromStdString(r.second));
    addGroup(kSoundGroup, QStringLiteral("소리 / 영역 없음"));
    m_contentHeight = y;
}

void TimelineView::setAllCollapsed(bool collapsed)
{
    m_collapsed.clear();
    if (collapsed) {
        for (const auto& r : m_regions)
            m_collapsed.insert(r.first);
        m_collapsed.insert(kSoundGroup);
    }
    rebuildRows();
    updateScrollBars();
    update();
}

void TimelineView::setPlayhead(double seconds)
{
    if (std::fabs(seconds - m_playhead) < 1e-4)
        return;
    m_playhead = std::max(0.0, seconds);
    // 재생 위치가 화면 오른쪽 밖으로 나가면 따라감
    const double visible = (trackArea().width()) / m_pxPerSec;
    if (m_drag == Drag::None && (m_playhead > m_scrollSec + visible * 0.95 || m_playhead < m_scrollSec))
        m_hbar->setValue(static_cast<int>(std::max(0.0, m_playhead - visible * 0.1) * 10.0));
    update();
}

void TimelineView::selectLayer(int layer)
{
    m_selectedLayer = layer;
    m_selectedClips.clear();
    m_selectedCue.clear();
    update();
    emit selectionChanged();
}

void TimelineView::selectCue(const std::string& id)
{
    m_selectedCue = id;
    m_selectedClips.clear();
    update();
    emit selectionChanged();
}

void TimelineView::clearClipSelection()
{
    m_selectedClips.clear();
    update();
    emit selectionChanged();
}

QRect TimelineView::trackArea() const
{
    return QRect(kHeaderWidth, kRulerHeight, width() - kHeaderWidth - m_vbar->sizeHint().width(),
                 height() - kRulerHeight - m_hbar->sizeHint().height());
}

double TimelineView::timeAt(int x) const
{
    return m_scrollSec + (x - kHeaderWidth) / m_pxPerSec;
}

int TimelineView::xAt(double t) const
{
    return kHeaderWidth + static_cast<int>(std::lround((t - m_scrollSec) * m_pxPerSec));
}

int TimelineView::rowAt(int y) const
{
    if (y < kRulerHeight)
        return -1;
    const int yy = y - kRulerHeight + m_scrollY;
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        const Row& r = m_rows[static_cast<std::size_t>(i)];
        if (yy >= r.top && yy < r.top + r.height)
            return i;
    }
    return -1;
}

int TimelineView::rowTop(const Row& row) const
{
    return kRulerHeight + row.top - m_scrollY;
}

int TimelineView::layerAt(int y) const
{
    const int row = rowAt(y);
    return row >= 0 ? m_rows[static_cast<std::size_t>(row)].layer : -1;
}

QRect TimelineView::clipRect(int layer, const TimelineClip& clip) const
{
    if (layer < 0 || layer >= static_cast<int>(m_layerRow.size()) || m_layerRow[static_cast<std::size_t>(layer)] < 0)
        return {};
    const int y = rowTop(m_rows[static_cast<std::size_t>(m_layerRow[static_cast<std::size_t>(layer)])]);
    const int x0 = xAt(clip.start);
    const int x1 = std::max(x0 + 3, xAt(clip.end()));
    return QRect(x0, y + 4, x1 - x0, kRowHeight - 8);
}

QRect TimelineView::toggleRect(int layer, Toggle which) const
{
    if (layer < 0 || layer >= static_cast<int>(m_layerRow.size()) || m_layerRow[static_cast<std::size_t>(layer)] < 0)
        return {};
    const int y = rowTop(m_rows[static_cast<std::size_t>(m_layerRow[static_cast<std::size_t>(layer)])]) + kRowHeight - 22;
    const int index = which == Toggle::Visible ? 0 : which == Toggle::Mute ? 1 : which == Toggle::Lock ? 2 : 3;
    return QRect(14 + index * 48, y, 44, 18);
}

QRect TimelineView::groupAddRect(const Row& row) const
{
    return QRect(kHeaderWidth - 30, rowTop(row) + 3, 24, kGroupHeight - 6);
}

bool TimelineView::hitClip(const QPoint& pos, std::string& clipId, int& zone) const
{
    const int layer = layerAt(pos.y());
    if (layer < 0 || pos.x() < kHeaderWidth)
        return false;
    const auto& clips = m_tl.layers[static_cast<std::size_t>(layer)].clips;
    for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
        const QRect r = clipRect(layer, *it);
        if (r.contains(pos)) {
            clipId = it->id;
            const int grab = std::min(kEdgeGrab, r.width() / 3);
            zone = pos.x() <= r.left() + grab ? -1 : pos.x() >= r.right() - grab ? 1 : 0;
            return true;
        }
    }
    return false;
}

std::string TimelineView::hitCue(const QPoint& pos) const
{
    if (pos.y() >= kRulerHeight)
        return {};
    for (const auto& cue : m_tl.cues)
        if (std::abs(pos.x() - xAt(cue.time)) <= 6)
            return cue.id;
    return {};
}

double TimelineView::snapThresholdSeconds() const
{
    return 8.0 / m_pxPerSec;
}

double TimelineView::clipMaxDuration(const TimelineClip& clip) const
{
    if (clip.loop || clip.kind != ClipKind::File)
        return 0.0;
    const MediaInfo info = probeMediaCached(clip.source);
    if (!info.ok || info.still || info.duration <= 0.0)
        return 0.0;
    return std::max(kMinClipDuration, info.duration - clip.inPoint);
}

void TimelineView::updateScrollBars()
{
    const QSize hs = m_hbar->sizeHint();
    const QSize vs = m_vbar->sizeHint();
    m_hbar->setGeometry(kHeaderWidth, height() - hs.height(), width() - kHeaderWidth - vs.width(), hs.height());
    m_vbar->setGeometry(width() - vs.width(), kRulerHeight, vs.width(), height() - kRulerHeight - hs.height());

    const double visibleSec = std::max(1.0, trackArea().width() / m_pxPerSec);
    const double total = std::max(m_tl.contentEnd() + 30.0, visibleSec);
    m_hbar->setRange(0, static_cast<int>(std::max(0.0, total - visibleSec) * 10.0));
    m_hbar->setPageStep(static_cast<int>(visibleSec * 10.0));
    m_hbar->setSingleStep(std::max(1, static_cast<int>(visibleSec)));
    const int contentH = m_contentHeight;
    const int visibleH = std::max(1, trackArea().height());
    m_vbar->setRange(0, std::max(0, contentH - visibleH));
    m_vbar->setPageStep(visibleH);
    m_vbar->setSingleStep(kRowHeight / 2);
}

void TimelineView::resizeEvent(QResizeEvent*)
{
    updateScrollBars();
}

void TimelineView::zoomBy(double factor, int anchorX)
{
    if (anchorX < kHeaderWidth)
        anchorX = kHeaderWidth + trackArea().width() / 2;
    const double anchorTime = timeAt(anchorX);
    m_pxPerSec = std::clamp(m_pxPerSec * factor, kMinPxPerSec, kMaxPxPerSec);
    updateScrollBars();
    m_hbar->setValue(static_cast<int>(std::max(0.0, anchorTime - (anchorX - kHeaderWidth) / m_pxPerSec) * 10.0));
    update();
}

void TimelineView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), kBg);
    const QRect area = trackArea();

    // ---- 그룹/레이어 줄
    p.save();
    p.setClipRect(QRect(0, kRulerHeight, area.right() + 1, area.height()));
    for (const Row& row : m_rows) {
        const int y = rowTop(row);
        if (y + row.height < kRulerHeight || y > area.bottom())
            continue;
        if (row.group)
            paintGroupRow(p, row, area);
        else
            paintLayerRow(p, row, area);
    }
    p.restore();
    if (m_rows.empty()) {
        p.setPen(kDim);
        p.drawText(area, Qt::AlignCenter, QStringLiteral("영역이 없습니다. '영역' 탭에서 영역을 추가하세요."));
    }
    const int contentBottom = kRulerHeight + m_contentHeight - m_scrollY;
    p.fillRect(QRect(0, contentBottom, kHeaderWidth, std::max(0, area.bottom() - contentBottom)), kHeaderBg);

    // ---- 시간 눈금
    p.fillRect(QRect(0, 0, width(), kRulerHeight), kHeaderBg);
    const double step = niceStep(70.0 / m_pxPerSec);
    const double first = std::floor(m_scrollSec / step) * step;
    p.setPen(kDim);
    for (double t = first; xAt(t) <= area.right(); t += step) {
        const int x = xAt(t);
        if (x < kHeaderWidth)
            continue;
        p.setPen(kGrid);
        p.drawLine(x, kRulerHeight - 8, x, kRulerHeight);
        p.drawLine(x, kRulerHeight, x, area.bottom());
        p.setPen(kDim);
        p.drawText(x + 3, 4, 70, kRulerHeight - 10, Qt::AlignLeft | Qt::AlignTop, shortTime(t));
    }
    p.setPen(kText);
    p.drawText(QRect(8, 0, kHeaderWidth - 16, kRulerHeight), Qt::AlignLeft | Qt::AlignVCenter,
               m_tl.length() > 0.0 ? QStringLiteral("길이 %1").arg(shortTime(m_tl.length()))
                                   : QStringLiteral("길이: 끝없이 반복"));

    // ---- 큐
    p.save();
    p.setClipRect(QRect(kHeaderWidth, 0, area.width(), height()));
    for (const TimelineCue& cue : m_tl.cues) {
        const int x = xAt(cue.time);
        const bool sel = cue.id == m_selectedCue;
        QPen pen(sel ? kCue.lighter(130) : kCue, sel ? 2 : 1, Qt::DashLine);
        p.setPen(pen);
        p.drawLine(x, kRulerHeight, x, area.bottom());
        QPolygon flag;
        flag << QPoint(x, 2) << QPoint(x + 10, 2) << QPoint(x + 10, 12) << QPoint(x, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(sel ? kCue.lighter(130) : kCue);
        p.drawPolygon(flag);
        p.setPen(kCue);
        p.drawText(x + 13, 2, 120, 14, Qt::AlignLeft | Qt::AlignTop,
                   QString::fromStdString(cue.name.empty() ? "큐" : cue.name));
    }
    // 자석 안내선
    if (m_snapLine >= 0.0) {
        p.setPen(QPen(kSnap, 1, Qt::DashLine));
        const int x = xAt(m_snapLine);
        p.drawLine(x, kRulerHeight, x, area.bottom());
    }
    // 재생 위치
    const int px = xAt(m_playhead);
    p.setPen(QPen(kPlayhead, 2));
    p.drawLine(px, kRulerHeight - 6, px, area.bottom());
    QPolygon head;
    head << QPoint(px - 6, kRulerHeight - 12) << QPoint(px + 6, kRulerHeight - 12) << QPoint(px, kRulerHeight - 4);
    p.setPen(Qt::NoPen);
    p.setBrush(kPlayhead);
    p.drawPolygon(head);
    p.restore();

    // 드래그 선택 상자
    if (m_drag == Drag::Rubber) {
        p.setPen(QPen(QColor(120, 170, 255), 1, Qt::DashLine));
        p.setBrush(QColor(120, 170, 255, 40));
        p.drawRect(m_rubber.normalized());
    }
}

void TimelineView::paintGroupRow(QPainter& p, const Row& row, const QRect& area)
{
    const int y = rowTop(row);
    const bool collapsed = m_collapsed.count(row.groupKey) > 0;
    const std::vector<int> layers = layersInGroup(row.groupKey);
    p.fillRect(QRect(0, y, area.right() + 1, row.height), kGroupBg);
    p.setPen(kGrid);
    p.drawLine(0, y + row.height - 1, area.right(), y + row.height - 1);

    bool anyVisible = false;
    for (int l : layers)
        anyVisible = anyVisible || m_tl.layers[static_cast<std::size_t>(l)].visible;
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.setPen(layers.empty() || anyVisible ? kText : kDim);
    const QString text = QStringLiteral("%1 %2").arg(collapsed ? QStringLiteral("▶") : QStringLiteral("▼"), row.title);
    p.drawText(QRect(6, y, kHeaderWidth - 40, row.height), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(text, Qt::ElideRight, kHeaderWidth - 76));
    p.setFont(font());
    p.setPen(kDim);
    p.drawText(QRect(6, y, kHeaderWidth - 36, row.height), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1개").arg(layers.size()));
    // 레이어 추가 버튼
    const QRect add = groupAddRect(row);
    p.setPen(Qt::NoPen);
    p.setBrush(palette::kHover);
    p.drawRoundedRect(add, 3, 3);
    p.setPen(kText);
    p.drawText(add, Qt::AlignCenter, QStringLiteral("+"));

    // 접힌 그룹: 클립 위치만 얇게 표시
    p.save();
    p.setClipRect(QRect(kHeaderWidth, std::max(y, kRulerHeight), area.width(), row.height));
    if (collapsed) {
        int lane = 0;
        for (int l : layers) {
            const TimelineLayer& layer = m_tl.layers[static_cast<std::size_t>(l)];
            const int ly = y + 4 + (lane % 4) * 5;
            for (const TimelineClip& clip : layer.clips) {
                const int x0 = xAt(clip.start);
                const int x1 = std::max(x0 + 2, xAt(clip.end()));
                p.fillRect(QRect(x0, ly, x1 - x0, 3), clipColor(clip));
            }
            ++lane;
        }
    } else if (layers.empty()) {
        p.setPen(kDim);
        p.drawText(QRect(kHeaderWidth + 8, y, area.width() - 16, row.height), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("레이어 없음 ('+' 또는 '영역' 탭의 플레이리스트에서 추가)"));
    }
    p.restore();
}

void TimelineView::paintLayerRow(QPainter& p, const Row& row, const QRect& area)
{
    const int l = row.layer;
    const TimelineLayer& layer = m_tl.layers[static_cast<std::size_t>(l)];
    const int y = rowTop(row);
    p.fillRect(QRect(kHeaderWidth, y, area.width(), kRowHeight), l % 2 ? kRowB : kRowA);
    // 머리
    const QRect head(0, y, kHeaderWidth, kRowHeight);
    p.fillRect(head, l == m_selectedLayer ? palette::kSelectBg : kHeaderBg);
    p.fillRect(QRect(0, y, 6, kRowHeight), kGroupBg); // 그룹 들여쓰기
    p.setPen(kGrid);
    p.drawLine(0, y + kRowHeight - 1, area.right(), y + kRowHeight - 1);
    p.setPen(layer.visible ? kText : kDim);
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(14, y + 4, kHeaderWidth - 22, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QString::fromStdString(layer.name.empty() ? "레이어" : layer.name));
    p.setFont(font());
    p.setPen(kDim);
    QString sub;
    const std::vector<int> group = layersInGroup(row.groupKey);
    if (row.groupKey != kSoundGroup && !group.empty() && group.front() == l)
        sub = QStringLiteral("플레이리스트 (영역 탭)");
    else if (row.groupKey == kSoundGroup)
        sub = QStringLiteral("소리만");
    if (layer.holdLast && !layer.repeat)
        sub += sub.isEmpty() ? QStringLiteral("끝 화면 유지") : QStringLiteral(" · 끝 화면 유지");
    p.drawText(QRect(14, y + 18, kHeaderWidth - 22, 14), Qt::AlignLeft | Qt::AlignVCenter, sub);
    const struct {
        Toggle which;
        bool on;
        const char* onText;
        const char* offText;
    } toggles[] = {{Toggle::Visible, layer.visible, "보임", "숨김"},
                   {Toggle::Mute, !layer.muted, "소리", "무음"},
                   {Toggle::Lock, layer.locked, "잠금", "편집"},
                   {Toggle::Repeat, layer.repeat, "반복", "1회"}};
    for (const auto& t : toggles) {
        const QRect r = toggleRect(l, t.which);
        // 눈에 띄어야 하는 상태: 숨김/무음/잠금, 반복 켬
        const bool active = (t.which == Toggle::Lock || t.which == Toggle::Repeat) ? t.on : !t.on;
        p.setPen(Qt::NoPen);
        p.setBrush(active ? (t.which == Toggle::Repeat ? QColor(29, 58, 44) : QColor(74, 50, 18)) : palette::kHover);
        p.drawRoundedRect(r, 3, 3);
        p.setPen(active ? (t.which == Toggle::Repeat ? QColor(143, 224, 181) : QColor(245, 200, 120)) : kText);
        p.drawText(r, Qt::AlignCenter, QString::fromUtf8(t.on ? t.onText : t.offText));
    }

    // 클립
    p.save();
    p.setClipRect(QRect(kHeaderWidth, std::max(y, kRulerHeight), area.width(), kRowHeight));
    const double viewEnd = timeAt(area.right());

    // 반복/유지 표시 (흐리게)
    if (!layer.clips.empty()) {
        QColor ghost(255, 255, 255, 28);
        if (isContinuousLayer(layer)) {
            const TimelineClip& c = layer.clips.front();
            const QRect r = clipRect(l, c);
            if (r.right() < area.right()) {
                const QRect g(r.right() + 1, r.top(), area.right() - r.right(), r.height());
                p.fillRect(g, ghost);
                p.setPen(kDim);
                p.drawText(g.adjusted(6, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("↻ 계속 재생"));
            }
        } else if (layer.repeat) {
            const double cycle = layerCycleLength(layer);
            for (int k = 1; k < 500; ++k) {
                const double shift = cycle * k;
                if (shift > viewEnd)
                    break;
                for (const TimelineClip& clip : layer.clips) {
                    TimelineClip copy = clip;
                    copy.start += shift;
                    const QRect r = clipRect(l, copy);
                    if (r.right() < kHeaderWidth || r.left() > area.right())
                        continue;
                    p.setPen(QPen(QColor(255, 255, 255, 50), 1, Qt::DashLine));
                    p.setBrush(QColor(clipColor(clip).red(), clipColor(clip).green(), clipColor(clip).blue(), 50));
                    p.drawRoundedRect(r, 3, 3);
                    if (k == 1 && &clip == &layer.clips.front()) {
                        p.setPen(kDim);
                        p.drawText(r.adjusted(6, 2, -4, -2), Qt::AlignLeft | Qt::AlignTop, QStringLiteral("↻ 반복"));
                    }
                }
            }
        } else if (layer.holdLast) {
            const double end = layer.contentEnd();
            const int x = xAt(end);
            if (x < area.right()) {
                const QRect g(std::max(x, kHeaderWidth), y + 4, area.right() - std::max(x, kHeaderWidth), kRowHeight - 8);
                p.fillRect(g, ghost);
                p.setPen(kDim);
                p.drawText(g.adjusted(6, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("마지막 화면 유지"));
            }
        }
    }

    for (const TimelineClip& clip : layer.clips) {
        const QRect r = clipRect(l, clip);
        if (r.right() < kHeaderWidth || r.left() > area.right())
            continue;
        QColor c = clipColor(clip);
        if (layer.locked || !layer.visible)
            c = c.darker(160);
        const bool selected = m_selectedClips.count(clip.id) > 0;
        p.setPen(QPen(selected ? Qt::white : c.lighter(140), selected ? 2 : 1));
        p.setBrush(c);
        p.drawRoundedRect(r, 3, 3);
        // 페이드 표시
        p.setPen(QPen(QColor(255, 255, 255, 120), 1));
        if (clip.fadeIn > 0.0)
            p.drawLine(r.left(), r.bottom(), std::min(r.right(), r.left() + int(clip.fadeIn * m_pxPerSec)), r.top());
        if (clip.fadeOut > 0.0)
            p.drawLine(std::max(r.left(), r.right() - int(clip.fadeOut * m_pxPerSec)), r.top(), r.right(), r.bottom());
        p.setPen(Qt::white);
        QString label = clipLabel(clip);
        if (clip.loop)
            label += QStringLiteral(" ↻");
        if (clip.autoDuration)
            label += QStringLiteral(" (자동 길이)");
        p.drawText(r.adjusted(6, 2, -4, -16), Qt::AlignLeft | Qt::AlignTop, label);
        p.setPen(QColor(235, 235, 235, 200));
        p.drawText(r.adjusted(6, 16, -4, -2), Qt::AlignLeft | Qt::AlignBottom,
                   QStringLiteral("%1 · %2초").arg(shortTime(clip.start)).arg(clip.duration, 0, 'f', 1));
    }
    p.restore();
}

void TimelineView::groupMenu(const Row& row, const QPoint& globalPos)
{
    const std::vector<int> layers = layersInGroup(row.groupKey);
    QMenu menu(this);
    QAction* add = menu.addAction(QStringLiteral("이 영역에 레이어 추가"));
    menu.addSeparator();
    QAction* show = menu.addAction(QStringLiteral("모든 레이어 보임"));
    QAction* hide = menu.addAction(QStringLiteral("모든 레이어 숨김"));
    QAction* unmute = menu.addAction(QStringLiteral("모든 레이어 소리 켬"));
    QAction* mute = menu.addAction(QStringLiteral("모든 레이어 무음"));
    QAction* lock = menu.addAction(QStringLiteral("모든 레이어 잠금"));
    QAction* unlock = menu.addAction(QStringLiteral("모든 레이어 잠금 해제"));
    for (QAction* a : {show, hide, unmute, mute, lock, unlock})
        a->setEnabled(!layers.empty());
    menu.addSeparator();
    QAction* foldAll = menu.addAction(QStringLiteral("모두 접기"));
    QAction* unfoldAll = menu.addAction(QStringLiteral("모두 펴기"));
    QAction* chosen = menu.exec(globalPos);
    if (!chosen)
        return;
    if (chosen == add) {
        emit addLayerRequested(row.groupKey == kSoundGroup ? QString() : QString::fromStdString(row.groupKey));
        return;
    }
    if (chosen == foldAll || chosen == unfoldAll) {
        setAllCollapsed(chosen == foldAll);
        return;
    }
    m_dragOrig = m_tl;
    for (int l : layers) {
        TimelineLayer& layer = m_tl.layers[static_cast<std::size_t>(l)];
        if (chosen == show || chosen == hide)
            layer.visible = chosen == show;
        else if (chosen == unmute || chosen == mute)
            layer.muted = chosen == mute;
        else
            layer.locked = chosen == lock;
    }
    emitEditIfChanged();
    update();
}

void TimelineView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    const QPoint pos = event->position().toPoint();
    m_pressPos = pos;
    m_dragOrig = m_tl;
    m_snapLine = -1.0;
    if (event->button() != Qt::LeftButton)
        return;

    // 시간 눈금: 큐 선택 또는 재생 위치 이동
    if (pos.y() < kRulerHeight && pos.x() >= kHeaderWidth) {
        const std::string cue = hitCue(pos);
        if (!cue.empty()) {
            selectCue(cue);
            m_drag = Drag::Cue;
            return;
        }
        m_drag = Drag::Playhead;
        setPlayhead(std::max(0.0, timeAt(pos.x())));
        emit seekRequested(m_playhead);
        return;
    }

    // 그룹 머리: '+' = 레이어 추가, 그 외 = 접기/펴기
    const int rowIndex = rowAt(pos.y());
    if (rowIndex >= 0 && m_rows[static_cast<std::size_t>(rowIndex)].group) {
        const Row row = m_rows[static_cast<std::size_t>(rowIndex)];
        if (groupAddRect(row).contains(pos)) {
            emit addLayerRequested(row.groupKey == kSoundGroup ? QString() : QString::fromStdString(row.groupKey));
            return;
        }
        if (pos.x() < kHeaderWidth) {
            if (m_collapsed.count(row.groupKey))
                m_collapsed.erase(row.groupKey);
            else
                m_collapsed.insert(row.groupKey);
            rebuildRows();
            updateScrollBars();
            update();
        }
        return;
    }

    // 레이어 머리: 버튼 또는 레이어 선택
    if (pos.x() < kHeaderWidth) {
        const int layer = layerAt(pos.y());
        if (layer < 0)
            return;
        for (Toggle t : {Toggle::Visible, Toggle::Mute, Toggle::Lock, Toggle::Repeat}) {
            if (toggleRect(layer, t).contains(pos)) {
                TimelineLayer& l = m_tl.layers[static_cast<std::size_t>(layer)];
                if (t == Toggle::Visible)
                    l.visible = !l.visible;
                else if (t == Toggle::Mute)
                    l.muted = !l.muted;
                else if (t == Toggle::Lock)
                    l.locked = !l.locked;
                else {
                    l.repeat = !l.repeat;
                    if (l.repeat)
                        l.holdLast = false;
                }
                emitEditIfChanged();
                update();
                return;
            }
        }
        selectLayer(layer);
        return;
    }

    // 클립
    std::string clipId;
    int zone = 0;
    if (hitClip(pos, clipId, zone)) {
        const ClipLocation loc = findClip(m_tl, clipId);
        m_selectedLayer = loc.layer;
        m_selectedCue.clear();
        if (event->modifiers() & Qt::ControlModifier) {
            if (m_selectedClips.count(clipId))
                m_selectedClips.erase(clipId);
            else
                m_selectedClips.insert(clipId);
            emit selectionChanged();
            update();
            return; // Ctrl+클릭은 선택만
        }
        if (!m_selectedClips.count(clipId)) {
            m_selectedClips = {clipId};
            emit selectionChanged();
        }
        if (m_tl.layers[static_cast<std::size_t>(loc.layer)].locked) {
            update();
            return;
        }
        m_dragClip = clipId;
        m_drag = zone < 0 ? Drag::TrimStart : zone > 0 ? Drag::TrimEnd : Drag::Move;
        update();
        return;
    }

    // 빈 곳: 영역 선택 시작
    const int layer = layerAt(pos.y());
    if (layer >= 0)
        m_selectedLayer = layer;
    if (!(event->modifiers() & Qt::ControlModifier))
        m_selectedClips.clear();
    m_selectedCue.clear();
    m_drag = Drag::Rubber;
    m_rubber = QRect(pos, pos);
    emit selectionChanged();
    update();
}

void TimelineView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    if (m_drag == Drag::None) {
        // 커서 모양
        std::string clipId;
        int zone = 0;
        if (pos.y() < kRulerHeight && !hitCue(pos).empty())
            setCursor(Qt::SizeHorCursor);
        else if (hitClip(pos, clipId, zone))
            setCursor(zone != 0 ? Qt::SizeHorCursor : Qt::OpenHandCursor);
        else
            unsetCursor();
        return;
    }

    const bool snap = m_snap && !(event->modifiers() & Qt::AltModifier);
    const double threshold = snap ? snapThresholdSeconds() : 0.0;
    const double dt = (pos.x() - m_pressPos.x()) / m_pxPerSec;
    m_snapLine = -1.0;

    switch (m_drag) {
    case Drag::Playhead:
        setPlayhead(std::max(0.0, timeAt(pos.x())));
        emit seekRequested(m_playhead);
        return;
    case Drag::Cue: {
        Timeline tl = m_dragOrig;
        for (auto& cue : tl.cues) {
            if (cue.id != m_selectedCue)
                continue;
            const auto points = timelineSnapPoints(tl, {}, m_playhead);
            bool snapped = false;
            cue.time = std::max(0.0, snapTime(cue.time + dt, points, threshold, &snapped));
            if (snapped)
                m_snapLine = cue.time;
        }
        m_tl = tl;
        update();
        return;
    }
    case Drag::Rubber: {
        m_rubber.setBottomRight(pos);
        const QRect sel = m_rubber.normalized();
        std::set<std::string> picked = (event->modifiers() & Qt::ControlModifier) ? m_selectedClips
                                                                                    : std::set<std::string>{};
        for (int l = 0; l < static_cast<int>(m_tl.layers.size()); ++l)
            for (const auto& c : m_tl.layers[static_cast<std::size_t>(l)].clips)
                if (clipRect(l, c).intersects(sel))
                    picked.insert(c.id);
        if (picked != m_selectedClips) {
            m_selectedClips = picked;
            emit selectionChanged();
        }
        update();
        return;
    }
    case Drag::Move: {
        const TimelineClip* primary = clipById(m_dragOrig, m_dragClip);
        if (!primary)
            return;
        const auto points = timelineSnapPoints(m_dragOrig, m_selectedClips, m_playhead);
        // 끌고 있는 클립의 시작 또는 끝 중 가까운 쪽을 기준점에 붙임
        bool s1 = false;
        bool s2 = false;
        const double a = snapTime(primary->start + dt, points, threshold, &s1) - (primary->start + dt);
        const double b = snapTime(primary->end() + dt, points, threshold, &s2) - (primary->end() + dt);
        double adj = 0.0;
        if (s1 && (!s2 || std::fabs(a) <= std::fabs(b))) {
            adj = a;
            m_snapLine = primary->start + dt + a;
        } else if (s2) {
            adj = b;
            m_snapLine = primary->end() + dt + b;
        }
        const int fromLayer = findClip(m_dragOrig, m_dragClip).layer;
        int toLayer = layerAt(pos.y());
        if (toLayer < 0)
            toLayer = fromLayer;
        const std::vector<std::string> ids(m_selectedClips.begin(), m_selectedClips.end());
        Timeline tl = m_dragOrig;
        bool moved = moveClips(tl, ids, dt + adj, toLayer - fromLayer);
        if (!moved) { // 레이어 이동이 불가능하면(잠금/범위 밖) 시간만 이동
            tl = m_dragOrig;
            moved = moveClips(tl, ids, dt + adj, 0);
        }
        if (moved)
            m_tl = tl;
        update();
        return;
    }
    case Drag::TrimStart:
    case Drag::TrimEnd: {
        const TimelineClip* c = clipById(m_dragOrig, m_dragClip);
        if (!c)
            return;
        std::set<std::string> self{m_dragClip};
        const auto points = timelineSnapPoints(m_dragOrig, self, m_playhead);
        Timeline tl = m_dragOrig;
        bool snapped = false;
        if (m_drag == Drag::TrimStart) {
            const double t = snapTime(c->start + dt, points, threshold, &snapped);
            trimClipStart(tl, m_dragClip, t);
            if (snapped)
                m_snapLine = t;
        } else {
            const double t = snapTime(c->end() + dt, points, threshold, &snapped);
            trimClipEnd(tl, m_dragClip, t, clipMaxDuration(*c));
            if (snapped)
                m_snapLine = t;
        }
        m_tl = tl;
        update();
        return;
    }
    case Drag::None:
        return;
    }
}

void TimelineView::mouseReleaseEvent(QMouseEvent*)
{
    const Drag drag = m_drag;
    m_drag = Drag::None;
    m_snapLine = -1.0;
    if (drag == Drag::Move || drag == Drag::TrimStart || drag == Drag::TrimEnd || drag == Drag::Cue)
        emitEditIfChanged();
    if (drag == Drag::Move || drag == Drag::TrimStart || drag == Drag::TrimEnd)
        emit selectionChanged(); // 속성 칸 값 갱신
    updateScrollBars();
    update();
}

void TimelineView::emitEditIfChanged()
{
    if (m_tl != m_dragOrig)
        emit timelineEdited(m_tl);
    m_dragOrig = m_tl;
}

void TimelineView::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (event->modifiers() & Qt::ControlModifier) {
        zoomBy(delta > 0 ? 1.25 : 0.8, event->position().toPoint().x());
    } else if (event->modifiers() & Qt::ShiftModifier) {
        m_hbar->setValue(m_hbar->value() - delta / 4);
    } else {
        m_vbar->setValue(m_vbar->value() - delta / 3);
    }
    event->accept();
}

void TimelineView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace: {
        if (!m_selectedClips.empty()) {
            const std::vector<std::string> ids(m_selectedClips.begin(), m_selectedClips.end());
            if (deleteClips(m_tl, ids) > 0) {
                m_selectedClips.clear();
                emitEditIfChanged();
                emit selectionChanged();
            }
        } else if (!m_selectedCue.empty()) {
            m_tl.cues.erase(std::remove_if(m_tl.cues.begin(), m_tl.cues.end(),
                                           [&](const TimelineCue& c) { return c.id == m_selectedCue; }),
                            m_tl.cues.end());
            m_selectedCue.clear();
            emitEditIfChanged();
            emit selectionChanged();
        }
        update();
        return;
    }
    case Qt::Key_Space:
        emit playToggleRequested();
        return;
    case Qt::Key_Home:
        setPlayhead(0.0);
        emit seekRequested(0.0);
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

void TimelineView::contextMenuEvent(QContextMenuEvent* event)
{
    const QPoint pos = event->pos();
    const int rowIndex = rowAt(pos.y());
    if (rowIndex >= 0 && m_rows[static_cast<std::size_t>(rowIndex)].group) {
        groupMenu(m_rows[static_cast<std::size_t>(rowIndex)], event->globalPos());
        return;
    }
    const int layer = layerAt(pos.y());
    const double t = std::max(0.0, pos.x() >= kHeaderWidth ? timeAt(pos.x()) : m_playhead);
    QMenu menu(this);
    QAction* addFiles = menu.addAction(QStringLiteral("여기에 파일 추가..."));
    addFiles->setEnabled(layer >= 0 && !m_tl.layers[static_cast<std::size_t>(layer)].locked);
    QAction* addCue = menu.addAction(QStringLiteral("여기에 큐 추가"));
    menu.addSeparator();
    QAction* del = menu.addAction(QStringLiteral("선택한 클립 삭제"));
    del->setEnabled(!m_selectedClips.empty());
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == addFiles)
        emit addFilesRequested(layer, t);
    else if (chosen == addCue)
        emit addCueRequested(t);
    else if (chosen == del) {
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        keyPressEvent(&key);
    }
}

void TimelineView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void TimelineView::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls())
        if (url.isLocalFile())
            paths << url.toLocalFile();
    if (paths.isEmpty())
        return;
    const QPoint pos = event->position().toPoint();
    const double t = pos.x() >= kHeaderWidth ? std::max(0.0, timeAt(pos.x())) : m_playhead;
    const int rowIndex = rowAt(pos.y());
    int layer = -1;
    QString regionId;
    if (rowIndex >= 0) {
        const Row& row = m_rows[static_cast<std::size_t>(rowIndex)];
        if (row.group) {
            // 그룹 머리에 놓음: 그 영역의 첫 레이어 (없으면 새로 만듦)
            const std::vector<int> layers = layersInGroup(row.groupKey);
            layer = layers.empty() ? -1 : layers.front();
            regionId = row.groupKey == kSoundGroup ? QString() : QString::fromStdString(row.groupKey);
        } else {
            layer = row.layer;
        }
    } else {
        layer = m_selectedLayer;
    }
    emit filesDropped(layer, t, paths, regionId);
    event->acceptProposedAction();
}

} // namespace led
