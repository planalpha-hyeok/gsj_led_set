#pragma once

// 운영 화면의 영역 빠른 제어: 이전/다음, 보임/숨김, 자막 문구 즉시 변경, 되돌리기
// 실제 동작은 외부 명령과 같은 경로로 처리 (commandRequested -> MainWindow::executeCommand)

#include <QString>
#include <QWidget>

class QVBoxLayout;

namespace led {

class ProjectController;

class OperatorRegionControls : public QWidget {
    Q_OBJECT

public:
    explicit OperatorRegionControls(ProjectController* controller, QWidget* parent = nullptr);

    // 이전/다음 버튼과 자막 문구를 보여줄 장면 (재생 중인 장면, 정지 중이면 편집 중인 장면)
    void setSceneId(const QString& sceneId);
    // 외부 명령으로 바꾼 상태를 되돌린 뒤 버튼 상태를 프로젝트 값으로 다시 맞춤
    void rebuild();

signals:
    void commandRequested(const QString& line);

private:
    ProjectController* m_controller;
    QVBoxLayout* m_rows;
    QWidget* m_content = nullptr;
    QString m_sceneId;
};

} // namespace led
