// 프로젝트 파일 저장/불러오기 검사 (버전 2 + 버전 1 자동 변환)

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
    r.fit = FitMode::Fill;
    r.opacity = 0.75;
    p.regions.push_back(r);

    Region r2;
    r2.id = "r2";
    r2.name = "하단 자막";
    r2.rect = RectI{0, 2000, 7680, 160};
    r2.fit = FitMode::Original;
    r2.visible = false;
    r2.decode = DecodeMode::Software;
    p.regions.push_back(r2);
    p.hardwareDecoding = false;
    p.audioDevice = "스피커 (USB Audio)";

    Scene s;
    s.id = "s1";
    s.name = "오전";
    RegionContent c;
    c.regionId = "r1";
    c.loopPlaylist = false;
    PlaylistItem a;
    a.id = "i1";
    a.source = "C:/콘텐츠/메인 영상.mp4";
    PlaylistItem b;
    b.id = "i2";
    b.source = "C:/콘텐츠/포스터.png";
    b.duration = 7.5;
    b.transition = TransitionType::Fade;
    b.transitionDuration = 2.0;
    c.items = {a, b};
    s.contents.push_back(c);

    RegionContent t;
    t.regionId = "r2";
    t.kind = ContentKind::Ticker;
    t.ticker.lines = {"환영합니다", "오늘 행사 안내"};
    t.ticker.textFile = "C:/자막/공지.txt";
    t.ticker.fontFamily = "맑은 고딕";
    t.ticker.fontSize = 120;
    t.ticker.bold = false;
    t.ticker.textColor = 0xFFFFCC00u;
    t.ticker.backgroundColor = 0x80000000u;
    t.ticker.outlineWidth = 3;
    t.ticker.outlineColor = 0xFF102030u;
    t.ticker.shadow = true;
    t.ticker.shadowOffset = 6;
    t.ticker.shadowColor = 0x40000000u;
    t.ticker.speed = 240.0;
    t.ticker.direction = TickerDirection::Up;
    t.ticker.gap = 333;
    s.contents.push_back(t);
    p.scenes.push_back(s);

    Scene s2;
    s2.id = "s2";
    s2.name = "오후";
    p.scenes.push_back(s2);

    Scene s3;
    s3.id = "s3";
    s3.name = "공연";
    s3.kind = SceneKind::Timeline;
    TimelineLayer layer;
    layer.id = "L1";
    layer.name = "배경";
    layer.regionId = "r1";
    layer.muted = true;
    layer.locked = true;
    TimelineClip clip;
    clip.id = "k1";
    clip.source = "C:/공연/오프닝.mp4";
    clip.start = 1.5;
    clip.duration = 12.25;
    clip.inPoint = 3.0;
    clip.fadeIn = 0.5;
    clip.fadeOut = 1.0;
    clip.opacity = 0.8;
    clip.volume = 0.6;
    clip.loop = true;
    layer.clips = {clip};
    TimelineLayer soundOnly;
    soundOnly.id = "L2";
    soundOnly.name = "음악";
    soundOnly.visible = false;
    s3.timeline.layers = {layer, soundOnly};
    TimelineCue cue;
    cue.id = "q1";
    cue.name = "막 1";
    cue.time = 30.0;
    s3.timeline.cues = {cue};
    s3.timeline.endAction = TimelineEnd::Loop;
    p.scenes.push_back(s3);

    p.schedule.enabled = true;
    p.schedule.outside = OutsideBehavior::DefaultScene;
    p.schedule.defaultSceneId = "s2";
    ScheduleEntry e;
    e.id = "e1";
    e.name = "평일 오전";
    e.sceneId = "s1";
    e.weekdays = 0x1F;
    e.startMinute = 9 * 60 + 30;
    e.endMinute = 12 * 60;
    p.schedule.entries.push_back(e);
    ScheduleEntry e2 = e;
    e2.id = "e2";
    e2.name = "행사일";
    e2.date = "2026-12-24";
    e2.enabled = false;
    e2.startMinute = 22 * 60;
    e2.endMinute = 2 * 60;
    p.schedule.entries.push_back(e2);
    p.autoStart = true;

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
    if (loaded.regions.size() == 2) {
        CHECK(loaded.regions[0].name == "메인 화면");
        CHECK(loaded.regions[0].fit == FitMode::Fill);
        CHECK(test::near(loaded.regions[0].opacity, 0.75));
        CHECK(!loaded.regions[1].visible);
        CHECK(loaded.regions[0].decode == DecodeMode::Default);
        CHECK(loaded.regions[1].decode == DecodeMode::Software);
    }

    CHECK(loaded.scenes.size() == 3);
    if (loaded.scenes.size() == 3) {
        const Scene& t = loaded.scenes[2];
        CHECK(t.kind == SceneKind::Timeline);
        CHECK(t.timeline == original.scenes[2].timeline);
        CHECK(t.timeline.endAction == TimelineEnd::Loop);
        CHECK(t.timeline.layers.size() == 2 && t.timeline.layers[1].regionId.empty());
        CHECK(loaded.scenes[0].kind == SceneKind::Regions);
    }
    if (loaded.scenes.size() == 3) {
        const Scene& s = loaded.scenes[0];
        CHECK(s.name == "오전");
        const RegionContent* c = s.contentFor("r1");
        CHECK(c != nullptr);
        if (c) {
            CHECK(*c == original.scenes[0].contents[0]);
            CHECK(!c->loopPlaylist);
            CHECK(c->items.size() == 2);
        }
        const RegionContent* t = s.contentFor("r2");
        CHECK(t != nullptr);
        if (t) {
            CHECK(t->kind == ContentKind::Ticker);
            CHECK(t->ticker == original.scenes[0].contents[1].ticker);
        }
    }

    CHECK(loaded.schedule.enabled);
    CHECK(loaded.schedule.outside == OutsideBehavior::DefaultScene);
    CHECK(loaded.schedule.defaultSceneId == "s2");
    CHECK(loaded.schedule.entries.size() == 2);
    if (loaded.schedule.entries.size() == 2) {
        const ScheduleEntry& a = loaded.schedule.entries[0];
        CHECK(a.weekdays == 0x1F && a.startMinute == 570 && a.endMinute == 720 && a.date.empty());
        const ScheduleEntry& b = loaded.schedule.entries[1];
        CHECK(b.date == "2026-12-24" && !b.enabled && b.startMinute == 1320 && b.endMinute == 120);
    }
    CHECK(loaded.autoStart);
    CHECK(!loaded.hardwareDecoding);
    CHECK(loaded.audioDevice == "스피커 (USB Audio)");

    CHECK(loaded.outputs.size() == 1);
    if (loaded.outputs.size() == 1) {
        const OutputConfig& o = loaded.outputs[0];
        CHECK(o.name == "노바스타 1");
        CHECK(o.displayId == "\\\\.\\DISPLAY2");
        CHECK(o.canvasRect == (RectI{0, 0, 3840, 2160}));
        CHECK(o.offsetX == 10 && o.offsetY == -5);
        CHECK(!o.enabled);
    }
}

