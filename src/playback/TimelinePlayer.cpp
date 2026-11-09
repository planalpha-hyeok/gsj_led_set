#include "playback/TimelinePlayer.h"

#include "core/LayerContent.h"
#include "core/Log.h"
#include "media/MediaFactory.h"
#include "media/VideoSource.h"
#include "playback/TickerPlayer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace led {

namespace {

constexpr double kPreloadSeconds = 1.5;   // 클립 시작 전에 미리 여는 시간
constexpr double kHoldEpsilon = 0.001;
constexpr double kForever = 1e12;         // 계속 표시

double fadeFactor(double start, double end, double fadeIn, double fadeOut, double t)
{
    ClipFade f;
    f.clipStart = start;
    f.clipEnd = end;
    f.fadeIn = fadeIn;
    f.fadeOut = fadeOut;
    return f.alphaAt(t);
}

// 반복하지 않는 레이어에서 가장 늦게 끝나는 클립 (같으면 목록 뒤쪽)
const TimelineClip* lastClip(const TimelineLayer& layer)
{
    const TimelineClip* last = nullptr;
    for (const auto& c : layer.clips)
        if (!last || c.end() >= last->end())
            last = &c;
    return last;
}

// 클립 시작 시각 순서의 번호 목록
std::vector<std::size_t> clipOrder(const TimelineLayer& layer)
{
    std::vector<std::size_t> order(layer.clips.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return layer.clips[a].start < layer.clips[b].start; });
    return order;
}

} // namespace

TimelinePlayer::TimelinePlayer(const Project& project, const Scene& scene)
    : m_timeline(scene.timeline)
{
    setRegions(project);
}

TimelinePlayer::~TimelinePlayer()
{
    stop();
}

void TimelinePlayer::setRegions(const Project& project)
{
    m_regionOrder = project.regions;
    m_regions.clear();
    m_regionHardware.clear();
    m_hardwareDefault = project.hardwareDecoding;
    for (const Region& r : project.regions) {
        m_regions[r.id] = r;
        m_regionHardware[r.id] = effectiveHardwareDecoding(project, r);
    }
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
    return m_regions.count(layer.regionId) != 0;
}

void TimelinePlayer::collectOccurrences(double t, double preload, std::vector<Occurrence>& out) const
{
    out.clear();
    auto add = [&](const TimelineLayer& layer, const TimelineClip& clip, long cycle, double start, double showEnd,
                   double soundEnd, bool continuous) {
        if (!(start - preload <= t && t < showEnd))
            return;
        Occurrence o;
        o.layer = &layer;
        o.clip = &clip;
        o.key = clip.id + "#" + std::to_string(cycle);
        o.start = start;
        o.showEnd = showEnd;
        o.soundEnd = soundEnd;
        o.continuous = continuous;
        out.push_back(std::move(o));
    };

    for (const TimelineLayer& layer : m_timeline.layers) {
        if (layer.clips.empty())
            continue;
        const auto offIt = m_layerOffset.find(layer.id);
        const double offset = offIt != m_layerOffset.end() ? offIt->second : 0.0;

        if (isContinuousLayer(layer)) {
            const TimelineClip& c = layer.clips.front();
            add(layer, c, 0, offset, kForever, kForever, true);
            continue;
        }
        if (layer.repeat) {
            const double cycle = layerCycleLength(layer);
            for (const TimelineClip& c : layer.clips) {
                const double local = t - offset;
                const long kLo = std::max(0L, static_cast<long>(std::floor((local - c.end()) / cycle)));
                const long kHi = static_cast<long>(std::floor((local + preload - c.start) / cycle));
                for (long k = kLo; k <= kHi; ++k) {
                    const double start = offset + c.start + static_cast<double>(k) * cycle;
                    add(layer, c, k, start, start + c.duration, start + c.duration, false);
                }
            }
            continue;
        }
        const TimelineClip* last = layer.holdLast ? lastClip(layer) : nullptr;
        for (const TimelineClip& c : layer.clips) {
            const double start = offset + c.start;
            const double end = start + c.duration;
            add(layer, c, 0, start, &c == last ? kForever : end, end, false);
        }
    }
}

