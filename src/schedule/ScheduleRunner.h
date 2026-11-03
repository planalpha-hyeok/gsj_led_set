#pragma once

// 스케줄 실행기: 1초마다 현재 시각으로 재생할 장면을 판단해 바뀔 때 알림

#include "core/Project.h"
#include "core/ScheduleLogic.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

namespace led {

class ScheduleRunner : public QObject {
    Q_OBJECT

public:
    using ProjectProvider = std::function<const Project&()>;

    explicit ScheduleRunner(ProjectProvider provider, QObject* parent = nullptr);

    void setActive(bool active);
    bool isActive() const { return m_active; }

    // 스케줄 편집 후 즉시 다시 판단
    void reevaluate();

    // 현재 판단 결과 설명 (예: "일정 '오전' → 장면 1")
    QString statusText() const { return m_status; }

    static LocalMoment currentMoment();

signals:
    // 재생할 장면이 바뀜 (빈 문자열 = 정지)
    void sceneRequested(const QString& sceneId);
    void statusChanged(const QString& text);

private:
    void evaluate(bool force);

    ProjectProvider m_provider;
    QTimer m_timer;
    bool m_active = false;
    QString m_lastScene;
    QString m_status;
};

} // namespace led
