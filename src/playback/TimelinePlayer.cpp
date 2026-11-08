#include "playback/TimelinePlayer.h"

#include "core/Log.h"
#include "media/MediaFactory.h"
#include "media/VideoSource.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace led {

namespace {

constexpr double kPreloadSeconds = 1.5;   // 클립 시작 전에 미리 여는 시간
constexpr double kHoldEpsilon = 0.001;

double fadeFactor(double start, double end, double fadeIn, double fadeOut, double t)
{
    if (t < start || t >= end)
        return 0.0;
    double a = 1.0;
    if (fadeIn > 0.0)
        a = std::min(a, (t - start) / fadeIn);
    if (fadeOut > 0.0)
        a = std::min(a, (end - t) / fadeOut);
    return std::clamp(a, 0.0, 1.0);
}

} // namespace

double timelineClipAlpha(const ClipView& clip, double t)
{
    return clip.clipOpacity * fadeFactor(clip.clipStart, clip.clipEnd, clip.fadeIn, clip.fadeOut, t);
}

TimelinePlayer::TimelinePlayer(const Project& project, const Scene& scene)
    : m_timeline(scene.timeline)
    , m_hardwareDefault(project.hardwareDecoding)
{
    for (const Region& r : project.regions) {
        m_regions[r.id] = r;
        m_regionHardware[r.id] = effectiveHardwareDecoding(project, r);
    }
}

TimelinePlayer::~TimelinePlayer()
{
    stop();
}

const TimelineLayer* TimelinePlayer::findLayer(const std::string& id) const
{
    for (const auto& l : m_timeline.layers)
        if (l.id == id)
            return &l;
    return nullptr;
}

bool TimelinePlayer::layerShowsVideo(const TimelineLayer& layer) const
{
    if (!layer.visible || layer.regionId.empty())
        return false;
    const auto it = m_regions.find(layer.regionId);
    return it != m_regions.end();
}

void TimelinePlayer::openClip(const TimelineLayer& layer, const TimelineClip& clip, double t, bool wait)
{
    const MediaInfo info = probeMediaCached(clip.source);
    const bool needVideo = (info.hasVideo || info.still) && layerShowsVideo(layer);
    const bool needAudio = info.hasAudio && !layer.muted;

    // 소스 시각 0에 해당하는 타임라인 시각과 파일 시작 위치
    double sourceZero = clip.start;
    double mediaStart = clip.inPoint;
    if (t > clip.start) {
        const double elapsed = t - clip.start;
        if (clip.loop && info.duration > clip.inPoint) {
            // 반복 클립: 반복 구간의 시작부터 열고 현재 위치까지 따라잡음
            const double segment = info.duration - clip.inPoint;
            sourceZero = t - std::fmod(elapsed, segment);
        } else {
            sourceZero = t;
            mediaStart = clip.inPoint + elapsed;
        }
    }

    const auto hwIt = m_regionHardware.find(layer.regionId);
    const bool hardware = hwIt != m_regionHardware.end() ? hwIt->second : m_hardwareDefault;
    const std::string path = clip.source;
    const bool loop = clip.loop;
    const bool still = info.still;

    Pending pending;
    pending.layerId = layer.id;
    pending.clip = clip;
    pending.sourceZero = sourceZero;
    pending.future = std::async(std::launch::async, [=]() {
        Opened o;
        if (needVideo) {
            std::shared_ptr<MediaSource> v;
            if (still) {
                v = createMediaSource(path, false, false);
            } else {
                VideoOptions opt;
                opt.loop = loop;
                opt.hardware = hardware;
                opt.startTime = mediaStart;
                v = std::make_shared<VideoSource>(path, opt);
            }
            std::string error;
            if (v->open(error))
                o.video = v;
            else
                o.error = error;
        }
        if (needAudio) {
            auto a = std::make_shared<AudioSource>(path, mediaStart, loop);
            std::string error;
            if (a->open(error))
                o.audio = a;
            else if (o.error.empty() && !needVideo)
                o.error = error;
        }
        if (!needVideo && !needAudio && !info.ok)
            o.error = info.error.empty() ? "파일을 열 수 없음" : info.error;
        return o;
    });
    if (wait)
        pending.future.wait();
    m_pending[clip.id] = std::move(pending);
}

void TimelinePlayer::closeInstance(Instance& inst)
{
    if (inst.video)
        inst.video->stop();
    if (inst.audio) {
        AudioOutput::instance().remove(inst.audio);
        inst.audio->source().stop();
    }
}

void TimelinePlayer::clearAll()
{
    for (auto& [id, inst] : m_instances)
        closeInstance(inst);
    m_instances.clear();
    for (auto& [id, p] : m_pending) {
        if (p.future.valid()) {
            Opened o = p.future.get();
            if (o.video)
                o.video->stop();
            if (o.audio)
                o.audio->stop();
        }
    }
    m_pending.clear();
}

