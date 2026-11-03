#include "playback/PlaybackEngine.h"

#include "core/Log.h"
#include "media/MediaFactory.h"

#include <algorithm>
#include <functional>

namespace led {

namespace {
constexpr int kFirstFrameTimeoutMs = 3000;
}

PlaybackEngine::~PlaybackEngine()
{
    stop();
}

std::shared_ptr<PlaybackEngine::LayerList> PlaybackEngine::buildLayers(
    const Project& project, const LayerList* previous, Clock::time_point now, std::vector<std::string>& errors,
    std::vector<std::shared_ptr<MediaSource>>& created)
{
    auto layers = std::make_shared<LayerList>();
    for (const Region& region : project.regions) {
        if (!region.visible || region.source.empty() || region.rect.isEmpty())
            continue;

        Layer layer;
        layer.regionId = region.id;
        layer.key = std::hash<std::string>{}(region.id);
        layer.rect = region.rect;
        layer.fit = region.fit;
        layer.opacity = static_cast<float>(std::clamp(region.opacity, 0.0, 1.0));
        layer.loop = region.loop;

        // 같은 영역, 같은 파일, 같은 반복 설정이면 기존 소스 재사용
        if (previous) {
            for (const Layer& old : *previous) {
                if (old.regionId == region.id && old.loop == region.loop && old.source &&
                    old.source->path() == region.source) {
                    layer.source = old.source;
                    layer.start = old.start;
                    break;
                }
            }
        }

        if (!layer.source) {
            std::shared_ptr<MediaSource> source = createMediaSource(region.source, region.loop);
            std::string error;
            if (!source->open(error)) {
                source->stop();
                errors.push_back(region.name + ": " + error + " [" + region.source + "]");
                continue;
            }
            layer.source = source;
            layer.start = now;
            created.push_back(source);
        }
        layers->push_back(std::move(layer));
    }
    return layers;
}

void PlaybackEngine::stopUnused(const LayerList* previous, const LayerList& next)
{
    if (!previous)
        return;
    for (const Layer& old : *previous) {
        const bool reused = std::any_of(next.begin(), next.end(),
                                        [&](const Layer& l) { return l.source == old.source; });
        if (!reused && old.source)
            old.source->stop();
    }
}

void PlaybackEngine::play(const Project& project, std::vector<std::string>& errors)
{
    stop();

    std::vector<std::shared_ptr<MediaSource>> created;
    std::shared_ptr<LayerList> layers = buildLayers(project, nullptr, Clock::now(), errors, created);

    // 모든 소스의 첫 프레임 준비를 기다린 뒤 같은 시각으로 시작 (영역 간 동기화)
    const auto deadline = Clock::now() + std::chrono::milliseconds(kFirstFrameTimeoutMs);
    for (const auto& source : created) {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        if (!source->waitForFirstFrame(static_cast<int>(std::max<long long>(remaining, 0))))
            logWarning("첫 프레임 준비 시간 초과: " + source->path());
    }
    const Clock::time_point start = Clock::now();
    for (Layer& layer : *layers)
        layer.start = start;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_layers = layers;
        m_playing = true;
    }
    logInfo("재생 시작 (레이어 " + std::to_string(layers->size()) + "개)");
}

void PlaybackEngine::update(const Project& project, std::vector<std::string>& errors)
{
    std::shared_ptr<const LayerList> previous;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_playing)
            return;
        previous = m_layers;
    }

    std::vector<std::shared_ptr<MediaSource>> created;
    std::shared_ptr<LayerList> layers = buildLayers(project, previous.get(), Clock::now(), errors, created);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_layers = layers;
    }
    stopUnused(previous.get(), *layers);
}

void PlaybackEngine::stop()
{
    std::shared_ptr<const LayerList> previous;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        previous = std::move(m_layers);
        m_layers.reset();
        if (!m_playing)
            return;
        m_playing = false;
    }
    if (previous)
        stopUnused(previous.get(), LayerList{});
    logInfo("재생 정지");
}

bool PlaybackEngine::isPlaying() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_playing;
}

void PlaybackEngine::collect(std::vector<SceneLayer>& out)
{
    out.clear();
    std::shared_ptr<const LayerList> layers;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        layers = m_layers;
    }
    if (!layers)
        return;

    const Clock::time_point now = Clock::now();
    for (const Layer& layer : *layers) {
        const double seconds = std::chrono::duration<double>(now - layer.start).count();
        std::shared_ptr<const Frame> frame = layer.source->frameAt(std::max(0.0, seconds));
        if (!frame)
            continue;
        const Placement p = computePlacement(layer.rect, frame->width, frame->height, layer.fit);
        if (!p.visible)
            continue;
        SceneLayer s;
        s.key = layer.key;
        s.dst = p.dst;
        s.uv = p.uv;
        s.opacity = layer.opacity;
        s.frame = std::move(frame);
        out.push_back(std::move(s));
    }
}

} // namespace led
