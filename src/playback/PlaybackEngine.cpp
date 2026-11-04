#include "playback/PlaybackEngine.h"

#include "core/Log.h"
#include "core/TickerMotion.h"
#include "playback/PlaylistPlayer.h"
#include "playback/TickerPlayer.h"

#include <algorithm>
#include <cmath>

namespace led {

namespace {

constexpr int kFirstFrameTimeoutMs = 3000;
constexpr auto kControlInterval = std::chrono::milliseconds(10);
constexpr std::uint64_t kTickerKeyFlag = 1ULL << 63;
// 이 시간 안에 화면을 갱신하는 출력들은 같은 프레임을 표시 (60Hz 1프레임 = 16.7ms의 약 1/3)
constexpr auto kShareWindow = std::chrono::milliseconds(6);

double secondsBetween(PlaybackClock::time_point from, PlaybackClock::time_point to)
{
    return std::chrono::duration<double>(to - from).count();
}

} // namespace

struct PlaybackEngine::Slot {
    Region region;
    RegionContent content;
    bool hardware = false;   // 실제 적용된 디코딩 방식 (바뀌면 다시 열어야 함)
    std::unique_ptr<PlaylistPlayer> playlist;
    std::unique_ptr<TickerPlayer> ticker;

    void start(PlaybackClock::time_point t)
    {
        if (playlist)
            playlist->start(t);
        if (ticker)
            ticker->start(t);
    }
    void tick(PlaybackClock::time_point t)
    {
        if (playlist)
            playlist->tick(t);
        if (ticker)
            ticker->tick(t);
    }
    void stop()
    {
        if (playlist)
            playlist->stop();
    }
};

PlaybackEngine::PlaybackEngine() = default;

PlaybackEngine::~PlaybackEngine()
{
    stop();
}

std::vector<std::unique_ptr<PlaybackEngine::Slot>> PlaybackEngine::buildSlots(
    const Project& project, const Scene& scene, std::vector<std::unique_ptr<Slot>>* reuse,
    std::vector<std::string>& errors, std::vector<Slot*>& created)
{
    std::vector<std::unique_ptr<Slot>> slotList;
    const auto now = PlaybackClock::now();

    for (const Region& region : project.regions) {
        if (!region.visible || region.rect.isEmpty())
            continue;
        const RegionContent* content = scene.contentFor(region.id);
        if (!content || content->isEmpty())
            continue;

        const bool hardware = effectiveHardwareDecoding(project, region);

        // 내용과 디코딩 방식이 같은 기존 영역은 재생을 이어감 (위치/표시 방식만 갱신)
        if (reuse) {
            auto it = std::find_if(reuse->begin(), reuse->end(), [&](const std::unique_ptr<Slot>& s) {
                return s && s->region.id == region.id && s->content == *content &&
                       (s->hardware == hardware || content->kind == ContentKind::Ticker);
            });
            if (it != reuse->end()) {
                std::unique_ptr<Slot> slot = std::move(*it);
                slot->region = region;
                if (slot->ticker)
                    slot->ticker->setRegionSize(region.rect.w, region.rect.h, now);
                slotList.push_back(std::move(slot));
                continue;
            }
        }

        auto slot = std::make_unique<Slot>();
        slot->region = region;
        slot->content = *content;
        slot->hardware = hardware;
        std::vector<std::string> slotErrors;
        bool ok;
        if (content->kind == ContentKind::Ticker) {
            slot->ticker = std::make_unique<TickerPlayer>(*content, region.rect.w, region.rect.h);
            ok = slot->ticker->prepare(slotErrors);
        } else {
            slot->playlist = std::make_unique<PlaylistPlayer>(*content, hardware);
            ok = slot->playlist->prepare(slotErrors);
        }
        for (const std::string& e : slotErrors)
            errors.push_back(region.name + ": " + e);
        if (!ok)
            continue;
        created.push_back(slot.get());
        slotList.push_back(std::move(slot));
    }
    return slotList;
}

void PlaybackEngine::publishLocked()
{
    auto views = std::make_shared<std::vector<RegionView>>();
    views->reserve(m_slots.size());
    for (const auto& slot : m_slots) {
        RegionView v;
        v.rect = slot->region.rect;
        v.fit = slot->region.fit;
        v.opacity = static_cast<float>(std::clamp(slot->region.opacity, 0.0, 1.0));
        if (slot->playlist)
            slot->playlist->appendClips(v.clips);
        if (slot->ticker)
            v.ticker = slot->ticker->view();
        views->push_back(std::move(v));
    }
    std::lock_guard<std::mutex> lock(m_viewMutex);
    m_views = std::move(views);
}

void PlaybackEngine::play(const Project& project, const std::string& sceneId, std::vector<std::string>& errors)
{
    stop();
    const Scene* scene = project.findScene(sceneId);
    if (!scene) {
        errors.push_back("재생할 장면을 찾을 수 없음");
        return;
    }

    std::vector<Slot*> created;
    std::vector<std::unique_ptr<Slot>> slotList = buildSlots(project, *scene, nullptr, errors, created);

    // 모든 영역의 첫 프레임 준비 후 같은 시각에 시작 (영역 간 동기화)
    const auto deadline = PlaybackClock::now() + std::chrono::milliseconds(kFirstFrameTimeoutMs);
    for (Slot* slot : created) {
        if (!slot->playlist)
            continue;
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - PlaybackClock::now()).count();
        if (!slot->playlist->waitForFirstFrame(static_cast<int>(std::max<long long>(remaining, 0))))
            logWarning("첫 프레임 준비 시간 초과: " + slot->region.name);
    }
    const auto start = PlaybackClock::now();
    for (auto& slot : slotList)
        slot->start(start);

