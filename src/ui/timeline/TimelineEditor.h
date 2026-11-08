#pragma once

// 타임라인 편집 창 (편집 중인 장면이 타임라인 장면일 때)
//  위: 재생 제어(재생/일시정지, 처음으로, GO), 시간, 레이어/클립/큐 추가, 끝 동작, 자석, 확대/축소
//  가운데: 타임라인 화면, 오른쪽: 속성

#include "playback/TimelinePlayer.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace led {

class ProjectController;
class TimelinePropertyPanel;
class TimelineView;

class TimelineEditor : public QWidget {
    Q_OBJECT

public:
    explicit TimelineEditor(ProjectController* controller, QWidget* parent = nullptr);

    // 편집 중인 타임라인 장면 ID (타임라인 장면이 아니면 빈 문자열)
    QString sceneId() const;

    // 재생 엔진 상태 표시 (playingThisScene: 이 장면이 재생 중)
    void setPlaybackStatus(const TimelineStatus& status, bool playingThisScene);

signals:
    void playPauseRequested(const QString& sceneId, double fromTime);
    void seekRequested(double seconds);
    void goRequested();

private slots:
    void reload();
    void applyEdit(const Timeline& timeline);
    void updatePanel();
    void addLayer();
    void removeLayer();
    void moveLayer(int delta);
    void addFiles(int layer, double time);
    void insertFiles(int layer, double time, const QStringList& paths);
    void addCue(double time);
    void onSeek(double seconds);
    void onPlayToggle();

private:
    std::vector<std::pair<std::string, std::string>> regionList() const;

    ProjectController* m_controller;
    QStackedWidget* m_stack;
    TimelineView* m_view;
    TimelinePropertyPanel* m_panel;
    QLabel* m_time;
    QPushButton* m_play;
    QPushButton* m_go;
    QComboBox* m_endAction;
    QCheckBox* m_snap;
    bool m_playingThis = false;
    bool m_running = false;
};

} // namespace led
