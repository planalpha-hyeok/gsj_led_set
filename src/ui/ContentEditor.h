#pragma once

// 선택된 영역의 콘텐츠 편집 (편집 중인 장면 기준)
// 종류: 플레이리스트(영상/이미지) 또는 흐르는 자막
// = 장면 타임라인에서 그 영역의 첫 번째 레이어를 간단히 보여주는 화면

#include "core/Project.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QStackedWidget;

namespace led {

class PlaylistEditor;
class ProjectController;
class TickerEditor;

class ContentEditor : public QWidget {
    Q_OBJECT

public:
    explicit ContentEditor(ProjectController* controller, QWidget* parent = nullptr);

private slots:
    void reload();
    void onKindChanged(int index);
    void applyPlaylist(const RegionContent& content);
    void applyTicker(const RegionContent& content);

private:
    RegionContent currentContent() const;
    void store(const RegionContent& content);

    ProjectController* m_controller;
    QLabel* m_sceneLabel;
    QComboBox* m_kind;
    QStackedWidget* m_stack;
    PlaylistEditor* m_playlist;
    TickerEditor* m_ticker;
    bool m_loading = false;
    bool m_applying = false;
};

} // namespace led