void TimelinePlayer::openOccurrence(const Occurrence& occ, double t, bool wait, std::vector<std::string>* errors)
{
    const TimelineLayer& layer = *occ.layer;
    const TimelineClip& clip = *occ.clip;

    Pending pending;
    Instance& inst = pending.inst;
    inst.layerId = layer.id;
    inst.clip = clip;
    inst.start = occ.start;
    inst.showEnd = occ.showEnd;
    inst.soundEnd = occ.soundEnd;
    inst.sourceZero = occ.start;
    inst.key = nextRenderKey();

    // 자막: 바로 만듦 (파일을 열지 않음)
    if (clip.kind == ClipKind::Ticker) {
        if (layerShowsVideo(layer)) {
            const Region& region = m_regions.at(layer.regionId);
            auto ticker = std::make_shared<TickerPlayer>(clip.ticker, region.rect.w, region.rect.h);
            std::vector<std::string> tickerErrors;
            ticker->prepare(tickerErrors);
            if (errors)
                for (const auto& e : tickerErrors)
                    errors->push_back(layer.name + ": " + e);
            const auto now = PlaybackClock::now();
            const double elapsed = t - occ.start;
            ticker->start(now - std::chrono::duration_cast<PlaybackClock::duration>(
                                    std::chrono::duration<double>(elapsed)));
            inst.ticker = std::move(ticker);
        }
        std::promise<Opened> ready;
        ready.set_value(Opened{});
        pending.future = ready.get_future();
        m_pending[occ.key] = std::move(pending);
        return;
    }

    const auto hwIt = m_regionHardware.find(layer.regionId);
    const bool hardware = hwIt != m_regionHardware.end() ? hwIt->second : m_hardwareDefault;
    const bool showVideo = layerShowsVideo(layer);

    if (clip.kind == ClipKind::Live) {
        const std::string device = clip.source;
        const std::string size = clip.liveSize;
        const double fps = clip.liveFps;
        pending.future = std::async(std::launch::async, [=]() {
            Opened o;
            if (!showVideo)
                return o;
            if (device.empty()) {
                o.error = "입력 장치가 지정되지 않음";
                return o;
            }
            auto v = createLiveSource(device, size, fps);
            std::string error;
            if (v->open(error))
                o.video = v;
            else
                o.error = error;
            return o;
        });
        if (wait)
            pending.future.wait();
        m_pending[occ.key] = std::move(pending);
        return;
    }

    const MediaInfo info = probeMediaCached(clip.source);
    const bool needVideo = (info.hasVideo || info.still) && showVideo;
    const bool needAudio = info.hasAudio && !layer.muted;
    // 계속 재생 레이어는 파일을 반복, 그 외에는 클립의 '반복' 설정
    const bool loop = occ.continuous || clip.loop;

    // 소스 시각 0에 해당하는 타임라인 시각과 파일 시작 위치
    double mediaStart = clip.inPoint;
    if (t > occ.start) {
        const double elapsed = t - occ.start;
        if (loop && info.duration > clip.inPoint) {
            // 반복 재생: 반복 구간의 시작부터 열고 현재 위치까지 따라잡음
            const double segment = info.duration - clip.inPoint;
            inst.sourceZero = t - std::fmod(elapsed, segment);
        } else {
            inst.sourceZero = t;
            mediaStart = clip.inPoint + elapsed;
        }
    }

    const std::string path = clip.source;
    const bool still = info.still;
    pending.future = std::async(std::launch::async, [=]() {
        Opened o;
        if (path.empty()) {
            o.error = "파일이 지정되지 않음";
            return o;
        }
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
    m_pending[occ.key] = std::move(pending);
}

void TimelinePlayer::closeInstance(Instance& inst)
{
    if (inst.video)
        inst.video->stop();
    if (inst.audio) {
        AudioOutput::instance().remove(inst.audio);
        inst.audio->source().stop();
    }
    inst.ticker.reset();
}

void TimelinePlayer::closeLayer(const std::string& layerId)
{
    for (auto it = m_instances.begin(); it != m_instances.end();) {
        if (it->second.layerId == layerId) {
            closeInstance(it->second);
            it = m_instances.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (it->second.inst.layerId == layerId) {
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
    std::vector<Occurrence> occs;
    collectOccurrences(0.0, kPreloadSeconds, occs);
    for (const Occurrence& occ : occs)
        openOccurrence(occ, 0.0, true, &errors);
    m_errorSink = &errors;
    tick(); // 열린 클립을 재생 목록으로 옮김
    m_errorSink = nullptr;
    for (auto& [id, inst] : m_instances) {
        if (inst.video && !inst.video->waitForFirstFrame(timeoutMs))
            errors.push_back("첫 프레임 준비 시간 초과: " + inst.clip.source);
    }
}

void TimelinePlayer::start()
{
    m_clock->start(0.0);
    m_lastTime = 0.0;
    const double length = m_timeline.length();
    if (length > 0.0)
        logInfo("장면 재생 시작 (길이 " + std::to_string(static_cast<int>(length)) + "초)");
    else
        logInfo("장면 재생 시작 (끝없이 반복)");
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

    // 끝 동작 (길이 0 = 반복/유지 레이어만 있음: 끝없이 재생)
    if (running && length > 0.0 && t >= length && !m_finished) {
        switch (m_timeline.endAction) {
        case TimelineEnd::Loop:
            seek(0.0);
            t = 0.0;
            break;
        case TimelineEnd::Hold:
            t = std::max(0.0, length - kHoldEpsilon);
            m_clock->pauseAt(t);
            logInfo("장면 끝: 마지막 화면 유지");
            break;
        case TimelineEnd::Stop:
            m_clock->pauseAt(length);
            m_finished = true;
            clearAll();
            logInfo("장면 끝: 정지");
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

    std::vector<Occurrence> occs;
    collectOccurrences(t, kPreloadSeconds, occs);
    auto wantedKey = [&](const std::string& key) {
        for (const auto& o : occs)
            if (o.key == key)
                return true;
        return false;
    };

    // 더 이상 필요 없는 클립 닫기 (구간 밖, 또는 클립 중간에서 열었는데 시작 전으로 이동한 경우)
    for (auto it = m_instances.begin(); it != m_instances.end();) {
        Instance& inst = it->second;
        const bool keep = findLayer(inst.layerId) && wantedKey(it->first) &&
                          !(t < inst.start && inst.sourceZero != inst.start);
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
        Instance inst = std::move(p.inst);
        if (!o.error.empty() && !o.video && !o.audio &&
            m_failedSources.insert(inst.clip.source + "\n" + o.error).second) {
            logError("클립을 열 수 없음: " + o.error + " [" + inst.clip.source + "]");
            if (m_errorSink)
                m_errorSink->push_back(o.error + " [" + inst.clip.source + "]");
        }
        // 실패해도 같은 클립을 계속 다시 열지 않도록 빈 재생 항목으로 둠
        inst.video = o.video;
        if (o.audio) {
            std::string error;
            if (!m_audioReady)
                m_audioReady = AudioOutput::instance().ensureStarted(&error);
            inst.audio = std::make_shared<AudioTrack>(o.audio);
            AudioOutput::instance().add(inst.audio);
        }
        const std::string key = it->first;
        it = m_pending.erase(it);
        if (!wantedKey(key)) {
            closeInstance(inst);
            continue;
        }
        m_instances[key] = std::move(inst);
    }

    // 새로 필요한 클립 열기
    for (const Occurrence& occ : occs) {
        if (m_instances.count(occ.key) || m_pending.count(occ.key))
            continue;
        openOccurrence(occ, t, false, nullptr);
    }

    // 자막 파일 변경 확인
    const auto now = PlaybackClock::now();
    for (auto& [key, inst] : m_instances)
        if (inst.ticker)
            inst.ticker->tick(now);

    updateAudio(t);
}

void TimelinePlayer::updateAudio(double t)
{
    const bool running = m_clock->running();
    for (auto& [id, inst] : m_instances) {
        if (!inst.audio)
            continue;
        const TimelineLayer* layer = findLayer(inst.layerId);
        const bool endless = inst.soundEnd >= kForever;
        const double end = endless ? kForever : inst.soundEnd;
        const bool active = t >= inst.start && t < end;
        const double gain = (layer && !layer->muted ? 1.0 : 0.0) * inst.clip.volume *
                            fadeFactor(inst.start, end, inst.clip.fadeIn, endless ? 0.0 : inst.clip.fadeOut, t);
        inst.audio->gain = static_cast<float>(gain);
        inst.audio->expected = std::max(0.0, t - inst.sourceZero);
        inst.audio->running = running && active;
    }
}

void TimelinePlayer::reload(const Project& project, const Scene& scene)
{
    m_failedSources.clear(); // 편집 후에는 다시 알림
    const Timeline old = m_timeline;
    const std::map<std::string, bool> oldHardware = m_regionHardware;
    const std::map<std::string, Region> oldRegions = m_regions;
    m_timeline = scene.timeline;
    setRegions(project);

    auto findOld = [&](const std::string& id) -> const TimelineLayer* {
        for (const auto& l : old.layers)
            if (l.id == id)
                return &l;
        return nullptr;
    };
    // 클립 또는 레이어 설정(영역/표시/음소거/반복)이 바뀐 것만 다시 엶
    // 반복/마지막 유지 레이어는 클립 하나만 바뀌어도 배치가 달라지므로 레이어 전체를 다시 엶
    auto unchanged = [&](const Instance& inst) {
        const TimelineLayer* now = findLayer(inst.layerId);
        const TimelineLayer* before = findOld(inst.layerId);
        if (!now || !before || now->regionId != before->regionId || now->visible != before->visible ||
            now->muted != before->muted || now->repeat != before->repeat || now->holdLast != before->holdLast)
            return false;
        if ((now->repeat || now->holdLast) && now->clips != before->clips)
            return false;
        if (!now->regionId.empty()) {
            const auto a = oldHardware.find(now->regionId);
            const auto b = m_regionHardware.find(now->regionId);
            if (a == oldHardware.end() || b == m_regionHardware.end() || a->second != b->second)
                return false;
            if (inst.ticker) {
                const auto ra = oldRegions.find(now->regionId);
                const auto rb = m_regions.find(now->regionId);
                if (ra == oldRegions.end() || rb == m_regions.end() || ra->second.rect.w != rb->second.rect.w ||
                    ra->second.rect.h != rb->second.rect.h)
                    return false; // 자막 크기 변경: 다시 만듦
            }
        }
        for (const auto& c : now->clips)
            if (c.id == inst.clip.id)
                return c == inst.clip;
        return false;
    };
    for (auto it = m_instances.begin(); it != m_instances.end();) {
        if (!unchanged(it->second)) {
            closeInstance(it->second);
            it = m_instances.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (!unchanged(it->second.inst)) {
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
    for (auto it = m_layerOffset.begin(); it != m_layerOffset.end();)
        it = findLayer(it->first) ? std::next(it) : m_layerOffset.erase(it);
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
    m_layerOffset.clear();
    m_finished = false;
    m_waitingCue.clear();
    m_clock->seek(t);
    m_lastTime = t;
}

bool TimelinePlayer::skip(const std::string& regionId, int delta)
{
    const int index = playlistLayerIndex(m_timeline, regionId);
    if (index < 0 || m_finished)
        return false;
    const TimelineLayer& layer = m_timeline.layers[static_cast<std::size_t>(index)];
    if (layer.clips.empty())
        return false;
    if (isContinuousLayer(layer))
        return true; // 클립 1개: 이동할 곳 없음

    const std::vector<std::size_t> order = clipOrder(layer);
    const long n = static_cast<long>(order.size());
    const double t = m_clock->time();
    const double offset = m_layerOffset.count(layer.id) ? m_layerOffset[layer.id] : 0.0;
    const double local = t - offset;

    long cycleIndex = 0;
    double within = local;
    double cycle = 0.0;
    if (layer.repeat) {
        cycle = layerCycleLength(layer);
        cycleIndex = std::max(0L, static_cast<long>(std::floor(local / cycle)));
        within = local - static_cast<double>(cycleIndex) * cycle;
    }
    // 지금 보이는 클립 = 시작 시각이 지난 마지막 클립 (페이드 중이면 새 클립)
    long current = 0;
    for (long i = 0; i < n; ++i)
        if (layer.clips[order[static_cast<std::size_t>(i)]].start <= within + 1e-9)
            current = i;

    long target = current + delta;
    if (layer.repeat) {
        while (target >= n) {
            target -= n;
            ++cycleIndex;
        }
        while (target < 0) {
            target += n;
            --cycleIndex;
        }
        if (cycleIndex < 0) {
            cycleIndex = 0;
            target = 0;
        }
    } else {
        target = std::clamp(target, 0L, n - 1);
    }
    const double targetLocal = static_cast<double>(cycleIndex) * cycle +
                               layer.clips[order[static_cast<std::size_t>(target)]].start;
    m_layerOffset[layer.id] = t - targetLocal;
    closeLayer(layer.id);
    logInfo("'" + layer.name + "' " + (delta > 0 ? "다음" : "이전") + " 항목");
    return true;
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
    // 영역은 프로젝트 순서대로 (뒤쪽 영역이 앞), 영역 안에서는 맨 아래 레이어(목록 끝)부터 그림
    for (const Region& region : m_regionOrder) {
        if (!region.visible || region.rect.isEmpty())
            continue;
        for (auto layerIt = m_timeline.layers.rbegin(); layerIt != m_timeline.layers.rend(); ++layerIt) {
            const TimelineLayer& layer = *layerIt;
            if (layer.regionId != region.id || !layerShowsVideo(layer))
                continue;

            std::vector<const Instance*> items;
            for (const auto& [id, inst] : m_instances)
                if (inst.layerId == layer.id && (inst.video || inst.ticker))
                    items.push_back(&inst);
            if (items.empty())
                continue;
            std::sort(items.begin(), items.end(),
                      [](const Instance* a, const Instance* b) { return a->start < b->start; });

            RegionView base;
            base.rect = region.rect;
            base.fit = region.fit;
            base.opacity = static_cast<float>(std::clamp(region.opacity, 0.0, 1.0));

            RegionView clips = base;
            auto flushClips = [&]() {
                if (!clips.clips.empty()) {
                    out.push_back(std::move(clips));
                    clips = base;
                }
            };
            for (const Instance* inst : items) {
                ClipFade fade;
                fade.timeline = m_clock;
                fade.clipStart = inst->start;
                fade.clipEnd = inst->showEnd;
                fade.fadeIn = inst->clip.fadeIn;
                fade.fadeOut = inst->showEnd >= kForever ? 0.0 : inst->clip.fadeOut;
                fade.clipOpacity = inst->clip.opacity;
                if (inst->ticker) {
                    // 자막은 따로 그림 (앞 클립 위)
                    flushClips();
                    RegionView tv = base;
                    tv.ticker = inst->ticker->view();
                    tv.tickerFade = fade;
                    out.push_back(std::move(tv));
                    continue;
                }
                ClipView c;
                c.source = inst->video;
                c.key = inst->key;
                c.fade = fade;
                c.sourceZero = inst->sourceZero;
                clips.clips.push_back(std::move(c));
            }
            flushClips();
        }
    }
}

} // namespace led
