// 프로젝트 파일 저장/불러오기 검사

#include "TestUtil.h"

#include "config/ProjectSerializer.h"

using namespace led;

static Project makeSample()
{
    Project p;
    p.canvas = CanvasConfig{7680, 2160};

    Region r;
    r.id = "r1";
    r.name = "메인 화면";
    r.rect = RectI{0, 0, 3840, 2160};
    r.source = "C:/콘텐츠/메인 영상.mp4";
    r.fit = FitMode::Fill;
    r.loop = false;
    r.opacity = 0.75;
    r.visible = true;
    p.regions.push_back(r);

    Region r2 = r;
    r2.id = "r2";
    r2.name = "측면";
    r2.rect = RectI{3840, 0, 1920, 1080};
    r2.source.clear();
    r2.fit = FitMode::Original;
    r2.visible = false;
    p.regions.push_back(r2);

    OutputConfig o;
    o.id = "o1";
    o.name = "노바스타 1";
    o.displayId = "\\\\.\\DISPLAY2";
    o.canvasRect = RectI{0, 0, 3840, 2160};
    o.offsetX = 10;
    o.offsetY = -5;
    o.enabled = false;
    p.outputs.push_back(o);
    return p;
}

static void testRoundTrip()
{
    const Project original = makeSample();
    Project loaded;
    QString error;
    CHECK(projectFromJson(projectToJson(original), loaded, &error));

    CHECK(loaded.canvas.width == 7680 && loaded.canvas.height == 2160);
    CHECK(loaded.regions.size() == 2);
    CHECK(loaded.outputs.size() == 1);
    if (loaded.regions.size() == 2) {
        const Region& a = loaded.regions[0];
        CHECK(a.id == "r1");
        CHECK(a.name == "메인 화면");
        CHECK(a.rect == (RectI{0, 0, 3840, 2160}));
        CHECK(a.source == "C:/콘텐츠/메인 영상.mp4");
        CHECK(a.fit == FitMode::Fill);
        CHECK(!a.loop);
        CHECK(test::near(a.opacity, 0.75));
        const Region& b = loaded.regions[1];
        CHECK(b.fit == FitMode::Original);
        CHECK(!b.visible);
        CHECK(b.source.empty());
    }
    if (loaded.outputs.size() == 1) {
        const OutputConfig& o = loaded.outputs[0];
        CHECK(o.name == "노바스타 1");
        CHECK(o.displayId == "\\\\.\\DISPLAY2");
        CHECK(o.canvasRect == (RectI{0, 0, 3840, 2160}));
        CHECK(o.offsetX == 10 && o.offsetY == -5);
        CHECK(!o.enabled);
    }
}

static void testInvalidInput()
{
    Project p;
    QString error;
    CHECK(!projectFromJson("not json", p, &error));
    CHECK(!error.isEmpty());

    error.clear();
    CHECK(!projectFromJson(R"({"version": 99, "canvas": {"width": 100, "height": 100}})", p, &error));
    CHECK(!error.isEmpty());

    error.clear();
    CHECK(!projectFromJson(R"({"version": 1, "canvas": {"width": 0, "height": 100}})", p, &error));

    error.clear();
    CHECK(!projectFromJson(
        R"({"version": 1, "canvas": {"width": 100, "height": 100},
            "regions": [{"name": "a", "x": 0, "y": 0, "width": 0, "height": 10}]})",
        p, &error));
}

static void testMissingIds()
{
    Project p;
    QString error;
    CHECK(projectFromJson(
        R"({"version": 1, "canvas": {"width": 100, "height": 100},
            "regions": [{"name": "a", "x": 0, "y": 0, "width": 10, "height": 10, "fit": "unknown"}]})",
        p, &error));
    CHECK(p.regions.size() == 1);
    if (!p.regions.empty()) {
        CHECK(p.regions[0].id.size() == 16);
        CHECK(p.regions[0].fit == FitMode::Stretch);
    }
}

int main()
{
    testRoundTrip();
    testInvalidInput();
    testMissingIds();
    return test::finish("test_serializer");
}
