#include "schedule/ScheduleRunner.h"

#include "core/Log.h"

#include <QDateTime>

namespace led {

namespace {

LocalDay toLocalDay(const QDate& d)
{
    LocalDay day;
    day.year = d.year();
    day.month = d.month();
    day.day = d.day();
    day.weekday = d.dayOfWeek(); // 1 = 월 ... 7 = 일
    return day;
}

} // namespace

ScheduleRunner::ScheduleRunner(ProjectProvider provider, QObject* parent)
    : QObject(parent)
    , m_provider(std::move(provider))
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] { evaluate(false); });
}

LocalMoment ScheduleRunner::currentMoment()
{
    const QDateTime now = QDateTime::currentDateTime();
    LocalMoment m;
    m.today = toLocalDay(now.date());
    m.yesterday = toLocalDay(now.date().addDays(-1));
    m.minuteOfDay = now.time().hour() * 60 + now.time().minute();
    return m;
}

void ScheduleRunner::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    if (active) {
        m_lastScene.clear();
        m_timer.start();
        logInfo("스케줄 운영 시작");
        evaluate(true);
    } else {
        m_timer.stop();
        logInfo("스케줄 운영 종료");
        m_status.clear();
        emit statusChanged(m_status);
    }
}

void ScheduleRunner::reevaluate()
{
    if (m_active)
        evaluate(false);
}

void ScheduleRunner::evaluate(bool force)
{
    const Project& project = m_provider();
    const ScheduleEntry* entry = nullptr;
    const QString scene = QString::fromStdString(resolveScheduledScene(project, currentMoment(), &entry));

    QString status;
    const Scene* s = project.findScene(scene.toStdString());
    const QString sceneName = s ? QString::fromStdString(s->name) : QString();
    if (entry && s)
        status = QStringLiteral("일정 '%1' → %2").arg(QString::fromStdString(entry->name), sceneName);
    else if (s)
        status = QStringLiteral("일정 없음 → 기본 장면 %1").arg(sceneName);
    else
        status = QStringLiteral("일정 없음 → 정지");
    if (status != m_status) {
        m_status = status;
        emit statusChanged(m_status);
    }

    if (force || scene != m_lastScene) {
        m_lastScene = scene;
        logInfo("스케줄: " + status.toStdString());
        emit sceneRequested(scene);
    }
}

} // namespace led
