#pragma once

// 스케줄 판정 (표준 C++만 사용, 시계와 분리되어 있어 검사 가능)

#include "core/Project.h"

#include <string>

namespace led {

struct LocalDay {
    int year = 2000;
    int month = 1;
    int day = 1;
    int weekday = 1;     // 1 = 월 ... 7 = 일

    std::string dateString() const; // "YYYY-MM-DD"
};

struct LocalMoment {
    LocalDay today;
    LocalDay yesterday;
    int minuteOfDay = 0; // 0 ~ 1439
};

// 지금 적용되는 일정 (없으면 nullptr)
// 우선순위: 날짜 지정 일정 > 요일 일정. 같은 종류끼리는 목록 위쪽 우선
const ScheduleEntry* findActiveEntry(const Schedule& schedule, const LocalMoment& now);

// 지금 재생해야 할 장면 ID (빈 문자열 = 정지/검은 화면)
std::string resolveScheduledScene(const Project& project, const LocalMoment& now,
                                  const ScheduleEntry** activeEntry = nullptr);

} // namespace led
