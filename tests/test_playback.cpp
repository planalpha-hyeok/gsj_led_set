// 재생 엔진 검사: 플레이리스트 전환(컷/페이드), 목록 반복, 자막, 재생 중 편집, 영상 끝 감지
// 사용법: test_playback [영상 파일(선택)]

#include "TestUtil.h"

#include "playback/PlaybackEngine.h"

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>

#include <chrono>
#include <cstdio>
#include <functional>
#include <thread>

using namespace led;

namespace {

std::string makeImage(const QTemporaryDir& dir, const char* name, QColor color)
{
    QImage img(64, 32, QImage::Format_RGB32);
    img.fill(color);
    const QString path = dir.filePath(QString::fromLatin1(name));
    img.save(path);
    return path.toStdString();
}

// 프레임 가운데 픽셀의 빨강/파랑 값으로 어떤 이미지인지 판단
char frameColor(const Frame& f)
{
    const std::size_t i = static_cast<std::size_t>(f.height / 2) * f.stride + static_cast<std::size_t>(f.width / 2) * 4;
    const int b = f.pixels[i];
    const int r = f.pixels[i + 2];
    if (r > 200 && b < 50)
        return 'R';
    if (b > 200 && r < 50)
        return 'B';
    return '?';
}

// 조건이 맞을 때까지 기다림 (최대 timeoutMs)
bool waitFor(PlaybackEngine& engine, int timeoutMs, const std::function<bool(const std::vector<SceneLayer>&)>& pred)
{
    std::vector<SceneLayer> layers;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        engine.collect(layers);
        if (pred(layers))
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

Project baseProject()
{
    Project p;
    p.canvas = CanvasConfig{800, 200};
    Region r;
    r.id = "r";
    r.name = "영역";
    r.rect = RectI{0, 0, 400, 100};
    p.regions.push_back(r);
    Scene s;
    s.id = "s";
    s.name = "장면";
    p.scenes.push_back(s);
    return p;
}

PlaylistItem item(const std::string& path, double duration, TransitionType t = TransitionType::Cut, double fade = 0.5)
{
    PlaylistItem it;
    it.id = generateId();
    it.source = path;
    it.duration = duration;
    it.transition = t;
    it.transitionDuration = fade;
    return it;
}

bool singleColor(const std::vector<SceneLayer>& l, char c)
{
    return l.size() == 1 && l[0].frame && frameColor(*l[0].frame) == c && l[0].opacity > 0.99f;
}

} // namespace

static void testCutLoop(const std::string& red, const std::string& blue)
{
    Project p = baseProject();
    RegionContent c;
    c.regionId = "r";
    c.items = {item(red, 0.3), item(blue, 0.3)};
    p.scenes[0].contents.push_back(c);

    PlaybackEngine engine;
    std::vector<std::string> errors;
    engine.play(p, "s", errors);
    CHECK(errors.empty());
    CHECK(engine.isPlaying());
    CHECK(engine.sceneId() == "s");

    CHECK(waitFor(engine, 200, [](const auto& l) { return singleColor(l, 'R'); }));
    CHECK(waitFor(engine, 1000, [](const auto& l) { return singleColor(l, 'B'); }));
    CHECK(waitFor(engine, 1000, [](const auto& l) { return singleColor(l, 'R'); })); // 목록 반복
    engine.stop();
    CHECK(!engine.isPlaying());
    std::vector<SceneLayer> layers;
    engine.collect(layers);
    CHECK(layers.empty());
}

static void testFade(const std::string& red, const std::string& blue)
{
    Project p = baseProject();
    RegionContent c;
    c.regionId = "r";
    c.items = {item(red, 1.0, TransitionType::Cut), item(blue, 1.0, TransitionType::Fade, 0.4)};
    p.scenes[0].contents.push_back(c);

    PlaybackEngine engine;
    std::vector<std::string> errors;
    engine.play(p, "s", errors);

    // 페이드 중: 아래 빨강(불투명) + 위 파랑(반투명)
    CHECK(waitFor(engine, 2000, [](const auto& l) {
        return l.size() == 2 && frameColor(*l[0].frame) == 'R' && frameColor(*l[1].frame) == 'B' &&
               l[0].opacity > 0.99f && l[1].opacity > 0.05f && l[1].opacity < 0.95f;
    }));
    // 페이드 끝: 파랑만
    CHECK(waitFor(engine, 1000, [](const auto& l) { return singleColor(l, 'B'); }));
    engine.stop();
}

static void testNoLoopHoldsLast(const std::string& red, const std::string& blue)
{
    Project p = baseProject();
    RegionContent c;
    c.regionId = "r";
    c.loopPlaylist = false;
    c.items = {item(red, 0.2), item(blue, 0.2)};
    p.scenes[0].contents.push_back(c);

    PlaybackEngine engine;
    std::vector<std::string> errors;
    engine.play(p, "s", errors);
    CHECK(waitFor(engine, 1000, [](const auto& l) { return singleColor(l, 'B'); }));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    CHECK(waitFor(engine, 50, [](const auto& l) { return singleColor(l, 'B'); })); // 마지막 유지
    engine.stop();
}

static void testMissingFileSkipped(const std::string& red, const std::string& blue)
{
    Project p = baseProject();
    RegionContent c;
    c.regionId = "r";
    c.items = {item("__없는파일__.png", 0.2), item(red, 0.2), item(blue, 0.2)};
    p.scenes[0].contents.push_back(c);

    PlaybackEngine engine;
    std::vector<std::string> errors;
    engine.play(p, "s", errors);
    CHECK(errors.size() == 1);   // 첫 항목 실패 보고
    CHECK(waitFor(engine, 200, [](const auto& l) { return singleColor(l, 'R'); }));
    CHECK(waitFor(engine, 1000, [](const auto& l) { return singleColor(l, 'B'); }));
    CHECK(waitFor(engine, 1500, [](const auto& l) { return singleColor(l, 'R'); })); // 실패 항목 건너뜀
    engine.stop();
}

static void testLiveUpdate(const std::string& red, const std::string& blue)
{
    Project p = baseProject();
    RegionContent c;
    c.regionId = "r";
    c.items = {item(red, 0.0)};
    p.scenes[0].contents.push_back(c);

    PlaybackEngine engine;
    std::vector<std::string> errors;
    engine.play(p, "s", errors);
    std::vector<SceneLayer> before;
    CHECK(waitFor(engine, 200, [&](const auto& l) {
        before = l;
        return singleColor(l, 'R');
    }));

    // 위치만 바꾸면 같은 재생이 이어짐 (텍스처 키 동일)
    p.regions[0].rect = RectI{100, 50, 200, 100};
    engine.update(p, errors);
    std::vector<SceneLayer> after;
    CHECK(waitFor(engine, 200, [&](const auto& l) {
        after = l;
        return l.size() == 1 && test::near(l[0].dst.x, 100) && test::near(l[0].dst.w, 200);
    }));
    if (!before.empty() && !after.empty())
        CHECK(before[0].key == after[0].key);

    // 콘텐츠를 바꾸면 새 소스로 교체
    p.scenes[0].contents[0].items = {item(blue, 0.0)};
    engine.update(p, errors);
    CHECK(waitFor(engine, 500, [](const auto& l) { return singleColor(l, 'B'); }));

    // 숨기면 사라짐
    p.regions[0].visible = false;
    engine.update(p, errors);
    CHECK(waitFor(engine, 200, [](const auto& l) { return l.empty(); }));

    // 장면 삭제 시 정지
    p.scenes.clear();
    engine.update(p, errors);
    CHECK(!engine.isPlaying());
}

static void testTicker()
{
    Project p = baseProject();
    RegionContent c;
    c.regionId = "r";
    c.kind = ContentKind::Ticker;
    c.ticker.lines = {"자막 테스트 ABC", "두 번째 문구"};
    c.ticker.fontSize = 40;
    c.ticker.outlineWidth = 2;
    c.ticker.shadow = true;
    c.ticker.speed = 2000;
    c.ticker.backgroundColor = 0x80000000u;
    p.scenes[0].contents.push_back(c);

    PlaybackEngine engine;
    std::vector<std::string> errors;
    engine.play(p, "s", errors);
    CHECK(errors.empty());

    // 배경 1개 + 자막 조각, 모두 영역(0,0,400,100) 안으로 잘려 있어야 함
    CHECK(waitFor(engine, 1500, [](const auto& l) {
        if (l.size() < 2)
            return false;
        for (const SceneLayer& s : l)
            if (s.dst.x < -0.001 || s.dst.y < -0.001 || s.dst.x + s.dst.w > 400.001 || s.dst.y + s.dst.h > 100.001)
                return false;
        return true;
    }));

    // 세로 방향
    p.scenes[0].contents[0].ticker.direction = TickerDirection::Up;
    engine.update(p, errors);
    CHECK(waitFor(engine, 1500, [](const auto& l) { return l.size() >= 2; }));
    engine.stop();
}

static void testSharedFrame(const std::string& red)
{
    // 거의 동시에 화면을 갱신하는 출력들은 같은 계산 결과를 받아야 함
    Project p = baseProject();
    RegionContent c;
    c.regionId = "r";
    c.items = {item(red, 0.0)};
    p.scenes[0].contents.push_back(c);
    PlaybackEngine engine;
    std::vector<std::string> errors;
    engine.play(p, "s", errors);
    CHECK(waitFor(engine, 200, [](const auto& l) { return l.size() == 1; }));
    std::vector<SceneLayer> a;
    std::vector<SceneLayer> b;
    engine.collect(a);
    engine.collect(b);
    CHECK(a.size() == 1 && b.size() == 1);
    if (a.size() == 1 && b.size() == 1)
        CHECK(a[0].frame == b[0].frame && a[0].key == b[0].key && test::near(a[0].dst.x, b[0].dst.x));
    engine.stop();
    engine.collect(a);
    CHECK(a.empty()); // 정지 후에는 공유 결과도 비움
}

static void testVideoEnd(const std::string& video, const std::string& blue)
{
    // 자동 길이 영상(약 1초) 다음 항목으로 넘어가는지
    Project p = baseProject();
    RegionContent c;
    c.regionId = "r";
    c.items = {item(video, 0.0), item(blue, 0.0)};
    p.scenes[0].contents.push_back(c);

    PlaybackEngine engine;
    std::vector<std::string> errors;
    engine.play(p, "s", errors);
    CHECK(errors.empty());
    // 영상 프레임은 320px, 이미지는 64px로 구분
    CHECK(waitFor(engine, 300, [](const auto& l) { return l.size() == 1 && l[0].frame->width == 320; }));
    CHECK(waitFor(engine, 3000, [](const auto& l) { return l.size() == 1 && l[0].frame->width == 64; }));
    engine.stop();
}

int main(int argc, char* argv[])
{
#ifndef _WIN32
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const std::string red = makeImage(dir, "red.png", QColor(255, 0, 0));
    const std::string blue = makeImage(dir, "blue.png", QColor(0, 0, 255));

    testCutLoop(red, blue);
    testFade(red, blue);
    testNoLoopHoldsLast(red, blue);
    testMissingFileSkipped(red, blue);
    testLiveUpdate(red, blue);
    testTicker();
    testSharedFrame(red);
    if (argc > 1)
        testVideoEnd(argv[1], blue);
    else
        std::printf("영상 파일 없음: 영상 끝 감지 검사 생략\n");
    return test::finish("test_playback");
}
