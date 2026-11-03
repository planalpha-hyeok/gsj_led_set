#include "core/ScheduleLogic.h"

#include <cstdio>

namespace led {

std::string LocalDay::dateString() const
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    return buf;
}

namespace {

bool appliesOnDay(const ScheduleEntry& e, const LocalDay& day)
{
    if (!e.date.empty())
        return e.date == day.dateString();
    if (day.weekday < 1 || day.weekday > 7)
        return false;
    return (e.weekdays & (1u << (day.weekday - 1))) != 0;
}

bool isActive(const ScheduleEntry& e, const LocalMoment& now)
{
    if (!e.enabled || e.sceneId.empty())
        return false;
    const int m = now.minuteOfDay;
    const int start = e.startMinute;
    const int end = e.endMinute;

    if (start == end) // 하루 종일
        return appliesOnDay(e, now.today);
    if (start < end)
        return appliesOnDay(e, now.today) && m >= start && m < end;
    // 자정을 넘기는 일정: 시작한 날 기준으로 판정
    return (appliesOnDay(e, now.today) && m >= start) || (appliesOnDay(e, now.yesterday) && m < end);
}

} // namespace

const ScheduleEntry* findActiveEntry(const Schedule& schedule, const LocalMoment& now)
{
    for (const auto& e : schedule.entries)
        if (!e.date.empty() && isActive(e, now))
            return &e;
    for (const auto& e : schedule.entries)
        if (e.date.empty() && isActive(e, now))
            return &e;
    return nullptr;
}

std::string resolveScheduledScene(const Project& project, const LocalMoment& now, const ScheduleEntry** activeEntry)
{
    const ScheduleEntry* entry = findActiveEntry(project.schedule, now);
    if (activeEntry)
        *activeEntry = entry;
    if (entry && project.findScene(entry->sceneId))
        return entry->sceneId;
    // 일정이 없거나, 일정의 장면이 삭제된 경우: 일정 외 동작 적용
    if (project.schedule.outside == OutsideBehavior::DefaultScene &&
        project.findScene(project.schedule.defaultSceneId))
        return project.schedule.defaultSceneId;
    return {};
}

} // namespace led
