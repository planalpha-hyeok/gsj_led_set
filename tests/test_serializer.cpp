// 프로젝트 파일 저장/불러오기 검사 (버전 3 + 버전 2/1 자동 변환)

#include "TestUtil.h"

#include "config/ProjectSerializer.h"
#include "core/LayerContent.h"

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

    // 장면 1: 영역별 플레이리스트/자막 (= 영역의 레이어)
    Scene s;
    s.id = "s1";
    s.name = "오전";
    TimelineLayer playlist;
    playlist.id = "P1";
    playlist.name = "메인 화면";
    playlist.regionId = "r1";
    playlist.repeat = false;
    playlist.holdLast = true;
    TimelineClip a;
    a.id = "i1";
    a.source = "C:/콘텐츠/메인 영상.mp4";
    a.autoDuration = true;
    a.duration = 20.0;
    TimelineClip b;
    b.id = "i2";
    b.source = "C:/콘텐츠/포스터.png";
    b.start = 18.0;
    b.duration = 7.5;
    b.fadeIn = 2.0;
    playlist.clips = {a, b};

    TimelineLayer tickerLayer;
    tickerLayer.id = "T1";
    tickerLayer.name = "하단 자막";
    tickerLayer.regionId = "r2";
    tickerLayer.repeat = true;
    TimelineClip t;
    t.id = "tk";
    t.kind = ClipKind::Ticker;
    t.duration = 60.0;
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
    tickerLayer.clips = {t};

    TimelineLayer live;
    live.id = "LV";
    live.name = "카메라";
    live.regionId = "r1";
    live.repeat = true;
    TimelineClip cam;
    cam.id = "cam";
    cam.kind = ClipKind::Live;
    cam.source = "USB Capture HDMI";
    cam.liveSize = "1920x1080";
    cam.liveFps = 59.94;
    cam.duration = 3600.0;
    live.clips = {cam};
    s.timeline.layers = {playlist, tickerLayer, live};
    p.scenes.push_back(s);

    Scene s2;
    s2.id = "s2";
    s2.name = "오후";
    p.scenes.push_back(s2);

    // 장면 3: 큐 타임라인
    Scene s3;
    s3.id = "s3";
    s3.name = "공연";
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
        for (std::size_t i = 0; i < 3; ++i)
            CHECK(loaded.scenes[i].timeline == original.scenes[i].timeline);
        const Scene& t = loaded.scenes[2];
        CHECK(t.timeline.endAction == TimelineEnd::Loop);
        CHECK(t.timeline.layers.size() == 2 && t.timeline.layers[1].regionId.empty());

        const Timeline& tl = loaded.scenes[0].timeline;
        CHECK(tl.layers.size() == 3);
        if (tl.layers.size() == 3) {
            CHECK(tl.layers[0].holdLast && !tl.layers[0].repeat);
            CHECK(tl.layers[0].clips.size() == 2 && tl.layers[0].clips[0].autoDuration);
            CHECK(tl.layers[1].repeat && tl.layers[1].clips.size() == 1);
            if (!tl.layers[1].clips.empty()) {
                CHECK(tl.layers[1].clips[0].kind == ClipKind::Ticker);
                CHECK(tl.layers[1].clips[0].ticker == original.scenes[0].timeline.layers[1].clips[0].ticker);
            }
            if (!tl.layers[2].clips.empty()) {
                const TimelineClip& c = tl.layers[2].clips[0];
                CHECK(c.kind == ClipKind::Live && c.liveSize == "1920x1080" && test::near(c.liveFps, 59.94));
            }
            // 플레이리스트 화면으로 보기
            const RegionContent rc = layerToContent(&tl.layers[0], "r1");
            CHECK(!rc.loopPlaylist && rc.items.size() == 2);
            if (rc.items.size() == 2) {
                CHECK(rc.items[0].duration == 0.0);   // 자동
                CHECK(rc.items[1].transition == TransitionType::Fade && test::near(rc.items[1].transitionDuration, 2.0));
            }
            CHECK(layerToContent(&tl.layers[1], "r2").kind == ContentKind::Ticker);
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
        const Timeline& tl = p.scenes[0].timeline;
        CHECK(tl.layers.size() == 1); // 콘텐츠 없는 영역은 제외
        if (tl.layers.size() == 1) {
            const TimelineLayer& l = tl.layers[0];
            CHECK(l.regionId == "a" && l.name == "왼쪽");
            CHECK(!l.repeat && l.holdLast && l.needsPacking && l.muted);
            CHECK(l.clips.size() == 1);
            if (!l.clips.empty()) {
                CHECK(l.clips[0].source == "C:/a.mp4");
                CHECK(l.clips[0].autoDuration);
            }
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

    CHECK(!projectFromJson(R"({"version": 3, "canvas": {"width": 0, "height": 100}})", p, &error));
    CHECK(!projectFromJson(
        R"({"version": 2, "canvas": {"width": 100, "height": 100},
            "regions": [{"name": "a", "x": 0, "y": 0, "width": 0, "height": 10}]})",
        p, &error));
    CHECK(!projectFromJson(
        R"({"version": 2, "canvas": {"width": 100, "height": 100},
            "schedule": {"entries": [{"name": "x", "start": "25:00", "end": "10:00"}]}})",
        p, &error));
}

