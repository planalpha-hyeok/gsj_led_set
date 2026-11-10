// 맞춤 방식 계산, 출력 좌표 변환, 자르기, 자막 위치 계산 검사

#include "TestUtil.h"

#include "core/Geometry.h"
#include "core/Snap.h"
#include "core/TickerMotion.h"
#include "render/Scene.h"

using namespace led;
using test::near;

static void testStretch()
{
    const Placement p = computePlacement(RectI{100, 50, 400, 200}, 1920, 1080, FitMode::Stretch);
    CHECK(p.visible);
    CHECK(near(p.dst.x, 100) && near(p.dst.y, 50) && near(p.dst.w, 400) && near(p.dst.h, 200));
    CHECK(near(p.uv.x, 0) && near(p.uv.y, 0) && near(p.uv.w, 1) && near(p.uv.h, 1));
}

static void testFit()
{
    const Placement p = computePlacement(RectI{0, 0, 1000, 500}, 1000, 1000, FitMode::Fit);
    CHECK(near(p.dst.w, 500) && near(p.dst.h, 500));
    CHECK(near(p.dst.x, 250) && near(p.dst.y, 0));
    CHECK(near(p.uv.w, 1) && near(p.uv.h, 1));
}

static void testFill()
{
    const Placement p = computePlacement(RectI{10, 20, 1000, 500}, 1000, 1000, FitMode::Fill);
    CHECK(near(p.dst.x, 10) && near(p.dst.y, 20) && near(p.dst.w, 1000) && near(p.dst.h, 500));
    CHECK(near(p.uv.x, 0) && near(p.uv.y, 0.25) && near(p.uv.w, 1) && near(p.uv.h, 0.5));
}

static void testOriginal()
{
    Placement p = computePlacement(RectI{0, 0, 960, 540}, 1920, 1080, FitMode::Original);
    CHECK(near(p.dst.w, 960) && near(p.dst.h, 540));
    CHECK(near(p.uv.x, 0) && near(p.uv.y, 0) && near(p.uv.w, 0.5) && near(p.uv.h, 0.5));

    p = computePlacement(RectI{100, 100, 1920, 1080}, 640, 360, FitMode::Original);
    CHECK(near(p.dst.x, 100) && near(p.dst.y, 100) && near(p.dst.w, 640) && near(p.dst.h, 360));
    CHECK(near(p.uv.w, 1) && near(p.uv.h, 1));
}

static void testInvalid()
{
    CHECK(!computePlacement(RectI{0, 0, 0, 100}, 100, 100, FitMode::Stretch).visible);
    CHECK(!computePlacement(RectI{0, 0, 100, 100}, 0, 100, FitMode::Fit).visible);
}

static void testClip()
{
    // 1000px 띠 중 영역(100~600)에 걸친 부분만 남김
    RectF dst{-200, 0, 1000, 50};
    RectF uv{0, 0, 1, 1};
    CHECK(clipPlacement(dst, uv, RectF{100, 0, 500, 50}));
    CHECK(near(dst.x, 100) && near(dst.w, 500) && near(dst.y, 0) && near(dst.h, 50));
    CHECK(near(uv.x, 0.3) && near(uv.w, 0.5) && near(uv.y, 0) && near(uv.h, 1));

    // 겹치지 않음
    RectF d2{0, 0, 10, 10};
    RectF u2{0, 0, 1, 1};
    CHECK(!clipPlacement(d2, u2, RectF{10, 0, 10, 10}));

    // uv가 이미 일부인 경우에도 비율 유지
    RectF d3{0, 0, 100, 100};
    RectF u3{0.5, 0.5, 0.5, 0.5};
    CHECK(clipPlacement(d3, u3, RectF{50, 0, 100, 50}));
    CHECK(near(u3.x, 0.75) && near(u3.w, 0.25) && near(u3.y, 0.5) && near(u3.h, 0.25));
}

