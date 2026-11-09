#include "playback/PlaybackEngine.h"

#include "core/Log.h"
#include "core/TickerMotion.h"

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

PlaybackEngine::PlaybackEngine() = default;

PlaybackEngine::~PlaybackEngine()
{
    stop();
}

void PlaybackEngine::publishLocked()
{
    auto views = std::make_shared<std::vector<RegionView>>();
    if (m_timelinePlayer)
        m_timelinePlayer->appendViews(*views);
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

    auto player = std::make_unique<TimelinePlayer>(project, *scene);
    player->prepare(errors, kFirstFrameTimeoutMs);
    player->start();
    {
        std::lock_guard<std::mutex> lock(m_slotMutex);
        m_timelinePlayer = std::move(player);
        m_sceneId = sceneId;
        m_playing = true;
        publishLocked();
    }
    startControlThread();
    logInfo("재생 시작: " + scene->name);
}

void PlaybackEngine::update(const Project& project, std::vector<std::string>& errors)
{
    (void)errors;
    bool sceneMissing = false;
    {
        std::lock_guard<std::mutex> lock(m_slotMutex);
        if (!m_playing || !m_timelinePlayer)
            return;
        const Scene* scene = project.findScene(m_sceneId);
        if (!scene) {
            sceneMissing = true;
        } else {
            m_timelinePlayer->reload(project, *scene);
            publishLocked();
        }
    }
    if (sceneMissing) {
        logWarning("재생 중인 장면이 삭제되어 재생을 정지합니다");
        stop();
    }
}

void PlaybackEngine::stop()
{
    stopControlThread();
    std::unique_ptr<TimelinePlayer> timeline;
    bool wasPlaying;
    {
        std::lock_guard<std::mutex> lock(m_slotMutex);
        timeline = std::move(m_timelinePlayer);
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
    if (timeline)
        timeline->stop();
    if (wasPlaying)
        logInfo("재생 정지");
}

bool PlaybackEngine::skip(const std::string& regionId, int delta)
{
    std::lock_guard<std::mutex> lock(m_slotMutex);
    if (!m_timelinePlayer || !m_timelinePlayer->skip(regionId, delta))
        return false;
    m_timelinePlayer->tick();
    publishLocked();
    return true;
}

bool PlaybackEngine::timelinePause()
{
    std::lock_guard<std::mutex> lock(m_slotMutex);
    if (!m_timelinePlayer)
        return false;
    m_timelinePlayer->pause();
    return true;
}

bool PlaybackEngine::timelineResume()
{
    std::lock_guard<std::mutex> lock(m_slotMutex);
    if (!m_timelinePlayer)
        return false;
    m_timelinePlayer->resume();
    return true;
}

bool PlaybackEngine::timelineSeek(double seconds)
{
    std::lock_guard<std::mutex> lock(m_slotMutex);
    if (!m_timelinePlayer)
        return false;
    m_timelinePlayer->seek(seconds);
    m_timelinePlayer->tick();
    publishLocked();
    return true;
}

TimelineStatus PlaybackEngine::timelineStatus() const
{
    std::lock_guard<std::mutex> lock(m_slotMutex);
    return m_timelinePlayer ? m_timelinePlayer->status() : TimelineStatus{};
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
            if (m_timelinePlayer)
                m_timelinePlayer->tick();
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

        // 클립 (페이드 중에는 이전 클립 위에 새 클립이 겹침). 타임라인 시계 기준 (일시정지 시 같은 프레임 유지)
        for (const ClipView& clip : view.clips) {
            if (!clip.source || !clip.fade.timeline)
                continue;
            const double t = clip.fade.timeline->time();
            const float alpha = static_cast<float>(clip.fade.alphaAt(t));
            if (alpha <= 0.0f)
                continue;
            std::shared_ptr<const Frame> frame = clip.source->frameAt(std::max(0.0, t - clip.sourceZero));
            if (!frame)
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
            float tickerAlpha = 1.0f;
            if (view.tickerFade.timeline)
                tickerAlpha = static_cast<float>(view.tickerFade.alphaAt(view.tickerFade.timeline->time()));
            if (tickerAlpha <= 0.0f)
                continue;
            const std::uint64_t keyBase = kTickerKeyFlag | (t->key << 16);
            if (t->background) {
                SceneLayer bg;
                bg.key = keyBase;
                bg.dst = regionRect;
                bg.uv = RectF{0.0, 0.0, 1.0, 1.0};
                bg.opacity = view.opacity * tickerAlpha;
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
                    layer.opacity = view.opacity * tickerAlpha;
                    layer.frame = chunk.frame;
                    out.push_back(std::move(layer));
                }
            }
        }
    }
}

} // namespace led