    const std::size_t count = slotList.size();
    {
        std::lock_guard<std::mutex> lock(m_slotMutex);
        m_slots = std::move(slotList);
        m_sceneId = sceneId;
        m_playing = true;
        publishLocked();
    }
    startControlThread();
    logInfo("재생 시작: " + scene->name + " (영역 " + std::to_string(count) + "개)");
}

void PlaybackEngine::update(const Project& project, std::vector<std::string>& errors)
{
    std::vector<std::unique_ptr<Slot>> removed;
    bool sceneMissing = false;
    {
        std::lock_guard<std::mutex> lock(m_slotMutex);
        if (!m_playing)
            return;
        const Scene* scene = project.findScene(m_sceneId);
        if (!scene) {
            sceneMissing = true;
        } else {
            std::vector<std::unique_ptr<Slot>> old = std::move(m_slots);
            std::vector<Slot*> created;
            m_slots = buildSlots(project, *scene, &old, errors, created);
            const auto now = PlaybackClock::now();
            for (Slot* slot : created)
                slot->start(now);
            publishLocked();
            for (auto& s : old)
                if (s)
                    removed.push_back(std::move(s));
        }
    }
    for (auto& s : removed)
        s->stop();
    if (sceneMissing) {
        logWarning("재생 중인 장면이 삭제되어 재생을 정지합니다");
        stop();
    }
}

void PlaybackEngine::stop()
{
    stopControlThread();
    std::vector<std::unique_ptr<Slot>> slotList;
    bool wasPlaying;
    {
        std::lock_guard<std::mutex> lock(m_slotMutex);
        slotList = std::move(m_slots);
        m_slots.clear();
        wasPlaying = m_playing;
        m_playing = false;
        m_sceneId.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_viewMutex);
        m_views.reset();
    }
    {
        std::lock_guard<std::mutex> lock(m_shareMutex);
        m_sharedViews.reset();
        m_sharedLayers.clear();
    }
    for (auto& s : slotList)
        s->stop();
    if (wasPlaying)
        logInfo("재생 정지");
}

bool PlaybackEngine::isPlaying() const
{
    std::lock_guard<std::mutex> lock(m_slotMutex);
    return m_playing;
}

std::string PlaybackEngine::sceneId() const
{
    std::lock_guard<std::mutex> lock(m_slotMutex);
    return m_sceneId;
}

void PlaybackEngine::startControlThread()
{
    if (m_control.joinable())
        return;
    m_controlStop = false;
    m_control = std::thread([this] { controlLoop(); });
}

void PlaybackEngine::stopControlThread()
{
    {
        std::lock_guard<std::mutex> lock(m_wakeMutex);
        m_controlStop = true;
    }
    m_wake.notify_all();
    if (m_control.joinable())
        m_control.join();
}

