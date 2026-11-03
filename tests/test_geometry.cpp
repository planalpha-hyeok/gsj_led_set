// 맞춤 방식 계산과 출력 좌표 변환 검사

#include "TestUtil.h"

#include "core/Geometry.h"
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
    // 1000x500 영역에 1000x1000 콘텐츠 -> 500x500, 가로 가운데
    const Placement p = computePlacement(RectI{0, 0, 1000, 500}, 1000, 1000, FitMode::Fit);
    CHECK(near(p.dst.w, 500) && near(p.dst.h, 500));
    CHECK(near(p.dst.x, 250) && near(p.dst.y, 0));
    CHECK(near(p.uv.w, 1) && near(p.uv.h, 1));
}

static void testFill()
{
    // 1000x500 영역에 1000x1000 콘텐츠 -> 위아래 25%씩 잘림
    const Placement p = computePlacement(RectI{10, 20, 1000, 500}, 1000, 1000, FitMode::Fill);
    CHECK(near(p.dst.x, 10) && near(p.dst.y, 20) && near(p.dst.w, 1000) && near(p.dst.h, 500));
    CHECK(near(p.uv.x, 0) && near(p.uv.y, 0.25) && near(p.uv.w, 1) && near(p.uv.h, 0.5));
}

static void testOriginal()
{
    // 콘텐츠가 영역보다 큼 -> 좌상단부터 영역 크기만큼
    Placement p = computePlacement(RectI{0, 0, 960, 540}, 1920, 1080, FitMode::Original);
    CHECK(near(p.dst.w, 960) && near(p.dst.h, 540));
    CHECK(near(p.uv.x, 0) && near(p.uv.y, 0) && near(p.uv.w, 0.5) && near(p.uv.h, 0.5));

    // 콘텐츠가 영역보다 작음 -> 원본 크기 그대로
    p = computePlacement(RectI{100, 100, 1920, 1080}, 640, 360, FitMode::Original);
    CHECK(near(p.dst.x, 100) && near(p.dst.y, 100) && near(p.dst.w, 640) && near(p.dst.h, 360));
    CHECK(near(p.uv.w, 1) && near(p.uv.h, 1));
}

static void testInvalid()
{
    CHECK(!computePlacement(RectI{0, 0, 0, 100}, 100, 100, FitMode::Stretch).visible);
    CHECK(!computePlacement(RectI{0, 0, 100, 100}, 0, 100, FitMode::Fit).visible);
}

static void testDrawItems()
{
    // 캔버스 3840x1080, 출력2 = 오른쪽 절반
    auto frame = std::make_shared<Frame>();
    frame->width = 16;
    frame->height = 16;

    std::vector<SceneLayer> layers(3);
    layers[0].key = 1; // 왼쪽 절반에만 있음 -> 제외
    layers[0].dst = RectF{0, 0, 1920, 1080};
    layers[1].key = 2; // 두 출력에 걸침
    layers[1].dst = RectF{1000, 100, 2000, 500};
    layers[2].key = 3; // 투명도 0 -> 제외
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

    // 경계에 딱 붙은 레이어(겹치지 않음)는 제외
    std::vector<SceneLayer> edge(1);
    edge[0].dst = RectF{0, 0, 1920, 1080};
    edge[0].frame = frame;
    buildDrawItems(edge, RectI{1920, 0, 1920, 1080}, items);
    CHECK(items.empty());
}

int main()
{
    testStretch();
    testFit();
    testFill();
    testOriginal();
    testInvalid();
    testDrawItems();
    return test::finish("test_geometry");
}
