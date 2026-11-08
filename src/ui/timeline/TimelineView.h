#pragma once

// 타임라인 편집 화면 (직접 그림)
//  - 왼쪽: 레이어 머리 (이름, 영역, 표시/소리/잠금 버튼)
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

    void setTimeline(const Timeline& timeline, const std::map<std::string, std::string>& regionNames);
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
    void zoomBy(double factor, int anchorX = -1);

signals:
    void timelineEdited(const Timeline& timeline);
    void selectionChanged();
    void seekRequested(double seconds);
    void playToggleRequested();
    void addFilesRequested(int layer, double time);
    void addCueRequested(double time);
    void filesDropped(int layer, double time, const QStringList& paths);

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
    enum class Toggle { None, Visible, Mute, Lock };

    QRect trackArea() const;
    double timeAt(int x) const;
    int xAt(double t) const;
    int layerAt(int y) const;              // -1 = 레이어 없음
    QRect clipRect(int layer, const TimelineClip& clip) const;
    QRect toggleRect(int layer, Toggle which) const;
    bool hitClip(const QPoint& pos, std::string& clipId, int& zone) const; // zone: -1 앞, 0 가운데, 1 뒤
    std::string hitCue(const QPoint& pos) const;
    double snapThresholdSeconds() const;
    double clipMaxDuration(const TimelineClip& clip) const; // 파일 길이 제한 (0 = 제한 없음)
    void updateScrollBars();
    void emitEditIfChanged();

    Timeline m_tl;
    std::map<std::string, std::string> m_regionNames;
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
