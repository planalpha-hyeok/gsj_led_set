// 스케줄 판정 검사 (요일, 날짜 우선, 자정 넘김, 하루 종일, 일정 외 동작)

#include "TestUtil.h"

#include "core/ScheduleLogic.h"

using namespace led;

namespace {

LocalDay day(int y, int m, int d, int weekday)
{
    LocalDay out;
    out.year = y;
    out.month = m;
    out.day = d;
    out.weekday = weekday;
    return out;
}

// 2026-11-02 (월) 기준
LocalMoment at(int weekday, int hour, int minute)
{
    LocalMoment m;
    const int dayOfMonth = 1 + weekday; // 11/2 = 월(1) ... 11/8 = 일(7)
    m.today = day(2026, 11, dayOfMonth, weekday);
    m.yesterday = day(2026, 11, dayOfMonth - 1, weekday == 1 ? 7 : weekday - 1);
    m.minuteOfDay = hour * 60 + minute;
    return m;
}

ScheduleEntry entry(const char* id, const char* scene, std::uint8_t days, int start, int end, const char* date = "")
{
    ScheduleEntry e;
    e.id = id;
    e.name = id;
    e.sceneId = scene;
    e.weekdays = days;
    e.startMinute = start;
    e.endMinute = end;
    e.date = date;
    return e;
}

Project makeProject()
{
    Project p;
    for (const char* id : {"A", "B", "C", "D"}) {
        Scene s;
        s.id = id;
        s.name = id;
        p.scenes.push_back(s);
    }
    p.schedule.entries = {
        entry("weekday-am", "A", 0x1F, 9 * 60, 12 * 60),           // 평일 09~12
        entry("night", "C", 0x40, 22 * 60, 2 * 60),                // 일요일 22시 ~ 월요일 02시
        entry("weekday-all", "B", 0x1F, 12 * 60, 12 * 60),         // 평일 하루 종일 (위 일정들이 우선)
        entry("event", "D", 0, 10 * 60, 11 * 60, "2026-11-03"),     // 11/3(화) 10~11시 행사
    };
    return p;
}

std::string sceneAt(const Project& p, const LocalMoment& m)
{
    return resolveScheduledScene(p, m);
}

} // namespace

static void testWeekdays()
{
    const Project p = makeProject();
    CHECK(sceneAt(p, at(1, 9, 0)) == "A");    // 시작 시각 포함
    CHECK(sceneAt(p, at(1, 11, 59)) == "A");
    CHECK(sceneAt(p, at(1, 12, 0)) == "B");   // 종료 시각 미포함 -> 하루 종일 일정
    CHECK(sceneAt(p, at(1, 8, 59)) == "B");   // 오전 일정 전에도 하루 종일 일정
    CHECK(sceneAt(p, at(6, 10, 0)).empty());  // 토요일: 일정 없음 -> 정지
}

static void testDatePriority()
{
    const Project p = makeProject();
    CHECK(sceneAt(p, at(2, 10, 30)) == "D");  // 11/3 행사가 평일 일정보다 우선
    CHECK(sceneAt(p, at(2, 11, 0)) == "A");   // 행사 끝나면 평일 일정
    CHECK(sceneAt(p, at(3, 10, 30)) == "A");  // 다른 날짜는 해당 없음
}

static void testMidnight()
{
    const Project p = makeProject();
    CHECK(sceneAt(p, at(7, 22, 0)) == "C");   // 일요일 22시
    CHECK(sceneAt(p, at(7, 23, 59)) == "C");
    CHECK(sceneAt(p, at(1, 1, 59)) == "C");   // 월요일 새벽 (전날 일요일 일정)
    CHECK(sceneAt(p, at(1, 2, 0)) == "B");    // 02시 종료 후 평일 하루 종일
    CHECK(sceneAt(p, at(7, 21, 59)).empty()); // 일요일 22시 전
    CHECK(sceneAt(p, at(2, 1, 0)) == "B");    // 화요일 새벽: 월요일은 일정 요일 아님
}

static void testOutsideAndDisabled()
{
    Project p = makeProject();
    p.schedule.outside = OutsideBehavior::DefaultScene;
    p.schedule.defaultSceneId = "B";
    CHECK(sceneAt(p, at(6, 10, 0)) == "B");   // 토요일: 기본 장면

    p.schedule.defaultSceneId = "없는장면";
    CHECK(sceneAt(p, at(6, 10, 0)).empty());

    // 꺼진 일정은 무시
    p.schedule.entries[0].enabled = false;
    p.schedule.outside = OutsideBehavior::Black;
    CHECK(sceneAt(p, at(1, 10, 0)) == "B");

    // 삭제된 장면을 가리키는 일정 -> 그 일정은 건너뛰지 않고, 일정 외 동작 적용
    Project q = makeProject();
    q.schedule.entries = {entry("x", "삭제됨", 0x7F, 0, 0)};
    CHECK(sceneAt(q, at(1, 10, 0)).empty());
    const ScheduleEntry* active = nullptr;
    resolveScheduledScene(q, at(1, 10, 0), &active);
    CHECK(active != nullptr);
}

static void testDateString()
{
    CHECK(day(2026, 1, 5, 1).dateString() == "2026-01-05");
}

int main()
{
    testWeekdays();
    testDatePriority();
    testMidnight();
    testOutsideAndDisabled();
    testDateString();
    return test::finish("test_schedule");
}
