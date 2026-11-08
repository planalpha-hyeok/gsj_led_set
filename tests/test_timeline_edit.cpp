// 타임라인 편집 계산 검사: 찾기, 이동(레이어 포함), 자르기, 자석, 삭제, 추가, 잠금

#include "TestUtil.h"

#include "core/TimelineEdit.h"

using namespace led;
using test::near;

static Timeline sample()
{
    Timeline tl;
    TimelineLayer a;
    a.id = "A";
    TimelineClip c1;
    c1.id = "c1";
    c1.start = 0;
    c1.duration = 10;
    TimelineClip c2;
    c2.id = "c2";
    c2.start = 12;
    c2.duration = 5;
    c2.inPoint = 2;
    a.clips = {c1, c2};
    TimelineLayer b;
    b.id = "B";
    TimelineClip c3;
    c3.id = "c3";
    c3.start = 3;
    c3.duration = 4;
    b.clips = {c3};
    tl.layers = {a, b};
    TimelineCue q;
    q.id = "q";
    q.time = 20;
    tl.cues = {q};
    return tl;
}

int main()
{
    Timeline tl = sample();
    CHECK(near(tl.length(), 17));
    CHECK(findClip(tl, "c3").layer == 1 && findClip(tl, "c3").index == 0);
    CHECK(!findClip(tl, "없음").valid());

    // 이동: 시간
    CHECK(moveClips(tl, {"c1", "c3"}, 2.5, 0));
    CHECK(near(clipById(tl, "c1")->start, 2.5) && near(clipById(tl, "c3")->start, 5.5));
    // 0초 앞으로는 못 감 (가장 앞 클립 기준으로 보정, 간격 유지)
    CHECK(moveClips(tl, {"c1", "c3"}, -100, 0));
    CHECK(near(clipById(tl, "c1")->start, 0) && near(clipById(tl, "c3")->start, 3));
    // 다른 레이어로
    CHECK(moveClips(tl, {"c3"}, 0, -1));
    CHECK(findClip(tl, "c3").layer == 0 && tl.layers[1].clips.empty());
    // 범위 밖 레이어: 변경 없음
    const Timeline before = tl;
    CHECK(!moveClips(tl, {"c1"}, 1, 5));
    CHECK(tl == before);
    // 시작 순서로 정렬 유지
    for (std::size_t i = 1; i < tl.layers[0].clips.size(); ++i)
        CHECK(tl.layers[0].clips[i - 1].start <= tl.layers[0].clips[i].start);

    // 앞쪽 자르기: 시작을 13으로 -> 길이 4, 자르기 지점 3
    tl = sample();
    CHECK(trimClipStart(tl, "c2", 13));
    TimelineClip* c2 = clipById(tl, "c2");
    CHECK(near(c2->start, 13) && near(c2->duration, 4) && near(c2->inPoint, 3) && near(c2->end(), 17));
    // 자르기 지점이 0 미만이 되도록 앞으로 늘리면 거기서 멈춤 (start-inPoint = 10)
    CHECK(trimClipStart(tl, "c2", 0));
    CHECK(near(c2->start, 10) && near(c2->inPoint, 0) && near(c2->end(), 17));
    // 끝을 넘어가면 최소 길이
    CHECK(trimClipStart(tl, "c2", 100));
    CHECK(near(c2->duration, kMinClipDuration) && near(c2->end(), 17));

    // 뒤쪽 자르기 + 파일 길이 제한
    tl = sample();
    CHECK(trimClipEnd(tl, "c1", 30, 0));
    CHECK(near(clipById(tl, "c1")->duration, 30));
    CHECK(trimClipEnd(tl, "c1", 30, 12));
    CHECK(near(clipById(tl, "c1")->duration, 12));
    CHECK(trimClipEnd(tl, "c1", -5, 0));
    CHECK(near(clipById(tl, "c1")->duration, kMinClipDuration));

    // 자석
    tl = sample();
    const auto points = timelineSnapPoints(tl, {"c1"}, 7.0);
    bool snapped = false;
    CHECK(near(snapTime(11.8, points, 0.5, &snapped), 12) && snapped);  // c2 시작
    CHECK(near(snapTime(19.7, points, 0.5, &snapped), 20) && snapped);  // 큐
    CHECK(near(snapTime(7.2, points, 0.5, &snapped), 7) && snapped);    // 재생 위치
    CHECK(near(snapTime(9.9, points, 0.05, &snapped), 9.9) && !snapped); // c1은 제외, 범위 밖
    CHECK(near(snapTime(0.3, points, 0.5, &snapped), 0) && snapped);    // 0초

    // 잠금
    tl = sample();
    tl.layers[1].locked = true;
    CHECK(!moveClips(tl, {"c3"}, 1, 0));
    CHECK(!moveClips(tl, {"c1"}, 0, 1)); // 잠긴 레이어로 이동 불가
    CHECK(!trimClipEnd(tl, "c3", 20, 0));
    CHECK(deleteClips(tl, {"c1", "c3"}) == 1); // 잠긴 레이어의 c3는 남음
    CHECK(clipById(tl, "c3") && !clipById(tl, "c1"));
    TimelineClip n;
    n.id = "n";
    CHECK(!addClip(tl, 1, n));
    CHECK(addClip(tl, 0, n) && clipById(tl, "n"));
    CHECK(!addClip(tl, 9, n));

    return test::finish("test_timeline_edit");
}
