#include "core/LayerContent.h"

#include <algorithm>
#include <cmath>

namespace led {

namespace {

double fallbackDuration(const TimelineClip& c)
{
    switch (c.kind) {
    case ClipKind::Live: return kDefaultLiveDuration;
    case ClipKind::Ticker: return kDefaultTickerDuration;
    case ClipKind::File: break;
    }
    return kDefaultImageDuration;
}

} // namespace

void packLayer(TimelineLayer& layer, const DurationResolver& resolve)
{
    double prevEnd = 0.0;
    double prevDuration = 0.0;
    bool first = true;
    for (TimelineClip& c : layer.clips) {
        if (c.autoDuration || c.duration <= 0.0) {
            // 조회 함수가 없으면 이미 정해진 길이를 유지 (다시 배치만)
            const double d = resolve ? resolve(c) : 0.0;
            if (d > 0.0)
                c.duration = d;
            else if (resolve || c.duration <= 0.0)
                c.duration = fallbackDuration(c);
        }
        if (first) {
            c.start = 0.0;
            first = false;
        } else {
            // 페이드 전환: 앞 클립과 겹침 (두 클립 길이의 절반을 넘지 않게)
            const double overlap = std::clamp(c.fadeIn, 0.0, std::min(prevDuration, c.duration) / 2.0);
            c.start = std::max(0.0, prevEnd - overlap);
        }
        prevEnd = c.end();
        prevDuration = c.duration;
    }
    layer.needsPacking = false;
}

double layerCycleLength(const TimelineLayer& layer)
{
    const double end = layer.contentEnd();
    double overlap = 0.0;
    if (!layer.clips.empty()) {
        const auto first = std::min_element(layer.clips.begin(), layer.clips.end(),
                                            [](const TimelineClip& a, const TimelineClip& b) { return a.start < b.start; });
        if (first->start <= 0.0 && layer.clips.size() > 1)
            overlap = std::clamp(first->fadeIn, 0.0, first->duration / 2.0);
    }
    return std::max(0.1, end - overlap);
}

bool isContinuousLayer(const TimelineLayer& layer)
{
    if (!layer.repeat || layer.clips.size() != 1)
        return false;
    const TimelineClip& c = layer.clips.front();
    // 파일 길이를 직접 정한 클립은 그 길이로 반복 (자동 길이 = 파일 전체를 끊김 없이 반복)
    return c.start <= 0.0 && (c.kind != ClipKind::File || c.autoDuration);
}

int playlistLayerIndex(const Timeline& timeline, const std::string& regionId)
{
    if (regionId.empty())
        return -1;
    for (int i = 0; i < static_cast<int>(timeline.layers.size()); ++i)
        if (timeline.layers[static_cast<std::size_t>(i)].regionId == regionId)
            return i;
    return -1;
}

bool isPlaylistCompatible(const TimelineLayer& layer)
{
    if (layer.clips.empty())
        return true;
    if (layer.clips.size() == 1 && layer.clips.front().kind == ClipKind::Ticker)
        return layer.clips.front().start <= 1e-3;

    std::vector<const TimelineClip*> clips;
    for (const auto& c : layer.clips) {
        if (c.kind == ClipKind::Ticker)
            return false;
        clips.push_back(&c);
    }
    std::stable_sort(clips.begin(), clips.end(),
                     [](const TimelineClip* a, const TimelineClip* b) { return a->start < b->start; });
    constexpr double kTolerance = 1e-3;
    double prevEnd = 0.0;
    double prevDuration = 0.0;
    for (std::size_t i = 0; i < clips.size(); ++i) {
        const TimelineClip& c = *clips[i];
        double expected = 0.0;
        if (i > 0) {
            const double overlap = std::clamp(c.fadeIn, 0.0, std::min(prevDuration, c.duration) / 2.0);
            expected = std::max(0.0, prevEnd - overlap);
        }
        if (std::abs(c.start - expected) > kTolerance)
            return false;
        prevEnd = c.end();
        prevDuration = c.duration;
    }
    return true;
}

RegionContent layerToContent(const TimelineLayer* layer, const std::string& regionId)
{
    RegionContent c;
    c.regionId = regionId;
    c.kind = ContentKind::Playlist;
    c.loopPlaylist = true;
    if (!layer)
        return c;
    c.loopPlaylist = layer->repeat;

    std::vector<const TimelineClip*> clips;
    for (const auto& clip : layer->clips)
        clips.push_back(&clip);
    std::stable_sort(clips.begin(), clips.end(),
                     [](const TimelineClip* a, const TimelineClip* b) { return a->start < b->start; });

    if (clips.size() == 1 && clips.front()->kind == ClipKind::Ticker) {
        c.kind = ContentKind::Ticker;
        c.ticker = clips.front()->ticker;
        return c;
    }
    for (const TimelineClip* clip : clips) {
        if (clip->kind == ClipKind::Ticker)
            continue; // 섞여 있는 자막 클립은 타임라인에서만 편집
        PlaylistItem item;
        item.id = clip->id;
        item.type = clip->kind == ClipKind::Live ? ItemType::Live : ItemType::File;
        item.source = clip->source;
        item.liveSize = clip->liveSize;
        item.liveFps = clip->liveFps;
        item.duration = clip->autoDuration ? 0.0 : clip->duration;
        item.transition = clip->fadeIn > 0.0 ? TransitionType::Fade : TransitionType::Cut;
        item.transitionDuration = clip->fadeIn > 0.0 ? clip->fadeIn : 1.0;
        c.items.push_back(std::move(item));
    }
    return c;
}

void contentToLayer(const RegionContent& content, TimelineLayer& layer, const DurationResolver& resolve)
{
    const std::vector<TimelineClip> old = layer.clips;
    auto existing = [&](const std::string& id) -> const TimelineClip* {
        for (const auto& c : old)
            if (c.id == id)
                return &c;
        return nullptr;
    };

    if (content.kind == ContentKind::Ticker) {
        TimelineClip clip;
        for (const auto& c : old)
            if (c.kind == ClipKind::Ticker) {
                clip = c;
                break;
            }
        if (clip.id.empty()) {
            clip.id = generateId();
            clip.duration = kDefaultTickerDuration;
        }
        clip.kind = ClipKind::Ticker;
        clip.ticker = content.ticker;
        clip.start = 0.0;
        clip.autoDuration = false;
        layer.clips = {clip};
        layer.repeat = true; // 자막은 끊김 없이 계속
        layer.holdLast = false;
        return;
    }

    std::vector<TimelineClip> clips;
    for (const PlaylistItem& item : content.items) {
        TimelineClip clip;
        if (const TimelineClip* prev = existing(item.id))
            clip = *prev; // 불투명도/소리/자르기 등 유지
        else
            clip.id = item.id.empty() ? generateId() : item.id;
        clip.kind = item.type == ItemType::Live ? ClipKind::Live : ClipKind::File;
        clip.source = item.source;
        clip.liveSize = item.liveSize;
        clip.liveFps = item.liveFps;
        clip.autoDuration = item.duration <= 0.0;
        if (!clip.autoDuration)
            clip.duration = item.duration;
        clip.fadeIn = item.transition == TransitionType::Fade ? item.transitionDuration : 0.0;
        clips.push_back(std::move(clip));
    }
    const bool wasRepeat = layer.repeat;
    const bool wasEmpty = old.empty();
    layer.clips = std::move(clips);
    layer.repeat = content.loopPlaylist;
    if (layer.repeat)
        layer.holdLast = false;
    else if (wasRepeat || wasEmpty)
        layer.holdLast = true; // 반복을 끄면 마지막 화면 유지 (그 외에는 기존 설정 유지)
    packLayer(layer, resolve);
}

} // namespace led