void TimelinePlayer::prepare(std::vector<std::string>& errors, int timeoutMs)
{
    for (const TimelineLayer& layer : m_timeline.layers)
        for (const TimelineClip& clip : layer.clips)
            if (clip.start - kPreloadSeconds <= 0.0 && 0.0 < clip.end())
                openClip(layer, clip, 0.0, true);
    tick(); // 열린 클립을 재생 목록으로 옮김
    for (auto& [id, inst] : m_instances) {
        if (inst.video && !inst.video->waitForFirstFrame(timeoutMs))
            errors.push_back("첫 프레임 준비 시간 초과: " + inst.clip.source);
    }
}

void TimelinePlayer::start()
{
    m_clock->start(0.0);
    m_lastTime = 0.0;
    logInfo("타임라인 재생 시작 (길이 " + std::to_string(static_cast<int>(m_timeline.length())) + "초)");
}

void TimelinePlayer::stop()
{
    clearAll();
}

void TimelinePlayer::tick()
{
    double t = m_clock->time();
    const bool running = m_clock->running();
    const double length = m_timeline.length();

    // 끝 동작
    if (running && length > 0.0 && t >= length && !m_finished) {
        switch (m_timeline.endAction) {
        case TimelineEnd::Loop:
            seek(0.0);
            t = 0.0;
            break;
        case TimelineEnd::Hold:
            t = std::max(0.0, length - kHoldEpsilon);
            m_clock->pauseAt(t);
            logInfo("타임라인 끝: 마지막 화면 유지");
            break;
        case TimelineEnd::Stop:
            m_clock->pauseAt(length);
            m_finished = true;
            clearAll();
            logInfo("타임라인 끝: 정지");
            break;
        }
    }

    // 큐 지점: 지나치는 순간 멈춤
    if (running && !m_finished) {
        for (const TimelineCue& cue : m_timeline.cues) {
            if (m_lastTime < cue.time && cue.time <= t) {
                m_clock->pauseAt(cue.time);
                t = cue.time;
                m_waitingCue = cue.name.empty() ? std::string("큐") : cue.name;
                logInfo("큐 '" + m_waitingCue + "'에서 대기 (GO로 계속)");
                break;
            }
        }
    }
    m_lastTime = t;
    if (m_finished)
        return;

    auto wanted = [t](const TimelineClip& c) { return c.start - kPreloadSeconds <= t && t < c.end(); };

    // 더 이상 필요 없는 클립 닫기 (구간 밖, 또는 클립 중간에서 열었는데 시작 전으로 이동한 경우)
    for (auto it = m_instances.begin(); it != m_instances.end();) {
        Instance& inst = it->second;
        const bool keep = findLayer(inst.layerId) && wanted(inst.clip) &&
                          !(t < inst.clip.start && inst.sourceZero != inst.clip.start);
        if (!keep) {
            closeInstance(inst);
            it = m_instances.erase(it);
        } else {
            ++it;
        }
    }

    // 열기가 끝난 클립을 재생 목록으로
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        Pending& p = it->second;
        if (p.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++it;
            continue;
        }
        Opened o = p.future.get();
        if (!o.error.empty() && !o.video && !o.audio) {
            logError("타임라인 클립을 열 수 없음: " + o.error + " [" + p.clip.source + "]");
            // 같은 클립을 계속 다시 열지 않도록 빈 재생 항목으로 둠
        }
        Instance inst;
        inst.layerId = p.layerId;
        inst.clip = p.clip;
        inst.video = o.video;
        inst.sourceZero = p.sourceZero;
        inst.key = nextRenderKey();
        if (o.audio) {
            std::string error;
            if (!m_audioReady)
                m_audioReady = AudioOutput::instance().ensureStarted(&error);
            inst.audio = std::make_shared<AudioTrack>(o.audio);
            AudioOutput::instance().add(inst.audio);
        }
        const std::string id = it->first;
        it = m_pending.erase(it);
        if (!wanted(inst.clip)) {
            closeInstance(inst);
            continue;
        }
        m_instances[id] = std::move(inst);
    }

    // 새로 필요한 클립 열기
    for (const TimelineLayer& layer : m_timeline.layers) {
        for (const TimelineClip& clip : layer.clips) {
            if (!wanted(clip) || m_instances.count(clip.id) || m_pending.count(clip.id))
                continue;
            openClip(layer, clip, t, false);
        }
    }

    updateAudio(t);
}