static void testVersion2Migration()
{
    // 버전 2: 영역별 콘텐츠 장면 + 타임라인 장면
    const char* v2 = R"({
        "version": 2,
        "canvas": {"width": 1920, "height": 1080},
        "regions": [
            {"id": "r1", "name": "메인", "x": 0, "y": 0, "width": 1920, "height": 900},
            {"id": "r2", "name": "자막", "x": 0, "y": 900, "width": 1920, "height": 180}
        ],
        "scenes": [
            {"id": "s1", "name": "기본", "kind": "regions", "contents": [
                {"regionId": "r2", "kind": "ticker", "loop": true, "items": [],
                 "ticker": {"lines": ["안녕"], "speed": 99}},
                {"regionId": "r1", "kind": "playlist", "loop": true, "items": [
                    {"id": "a", "type": "file", "source": "C:/a.mp4", "duration": 0},
                    {"id": "b", "type": "file", "source": "C:/b.png", "duration": 4,
                     "transition": "fade", "transitionDuration": 1},
                    {"id": "c", "type": "live", "source": "Cam", "liveSize": "1280x720", "liveFps": 30,
                     "duration": 6}
                ]},
                {"regionId": "zzz", "kind": "playlist", "items": [{"id": "x", "source": "C:/x.mp4"}]}
            ]},
            {"id": "s2", "name": "공연", "kind": "timeline", "timeline": {
                "endAction": "stop",
                "layers": [{"id": "L", "name": "배경", "regionId": "r1",
                            "clips": [{"id": "k", "source": "C:/k.mp4", "start": 2, "duration": 3}]}],
                "cues": []}}
        ]
    })";
    Project p;
    QString error;
    CHECK(projectFromJson(v2, p, &error));
    CHECK(p.scenes.size() == 2);
    if (p.scenes.size() != 2)
        return;

    // 영역 순서대로 레이어 (없는 영역 콘텐츠는 제외)
    const Timeline& tl = p.scenes[0].timeline;
    CHECK(tl.layers.size() == 2);
    if (tl.layers.size() == 2) {
        const TimelineLayer& pl = tl.layers[0];
        CHECK(pl.regionId == "r1" && pl.repeat && !pl.holdLast && pl.needsPacking && pl.muted);
        CHECK(pl.clips.size() == 3);
        if (pl.clips.size() == 3) {
            CHECK(pl.clips[0].autoDuration);
            CHECK(!pl.clips[1].autoDuration && test::near(pl.clips[1].duration, 4.0) && test::near(pl.clips[1].fadeIn, 1.0));
            CHECK(pl.clips[2].kind == ClipKind::Live && pl.clips[2].liveSize == "1280x720");
            // 실제 길이로 다시 배치 (영상 = 8초로 가정)
            TimelineLayer packed = pl;
            packLayer(packed, [](const TimelineClip& c) { return c.source == "C:/a.mp4" ? 8.0 : 0.0; });
            CHECK(test::near(packed.clips[0].duration, 8.0));
            CHECK(test::near(packed.clips[1].start, 7.0));  // 페이드 1초 겹침
            CHECK(test::near(packed.clips[2].start, 11.0));
            CHECK(!packed.needsPacking);
            CHECK(isPlaylistCompatible(packed));
        }
        const TimelineLayer& tk = tl.layers[1];
        CHECK(tk.regionId == "r2" && tk.repeat && tk.clips.size() == 1);
        if (!tk.clips.empty()) {
            CHECK(tk.clips[0].kind == ClipKind::Ticker);
            CHECK(tk.clips[0].ticker.lines.size() == 1 && test::near(tk.clips[0].ticker.speed, 99.0));
        }
    }
    CHECK(tl.length() == 0.0); // 반복 레이어만 있음: 끝없이 재생

    const Timeline& t2 = p.scenes[1].timeline;
    CHECK(t2.endAction == TimelineEnd::Stop);
    CHECK(t2.layers.size() == 1 && t2.layers[0].clips.size() == 1 && !t2.layers[0].repeat);
    CHECK(test::near(t2.length(), 5.0));
}

static void testNormalize()
{
    Project p;
    QString error;
    // 장면 없음 -> 기본 장면 생성
    CHECK(projectFromJson(
        R"({"version": 3, "canvas": {"width": 100, "height": 100},
            "regions": [{"id": "r", "name": "a", "x": 0, "y": 0, "width": 10, "height": 10, "fit": "unknown"}],
            "scenes": []})",
        p, &error));
    CHECK(p.scenes.size() == 1);
    CHECK(p.regions.size() == 1 && p.regions[0].fit == FitMode::Stretch);

    // 없는 영역을 가리키는 레이어 -> 소리 전용
    CHECK(projectFromJson(
        R"({"version": 3, "canvas": {"width": 100, "height": 100},
            "regions": [{"id": "r", "name": "a", "x": 0, "y": 0, "width": 10, "height": 10}],
            "scenes": [{"id": "s", "name": "x", "timeline": {"layers": [
                {"id": "1", "regionId": "zzz", "clips": []},
                {"id": "2", "regionId": "r", "repeat": true, "holdLast": true,
                 "clips": [{"id": "c", "kind": "bogus", "source": "a.mp4"}]}]}}]})",
        p, &error));
    CHECK(p.scenes.size() == 1 && p.scenes[0].timeline.layers.size() == 2);
    if (p.scenes.size() == 1 && p.scenes[0].timeline.layers.size() == 2) {
        const auto& layers = p.scenes[0].timeline.layers;
        CHECK(layers[0].regionId.empty());
        CHECK(layers[1].regionId == "r");
        CHECK(layers[1].repeat && !layers[1].holdLast); // 반복이면 마지막 화면 유지는 무시
        CHECK(!layers[1].clips.empty() && layers[1].clips[0].kind == ClipKind::File);
    }
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
    testVersion2Migration();
    testInvalidInput();
    testNormalize();
    testHelpers();
    testEffectiveDecode();
    return test::finish("test_serializer");
}