static void testTickerPositions()
{
    std::vector<double> pos;

    // 시작 직후: 아무것도 안 보임
    tickerCopyPositions(0, 0, 1000, 300, TickerDirection::Left, pos);
    CHECK(pos.empty());

    // 100px 이동: 첫 사본이 오른쪽 끝에서 들어옴 (시작 위치 900)
    tickerCopyPositions(100, 0, 1000, 300, TickerDirection::Left, pos);
    CHECK(pos.size() == 1 && near(pos[0], 900));

    // 700px 이동: 사본 0(300), 1(600), 2(900)
    tickerCopyPositions(700, 0, 1000, 300, TickerDirection::Left, pos);
    CHECK(pos.size() == 3);
    if (pos.size() == 3)
        CHECK(near(pos[0], 300) && near(pos[1], 600) && near(pos[2], 900));

    // 오래 흐른 뒤에도 보이는 사본만 (영역 1000 / 띠 300 -> 최대 5개)
    tickerCopyPositions(1000000, 0, 1000, 300, TickerDirection::Left, pos);
    CHECK(!pos.empty() && pos.size() <= 5);
    for (double p : pos)
        CHECK(p < 1000 && p + 300 > 0);

    // 오른쪽으로: 왼쪽 끝에서 들어옴
    tickerCopyPositions(100, 50, 1000, 300, TickerDirection::Right, pos);
    CHECK(pos.size() == 1 && near(pos[0], 50 - 300 + 100));

    // 위로: 세로 축 동일 규칙
    tickerCopyPositions(100, 0, 200, 80, TickerDirection::Up, pos);
    CHECK(pos.size() == 2);
    if (pos.size() == 2)
        CHECK(near(pos[0], 100) && near(pos[1], 180));

    // 잘못된 값
    tickerCopyPositions(100, 0, 1000, 0, TickerDirection::Left, pos);
    CHECK(pos.empty());
}

static void testDrawItems()
{
    auto frame = std::make_shared<Frame>();
    frame->width = 16;
    frame->height = 16;

    std::vector<SceneLayer> layers(3);
    layers[0].key = 1;
    layers[0].dst = RectF{0, 0, 1920, 1080};
    layers[1].key = 2;
    layers[1].dst = RectF{1000, 100, 2000, 500};
    layers[2].key = 3;
    layers[2].dst = RectF{2000, 0, 100, 100};
    layers[2].opacity = 0.0f;
    for (auto& l : layers) {
        l.uv = RectF{0, 0, 1, 1};
        l.frame = frame;
    }

    std::vector<DrawItem> items;
    buildDrawItems(layers, RectI{1920, 0, 1920, 1080}, items);
    CHECK(items.size() == 1);
    if (items.size() == 1) {
        CHECK(items[0].key == 2);
        CHECK(near(items[0].dst.x, 1000 - 1920) && near(items[0].dst.y, 100));
        CHECK(near(items[0].dst.w, 2000) && near(items[0].dst.h, 500));
    }

    std::vector<SceneLayer> edge(1);
    edge[0].dst = RectF{0, 0, 1920, 1080};
    edge[0].frame = frame;
    buildDrawItems(edge, RectI{1920, 0, 1920, 1080}, items);
    CHECK(items.empty());
}

