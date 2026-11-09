#pragma once

// 타임라인 편집 화면 (직접 그림)
//  - 영역마다 그룹으로 묶어 표시 (프로젝트 영역 순서). 그룹 머리를 눌러 접기/펴기, '+'로 레이어 추가
//    영역이 없는 레이어는 '소리 / 영역 없음' 그룹. 영역의 첫 레이어 = '영역' 탭의 플레이리스트
//  - 왼쪽: 레이어 머리 (이름, 표시/소리/잠금/반복 버튼)
//  - 반복 레이어는 반복되는 모습을 흐리게, 마지막 화면 유지는 유지 구간을 표시
//  - 위: 시간 눈금, 큐 표시, 재생 위치
//  - 클립: 끌어 이동(다른 레이어로도), 양 끝을 끌어 자르기, Ctrl+클릭/빈 곳 끌기로 여러 개 선택
//  - 자석: 클립 끝, 큐, 재생 위치, 0초에 붙음 (Alt를 누르면 일시 해제)
//  - Ctrl+휠: 확대/축소, Shift+휠: 가로 이동, Delete: 삭제, Space: 재생/일시정지

#include "core/Project.h"

#include <QWidget>

#include <map>
#include <set>
#include <string>
#include <vector>

class QScrollBar;

namespace led {

class TimelineView : public QWidget {
    Q_OBJECT

public:
    explicit TimelineView(QWidget* parent = nullptr);

    // regions: 프로젝트 영역 순서의 (id, 이름)
    void setTimeline(const Timeline& timeline, const std::vector<std::pair<std::string, std::string>>& regions);
    const Timeline& timeline() const { return m_tl; }
    bool isDragging() const { return m_drag != Drag::None; }

    void setPlayhead(double seconds);
    double playhead() const { return m_playhead; }

    const std::set<std::string>& selectedClips() const { return m_selectedClips; }
    int selectedLayer() const { return m_selectedLayer; }
    const std::string& selectedCue() const { return m_selectedCue; }
    void selectLayer(int layer);
    void selectCue(const std::string& id);
    void clearClipSelection();

    void setSnapEnabled(bool on) { m_snap = on; }
    void setAllCollapsed(bool collapsed);
    // 레이어가 속한 그룹 (영역 id, 영역 없음 = 빈 문자열)
    std::string groupRegionOf(int layer) const;
    void zoomBy(double factor, int anchorX = -1);

signals:
    void timelineEdited(const Timeline& timeline);
    void selectionChanged();
    void seekRequested(double seconds);
    void playToggleRequested();
    void addFilesRequested(int layer, double time);
    void addCueRequested(double time);
    // layer < 0 이면 regionId 영역에 새 레이어를 만들어 추가
    void filesDropped(int layer, double time, const QStringList& paths, const QString& regionId);
    void addLayerRequested(const QString& regionId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    enum class Drag { None, Move, TrimStart, TrimEnd, Rubber, Playhead, Cue };
    enum class Toggle { None, Visible, Mute, Lock, Repeat };

    // 화면 줄: 그룹 머리 또는 레이어
    struct Row {
        bool group = false;
        std::string groupKey;    // 영역 id 또는 kSoundGroup
        QString title;           // 그룹 이름
        int layer = -1;          // 레이어 줄의 레이어 번호
        int top = 0;             // 내용 기준 위치 (스크롤 전)
        int height = 0;
    };

    void rebuildRows();
    std::string groupKeyOf(const TimelineLayer& layer) const;
    std::vector<int> layersInGroup(const std::string& key) const;
    int rowAt(int y) const;                // -1 = 줄 없음
    int rowTop(const Row& row) const;      // 화면 위치
    void paintLayerRow(QPainter& p, const Row& row, const QRect& area);
    void paintGroupRow(QPainter& p, const Row& row, const QRect& area);
    void groupMenu(const Row& row, const QPoint& globalPos);

    QRect trackArea() const;
    double timeAt(int x) const;
    int xAt(double t) const;
    int layerAt(int y) const;              // -1 = 레이어 없음 (그룹 머리 포함)
    QRect clipRect(int layer, const TimelineClip& clip) const;
    QRect toggleRect(int layer, Toggle which) const;
    QRect groupAddRect(const Row& row) const;
    bool hitClip(const QPoint& pos, std::string& clipId, int& zone) const; // zone: -1 앞, 0 가운데, 1 뒤
    std::string hitCue(const QPoint& pos) const;
    double snapThresholdSeconds() const;
    double clipMaxDuration(const TimelineClip& clip) const; // 파일 길이 제한 (0 = 제한 없음)
    void updateScrollBars();
    void emitEditIfChanged();

    Timeline m_tl;
    std::vector<std::pair<std::string, std::string>> m_regions;
    std::vector<Row> m_rows;
    std::vector<int> m_layerRow;           // 레이어 번호 -> 줄 번호 (접힌 그룹 = -1)
    std::set<std::string> m_collapsed;     // 접힌 그룹
    int m_contentHeight = 0;
    double m_pxPerSec = 40.0;
    double m_scrollSec = 0.0;
    int m_scrollY = 0;
    double m_playhead = 0.0;
    bool m_snap = true;

    std::set<std::string> m_selectedClips;
    int m_selectedLayer = -1;
    std::string m_selectedCue;

    Drag m_drag = Drag::None;
    QPoint m_pressPos;
    Timeline m_dragOrig;
    std::string m_dragClip;
    QRect m_rubber;
    double m_snapLine = -1.0;            // 그리는 동안 붙은 위치 표시

    QScrollBar* m_hbar;
    QScrollBar* m_vbar;
};

} // namespace led
