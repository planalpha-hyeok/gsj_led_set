#pragma once

// 장면 목록: 추가/복제/이름 변경/삭제/순서, 편집할 장면 선택, 바로 재생

#include <QWidget>

class QLineEdit;
class QListWidget;
class QPushButton;

namespace led {

class ProjectController;

class ScenePanel : public QWidget {
    Q_OBJECT

public:
    explicit ScenePanel(ProjectController* controller, QWidget* parent = nullptr);

    // 재생 중인 장면 표시 (빈 문자열 = 정지)
    void setPlayingScene(const QString& sceneId);

signals:
    void playSceneRequested(const QString& sceneId);

private slots:
    void refresh();

private:
    QString selectedId() const;

    ProjectController* m_controller;
    QString m_playingScene;
    QListWidget* m_list;
    QLineEdit* m_name;
    QPushButton* m_addButton;
    QPushButton* m_duplicateButton;
    QPushButton* m_removeButton;
    QPushButton* m_upButton;
    QPushButton* m_downButton;
    QPushButton* m_playButton;
};

} // namespace led
