#pragma once

// 운영 화면의 장면 버튼 (누르면 바로 전환). Ctrl+1 ~ Ctrl+9 = 1~9번째 장면

#include <QString>
#include <QWidget>

#include <vector>

class QGridLayout;
class QPushButton;
class QShortcut;

namespace led {

class ProjectController;

class OperatorSceneGrid : public QWidget {
    Q_OBJECT

public:
    explicit OperatorSceneGrid(ProjectController* controller, QWidget* parent = nullptr);

    void setLiveScene(const QString& sceneId);

signals:
    void playSceneRequested(const QString& sceneId);

private:
    void rebuild();
    void updateHighlight();

    ProjectController* m_controller;
    QGridLayout* m_grid;
    std::vector<QPushButton*> m_buttons;
    std::vector<QShortcut*> m_shortcuts;
    QString m_liveScene;
};

} // namespace led