void TimelinePlayer::updateAudio(double t)
{
    const bool running = m_clock->running();
    for (auto& [id, inst] : m_instances) {
        if (!inst.audio)
            continue;
        const TimelineLayer* layer = findLayer(inst.layerId);
        const bool active = t >= inst.clip.start && t < inst.clip.end();
        const double gain = (layer && !layer->muted ? 1.0 : 0.0) * inst.clip.volume *
                            fadeFactor(inst.clip.start, inst.clip.end(), inst.clip.fadeIn, inst.clip.fadeOut, t);
        inst.audio->gain = static_cast<float>(gain);
        inst.audio->expected = std::max(0.0, t - inst.sourceZero);
        inst.audio->running = running && active;
    }
}

void TimelinePlayer::reload(const Project& project, const Scene& scene)
{
    const Timeline old = m_timeline;
    m_timeline = scene.timeline;
    m_regions.clear();
    m_regionHardware.clear();
    m_hardwareDefault = project.hardwareDecoding;
    for (const Region& r : project.regions) {
        m_regions[r.id] = r;
        m_regionHardware[r.id] = effectiveHardwareDecoding(project, r);
    }

    // 클립 또는 레이어 설정(영역/표시/음소거)이 바뀐 것만 다시 엶
    auto unchanged = [&](const std::string& layerId, const TimelineClip& clip) {
        const TimelineLayer* now = findLayer(layerId);
        const TimelineLayer* before = nullptr;
        for (const auto& l : old.layers)
            if (l.id == layerId)
                before = &l;
        if (!now || !before || now->regionId != before->regionId || now->visible != before->visible ||
            now->muted != before->muted)
            return false;
        for (const auto& c : now->clips)
            if (c.id == clip.id)
                return c == clip;
        return false;
    };
    for (auto it = m_instances.begin(); it != m_instances.end();) {
        if (!unchanged(it->second.layerId, it->second.clip)) {
            closeInstance(it->second);
            it = m_instances.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (!unchanged(it->second.layerId, it->second.clip)) {
            Opened o = it->second.future.get();
            if (o.video)
                o.video->stop();
            if (o.audio)
                o.audio->stop();
            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }
    if (m_finished && m_timeline.length() > m_clock->time())
        m_finished = false;
}

void TimelinePlayer::pause()
{
    m_clock->pause();
    updateAudio(m_clock->time());
}

void TimelinePlayer::resume()
{
    const double length = m_timeline.length();
    const double t = m_clock->time();
    if (m_finished || (length > 0.0 && t >= length - 0.01))
        seek(0.0);
    m_finished = false;
    m_waitingCue.clear();
    m_clock->start(m_clock->time());
    m_lastTime = m_clock->time();
}

void TimelinePlayer::seek(double t)
{
    t = std::max(0.0, t);
    clearAll();
    m_finished = false;
    m_waitingCue.clear();
    m_clock->seek(t);
    m_lastTime = t;
}

TimelineStatus TimelinePlayer::status() const
{
    TimelineStatus s;
    s.active = true;
    s.time = m_clock->time();
    s.length = m_timeline.length();
    s.running = m_clock->running();
    s.finished = m_finished;
    s.waitingCue = m_waitingCue;
    return s;
}

void TimelinePlayer::appendViews(std::vector<RegionView>& out) const
{
    if (m_finished)
        return;
    // 맨 아래 레이어(목록 끝)부터 그림
    for (auto layerIt = m_timeline.layers.rbegin(); layerIt != m_timeline.layers.rend(); ++layerIt) {
        const TimelineLayer& layer = *layerIt;
        if (!layerShowsVideo(layer))
            continue;
        const Region& region = m_regions.at(layer.regionId);
        if (!region.visible)
            continue;

        std::vector<const Instance*> items;
        for (const auto& [id, inst] : m_instances)
            if (inst.layerId == layer.id && inst.video)
                items.push_back(&inst);
        if (items.empty())
            continue;
        std::sort(items.begin(), items.end(),
                  [](const Instance* a, const Instance* b) { return a->clip.start < b->clip.start; });

        RegionView v;
        v.rect = region.rect;
        v.fit = region.fit;
        v.opacity = static_cast<float>(std::clamp(region.opacity, 0.0, 1.0));
        for (const Instance* inst : items) {
            ClipView c;
            c.source = inst->video;
            c.key = inst->key;
            c.timeline = m_clock;
            c.sourceZero = inst->sourceZero;
            c.clipStart = inst->clip.start;
            c.clipEnd = inst->clip.end();
            c.fadeIn = inst->clip.fadeIn;
            c.fadeOut = inst->clip.fadeOut;
            c.clipOpacity = inst->clip.opacity;
            v.clips.push_back(std::move(c));
        }
        out.push_back(std::move(v));
    }
}

} // namespace led