static void testVersion1Migration()
{
    // 1단계에서 저장한 파일
    const char* v1 = R"({
        "version": 1,
        "canvas": {"width": 3840, "height": 1080},
        "regions": [
            {"id": "a", "name": "왼쪽", "x": 0, "y": 0, "width": 1920, "height": 1080,
             "source": "C:/a.mp4", "fit": "fit", "loop": false, "opacity": 1, "visible": true},
            {"id": "b", "name": "오른쪽", "x": 1920, "y": 0, "width": 1920, "height": 1080,
             "source": "", "fit": "stretch", "loop": true, "opacity": 0.5, "visible": true}
        ],
        "outputs": [{"id": "o", "name": "출력", "display": "X",
                     "canvasRect": {"x": 0, "y": 0, "width": 3840, "height": 1080}}]
    })";
    Project p;
    QString error;
    CHECK(projectFromJson(v1, p, &error));
    CHECK(p.regions.size() == 2);
    CHECK(p.scenes.size() == 1);
    if (p.scenes.size() == 1) {
        const Scene& s = p.scenes[0];
        CHECK(s.contents.size() == 1); // 콘텐츠 없는 영역은 제외
        const RegionContent* c = s.contentFor("a");
        CHECK(c != nullptr);
        if (c) {
            CHECK(c->kind == ContentKind::Playlist);
            CHECK(c->items.size() == 1);
            CHECK(!c->loopPlaylist);
            if (!c->items.empty())
                CHECK(c->items[0].source == "C:/a.mp4");
        }
    }
    if (p.regions.size() == 2) {
        CHECK(p.regions[0].fit == FitMode::Fit);
        CHECK(test::near(p.regions[1].opacity, 0.5));
    }
    CHECK(!p.schedule.enabled);
    CHECK(p.hardwareDecoding); // 예전 파일: 기본값 켜짐
    if (!p.regions.empty())
        CHECK(p.regions[0].decode == DecodeMode::Default);
}