static void testSnap()
{
    const std::vector<double> xs{0, 1920, 3840};
    const std::vector<double> ys{0, 1080};

    // 이동: 왼쪽 테두리가 1920 근처(5px) -> 1920에 붙음, 크기 유지
    SnapResult r = snapRect(RectF{1915.4, 3.0, 500, 300}, true, 0, xs, ys, 8);
    CHECK(r.rect == (RectI{1920, 0, 500, 300}));
    CHECK(r.snappedX && r.snappedY && near(r.guideX, 1920) && near(r.guideY, 0));

    // 이동: 오른쪽 테두리가 더 가까우면 오른쪽 기준 (1418+500=1918 -> 1920)
    r = snapRect(RectF{1418, 500, 500, 100}, true, 0, xs, ys, 8);
    CHECK(r.rect == (RectI{1420, 500, 500, 100}));
    CHECK(r.snappedX && near(r.guideX, 1920) && !r.snappedY);

    // 가운데 맞춤: 영역 가운데(1916)가 기준선 1920에 붙음
    {
        const std::vector<double> cx{0, 1920, 3840};
        r = snapRect(RectF{1666, 500, 500, 100}, true, 0, cx, ys, 8); // 가운데 1916
        CHECK(r.rect == (RectI{1670, 500, 500, 100}));
        CHECK(r.snappedX && near(r.guideX, 1920));
        // 세로 가운데: 가운데 540 근처
        const std::vector<double> cy{0, 540, 1080};
        r = snapRect(RectF{100, 493, 50, 100}, true, 0, cx, cy, 8); // 가운데 543
        CHECK(r.rect == (RectI{100, 490, 50, 100}) && r.snappedY && near(r.guideY, 540));
    }

    // 범위 밖: 반올림만
    r = snapRect(RectF{100.6, 200.4, 50.2, 60.7}, true, 0, xs, ys, 8);
    CHECK(r.rect == (RectI{101, 200, 50, 61}));
    CHECK(!r.snappedX && !r.snappedY);

    // 크기 변경: 오른쪽/아래 테두리만 붙음, 왼쪽/위는 고정
    r = snapRect(RectF{100, 100, 1815, 975}, false, SnapRight | SnapBottom, xs, ys, 8);
    CHECK(r.rect == (RectI{100, 100, 1820, 980}));

    // 크기 변경: 왼쪽 테두리 -> 0에 붙고 오른쪽은 고정
    r = snapRect(RectF{6, 100, 994, 50}, false, SnapLeft, xs, ys, 8);
    CHECK(r.rect == (RectI{0, 100, 1000, 50}));

    // 자석 끔 (threshold 0)
    r = snapRect(RectF{1917, 0, 10, 10}, true, 0, xs, ys, 0);
    CHECK(r.rect == (RectI{1917, 0, 10, 10}) && !r.snappedX);

    // 크기가 0 이하로 뒤집히지 않음
    r = snapRect(RectF{100, 100, -20, -5}, false, SnapRight | SnapBottom, {}, {}, 8);
    CHECK(r.rect.w == 1 && r.rect.h == 1 && r.rect.x == 100 && r.rect.y == 100);

    // 우선순위: 다른 영역 테두리(2)가 더 가까워도 캔버스 테두리(0)에 붙음
    {
        std::vector<SnapLines> groups(2);
        groups[0].xs = {0, 960, 1920};
        groups[0].ys = {0, 540, 1080};
        groups[1].xs = {2};
        groups[1].ys = {2};
        r = snapRect(RectF{3, 4, 300, 200}, true, 0, groups, 8);
        CHECK(r.rect == (RectI{0, 0, 300, 200}));
        CHECK(near(r.guideX, 0) && near(r.guideY, 0));
        // 캔버스 기준선이 범위 밖이면 다음 그룹 사용
        groups[1].xs = {500};
        r = snapRect(RectF{497, 300, 100, 100}, true, 0, groups, 8);
        CHECK(r.rect.x == 500 && r.snappedX && !r.snappedY);
        // 크기 변경도 같은 우선순위
        groups[1].xs = {1918};
        r = snapRect(RectF{100, 100, 1817, 50}, false, SnapRight, groups, 8);
        CHECK(r.rect.right() == 1920);
    }

    // 작은 영역: 붙으면 너비가 1px 미만이 되는 경우 붙이지 않음
    {
        const std::vector<double> lx{100};
        // 오른쪽 테두리(103)를 끄는 중, 기준선 100에 붙으면 너비 0 -> 붙이지 않음
        r = snapRect(RectF{101, 0, 2, 10}, false, SnapRight, lx, {}, 8);
        CHECK(!r.snappedX && r.rect.x == 101 && r.rect.w == 2);
        // 왼쪽 테두리도 같음
        const std::vector<double> rx{110};
        r = snapRect(RectF{105, 0, 3, 10}, false, SnapLeft, rx, {}, 8);
        CHECK(!r.snappedX && r.rect.x == 105 && r.rect.w == 3);
        // 붙어도 1px 이상 남으면 붙음
        r = snapRect(RectF{95, 0, 3, 10}, false, SnapRight, lx, {}, 8);
        CHECK(r.snappedX && r.rect.x == 95 && r.rect.right() == 100);
    }
}

int main()
{
    testSnap();
    testStretch();
    testFit();
    testFill();
    testOriginal();
    testInvalid();
    testClip();
    testTickerPositions();
    testDrawItems();
    return test::finish("test_geometry");
}
