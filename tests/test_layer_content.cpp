// 플레이리스트 <-> 타임라인 레이어 변환 검사

#include "TestUtil.h"

#include "core/LayerContent.h"

using namespace led;

static PlaylistItem item(const std::string& id, const std::string& src, double duration, bool fade = false)
{
    PlaylistItem it;
    it.id = id;
    it.source = src;
    it.duration = duration;
    if (fade) {
        it.transition = TransitionType::Fade;
        it.transitionDuration = 1.0;
    }
    return it;
}

static double resolver(const TimelineClip& c)
{
    if (c.source == "video.mp4")
        return 12.0;
    return 0.0; // 알 수 없음 -> 기본값
}

static void testPackAndCycle()
{
    RegionContent c;
    c.regionId = "r";
    c.items = {item("a", "video.mp4", 0), item("b", "img.png", 5, true), item("c", "unknown.mov", 0, true)};
    TimelineLayer layer;
    layer.regionId = "r";
    contentToLayer(c, layer, &resolver);

    CHECK(layer.repeat && !layer.holdLast);
    CHECK(layer.clips.size() == 3);
    if (layer.clips.size() == 3) {
        CHECK(test::near(layer.clips[0].start, 0.0) && test::near(layer.clips[0].duration, 12.0));
        CHECK(layer.clips[0].autoDuration);
        CHECK(test::near(layer.clips[1].start, 11.0));   // 페이드 1초 겹침
        CHECK(test::near(layer.clips[2].start, 15.0));
        CHECK(test::near(layer.clips[2].duration, kDefaultImageDuration)); // 알 수 없는 길이
    }
    CHECK(test::near(layer.contentEnd(), 25.0));
    CHECK(test::near(layerCycleLength(layer), 25.0)); // 첫 클립 페이드 없음
    layer.clips[0].fadeIn = 2.0;
    CHECK(test::near(layerCycleLength(layer), 23.0)); // 마지막과 첫 클립이 2초 겹쳐 이어짐
    CHECK(!isContinuousLayer(layer));
    CHECK(isPlaylistCompatible(layer)); // 첫 클립의 페이드는 배치에 영향 없음

    // 레이어 -> 플레이리스트 화면
    const RegionContent back = layerToContent(&layer, "r");
    CHECK(back.loopPlaylist && back.kind == ContentKind::Playlist && back.items.size() == 3);
    if (back.items.size() == 3) {
        CHECK(back.items[0].duration == 0.0);
        CHECK(back.items[1].transition == TransitionType::Fade);
        CHECK(back.items[2].duration == 0.0);
    }

    // 타임라인에서 클립을 옮기면 플레이리스트 화면에서 편집 불가
    TimelineLayer moved = layer;
    moved.clips[1].start += 3.0;
    CHECK(!isPlaylistCompatible(moved));
}

static void testPreserveAndHold()
{
    TimelineLayer layer;
    layer.regionId = "r";
    RegionContent c;
    c.regionId = "r";
    c.items = {item("a", "x.mp4", 4), item("b", "y.mp4", 6)};
    contentToLayer(c, layer, nullptr);
    layer.clips[0].opacity = 0.5;   // 타임라인에서 바꾼 속성
    layer.clips[0].inPoint = 1.0;

    // 반복 끄기 -> 마지막 화면 유지
    c.loopPlaylist = false;
    c.items[1].duration = 3;
    contentToLayer(c, layer, nullptr);
    CHECK(!layer.repeat && layer.holdLast);
    CHECK(test::near(layer.clips[0].opacity, 0.5) && test::near(layer.clips[0].inPoint, 1.0));
    CHECK(test::near(layer.clips[1].duration, 3.0) && test::near(layer.clips[1].start, 4.0));

    // 이미 반복이 꺼진 레이어는 유지 설정을 그대로 둠
    layer.holdLast = false;
    contentToLayer(c, layer, nullptr);
    CHECK(!layer.repeat && !layer.holdLast);

    // 장면 길이: 반복/유지 레이어 제외
    Timeline tl;
    TimelineLayer hold = layer;
    hold.holdLast = true;
    TimelineLayer rep = layer;
    rep.repeat = true;
    tl.layers = {hold, rep};
    CHECK(tl.length() == 0.0);
    CHECK(test::near(tl.contentEnd(), 7.0));
    tl.layers.push_back(layer);
    CHECK(test::near(tl.length(), 7.0));
}

static void testTickerAndContinuous()
{
    TimelineLayer layer;
    RegionContent t;
    t.regionId = "r";
    t.kind = ContentKind::Ticker;
    t.ticker.lines = {"안내"};
    contentToLayer(t, layer, nullptr);
    CHECK(layer.repeat && layer.clips.size() == 1);
    CHECK(!layer.clips.empty() && layer.clips[0].kind == ClipKind::Ticker);
    CHECK(isContinuousLayer(layer));
    const RegionContent back = layerToContent(&layer, "r");
    CHECK(back.kind == ContentKind::Ticker && back.ticker.lines.size() == 1);

    // 자막 클립 id 유지 (문구만 바꿈)
    const std::string id = layer.clips[0].id;
    t.ticker.lines = {"변경"};
    contentToLayer(t, layer, nullptr);
    CHECK(layer.clips[0].id == id && layer.clips[0].ticker.lines[0] == "변경");

    // 파일 1개: 자동 길이면 끊김 없이 반복, 길이 지정이면 그 길이로 반복
    TimelineLayer single;
    RegionContent c;
    c.items = {item("a", "video.mp4", 0)};
    contentToLayer(c, single, &resolver);
    CHECK(isContinuousLayer(single));
    c.items[0].duration = 5;
    contentToLayer(c, single, &resolver);
    CHECK(!isContinuousLayer(single));
}

static void testPlaylistLayerIndex()
{
    Timeline tl;
    TimelineLayer a;
    a.regionId = "x";
    TimelineLayer b;
    b.regionId = "y";
    TimelineLayer c;
    c.regionId = "y";
    tl.layers = {a, b, c};
    CHECK(playlistLayerIndex(tl, "y") == 1);
    CHECK(playlistLayerIndex(tl, "x") == 0);
    CHECK(playlistLayerIndex(tl, "z") == -1);
    CHECK(playlistLayerIndex(tl, "") == -1);
    const RegionContent empty = layerToContent(nullptr, "z");
    CHECK(empty.isEmpty() && empty.loopPlaylist);
}

int main()
{
    testPackAndCycle();
    testPreserveAndHold();
    testTickerAndContinuous();
    testPlaylistLayerIndex();
    return test::finish("test_layer_content");
}