static void testEffectiveDecode()
{
    Project p;
    Region r;
    p.hardwareDecoding = true;
    CHECK(effectiveHardwareDecoding(p, r));
    p.hardwareDecoding = false;
    CHECK(!effectiveHardwareDecoding(p, r));
    r.decode = DecodeMode::Hardware;
    CHECK(effectiveHardwareDecoding(p, r));
    p.hardwareDecoding = true;
    r.decode = DecodeMode::Software;
    CHECK(!effectiveHardwareDecoding(p, r));
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

    CHECK(!projectFromJson(R"({"version": 2, "canvas": {"width": 0, "height": 100}})", p, &error));
    CHECK(!projectFromJson(
        R"({"version": 2, "canvas": {"width": 100, "height": 100},
            "regions": [{"name": "a", "x": 0, "y": 0, "width": 0, "height": 10}]})",
        p, &error));
    CHECK(!projectFromJson(
        R"({"version": 2, "canvas": {"width": 100, "height": 100},
            "schedule": {"entries": [{"name": "x", "start": "25:00", "end": "10:00"}]}})",
        p, &error));
}

static void testNormalize()
{
    Project p;
    QString error;
    // 장면 없음 -> 기본 장면 생성, 없는 영역을 가리키는 콘텐츠 제거
    CHECK(projectFromJson(
        R"({"version": 2, "canvas": {"width": 100, "height": 100},
            "regions": [{"id": "r", "name": "a", "x": 0, "y": 0, "width": 10, "height": 10, "fit": "unknown"}],
            "scenes": []})",
        p, &error));
    CHECK(p.scenes.size() == 1);
    CHECK(p.regions.size() == 1 && p.regions[0].fit == FitMode::Stretch);

    CHECK(projectFromJson(
        R"({"version": 2, "canvas": {"width": 100, "height": 100},
            "regions": [{"id": "r", "name": "a", "x": 0, "y": 0, "width": 10, "height": 10}],
            "scenes": [{"id": "s", "name": "x", "contents": [{"regionId": "zzz", "kind": "playlist"},
                                                               {"regionId": "r", "kind": "ticker"}]}]})",
        p, &error));
    CHECK(p.scenes.size() == 1 && p.scenes[0].contents.size() == 1);
    if (!p.scenes.empty() && !p.scenes[0].contents.empty())
        CHECK(p.scenes[0].contents[0].kind == ContentKind::Ticker);
}

static void testHelpers()
{
    std::uint32_t c = 0;
    CHECK(colorFromString("#80FF0010", c) && c == 0x80FF0010u);
    CHECK(colorFromString("#ff0010", c) && c == 0xFFFF0010u);
    CHECK(!colorFromString("FF0010", c));
    CHECK(!colorFromString("#GG0010", c));
    CHECK(colorToString(0x0A0B0C0Du) == "#0A0B0C0D");

    int m = -1;
    CHECK(minuteFromString("00:00", m) && m == 0);
    CHECK(minuteFromString("23:59", m) && m == 1439);
    CHECK(!minuteFromString("24:00", m));
    CHECK(!minuteFromString("9:60", m));
    CHECK(!minuteFromString("09:00x", m));
    CHECK(minuteToString(9 * 60 + 5) == "09:05");
}

int main()
{
    testRoundTrip();
    testVersion1Migration();
    testInvalidInput();
    testNormalize();
    testHelpers();
    testEffectiveDecode();
    return test::finish("test_serializer");
}