void PlaybackEngine::controlLoop()
{
    while (!m_controlStop) {
        {
            std::lock_guard<std::mutex> lock(m_slotMutex);
            const auto now = PlaybackClock::now();
            for (auto& slot : m_slots)
                slot->tick(now);
            publishLocked();
        }
        std::unique_lock<std::mutex> lock(m_wakeMutex);
        m_wake.wait_for(lock, kControlInterval, [this] { return m_controlStop.load(); });
    }
}

void PlaybackEngine::collect(std::vector<SceneLayer>& out)
{
    out.clear();
    std::shared_ptr<const std::vector<RegionView>> views;
    {
        std::lock_guard<std::mutex> lock(m_viewMutex);
        views = m_views;
    }
    if (!views)
        return;

    std::lock_guard<std::mutex> lock(m_shareMutex);
    const auto now = PlaybackClock::now();
    if (m_sharedViews && now - m_sharedTime < kShareWindow) {
        out = m_sharedLayers; // 다른 출력이 방금 계산한 결과 사용
        return;
    }
    computeLayers(*views, now, out);
    m_sharedViews = views;
    m_sharedTime = now;
    m_sharedLayers = out;
}

void PlaybackEngine::computeLayers(const std::vector<RegionView>& views, PlaybackClock::time_point now,
                                   std::vector<SceneLayer>& out)
{
    std::vector<double> positions;

    for (const RegionView& view : views) {
        if (view.opacity <= 0.0f)
            continue;
        const RectF regionRect{double(view.rect.x), double(view.rect.y), double(view.rect.w), double(view.rect.h)};

        // 플레이리스트 (페이드 중에는 이전 항목 위에 새 항목이 겹침)
        for (const ClipView& clip : view.clips) {
            std::shared_ptr<const Frame> frame =
                clip.source->frameAt(std::max(0.0, secondsBetween(clip.start, now)));
            if (!frame)
                continue;
            float alpha = 1.0f;
            if (clip.fadeSeconds > 0.0)
                alpha = static_cast<float>(
                    std::clamp(secondsBetween(clip.fadeStart, now) / clip.fadeSeconds, 0.0, 1.0));
            if (alpha <= 0.0f)
                continue;
            const Placement p = computePlacement(view.rect, frame->width, frame->height, view.fit);
            if (!p.visible)
                continue;
            SceneLayer layer;
            layer.key = clip.key;
            layer.dst = p.dst;
            layer.uv = p.uv;
            layer.opacity = view.opacity * alpha;
            layer.frame = std::move(frame);
            out.push_back(std::move(layer));
        }

        // 흐르는 자막
        if (const TickerView* t = view.ticker.get()) {
            const std::uint64_t keyBase = kTickerKeyFlag | (t->key << 16);
            if (t->background) {
                SceneLayer bg;
                bg.key = keyBase;
                bg.dst = regionRect;
                bg.uv = RectF{0.0, 0.0, 1.0, 1.0};
                bg.opacity = view.opacity;
                bg.frame = t->background;
                out.push_back(std::move(bg));
            }
            const TickerStrip* strip = t->strip.get();
            if (!strip || strip->length <= 0)
                continue;

            // 정수 픽셀 단위로 이동해 글자가 흐려지지 않게 함
            const double distance = std::floor(std::max(0.0, secondsBetween(t->start, now)) * t->speed);
            const bool vertical = strip->vertical;
            tickerCopyPositions(distance, vertical ? regionRect.y : regionRect.x,
                                vertical ? regionRect.h : regionRect.w, strip->length, t->direction, positions);

            for (double pos : positions) {
                for (std::size_t i = 0; i < strip->chunks.size(); ++i) {
                    const TickerChunk& chunk = strip->chunks[i];
                    const Frame& f = *chunk.frame;
                    RectF dst = vertical ? RectF{regionRect.x, pos + chunk.offset, double(f.width), double(f.height)}
                                         : RectF{pos + chunk.offset, regionRect.y, double(f.width), double(f.height)};
                    RectF uv{0.0, 0.0, 1.0, 1.0};
                    if (!clipPlacement(dst, uv, regionRect))
                        continue;
                    SceneLayer layer;
                    layer.key = keyBase | static_cast<std::uint64_t>(i + 1);
                    layer.dst = dst;
                    layer.uv = uv;
                    layer.opacity = view.opacity;
                    layer.frame = chunk.frame;
                    out.push_back(std::move(layer));
                }
            }
        }
    }
}

} // namespace led
