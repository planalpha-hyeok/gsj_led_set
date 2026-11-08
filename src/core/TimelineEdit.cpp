#include "core/TimelineEdit.h"

#include <algorithm>
#include <cmath>

namespace led {

ClipLocation findClip(const Timeline& tl, const std::string& clipId)
{
    for (int l = 0; l < static_cast<int>(tl.layers.size()); ++l) {
        const auto& clips = tl.layers[static_cast<std::size_t>(l)].clips;
        for (int i = 0; i < static_cast<int>(clips.size()); ++i)
            if (clips[static_cast<std::size_t>(i)].id == clipId)
                return ClipLocation{l, i};
    }
    return {};
}

TimelineClip* clipById(Timeline& tl, const std::string& clipId)
{
    const ClipLocation loc = findClip(tl, clipId);
    return loc.valid() ? &tl.layers[static_cast<std::size_t>(loc.layer)].clips[static_cast<std::size_t>(loc.index)]
                       : nullptr;
}

const TimelineClip* clipById(const Timeline& tl, const std::string& clipId)
{
    const ClipLocation loc = findClip(tl, clipId);
    return loc.valid() ? &tl.layers[static_cast<std::size_t>(loc.layer)].clips[static_cast<std::size_t>(loc.index)]
                       : nullptr;
}

std::vector<double> timelineSnapPoints(const Timeline& tl, const std::set<std::string>& exclude, double playhead)
{
    std::vector<double> points{0.0, playhead};
    for (const auto& cue : tl.cues)
        points.push_back(cue.time);
    for (const auto& layer : tl.layers)
        for (const auto& clip : layer.clips)
            if (!exclude.count(clip.id)) {
                points.push_back(clip.start);
                points.push_back(clip.end());
            }
    return points;
}

double snapTime(double value, const std::vector<double>& points, double threshold, bool* snapped)
{
    double best = value;
    double bestDist = threshold;
    bool found = false;
    for (double p : points) {
        const double d = std::fabs(p - value);
        if (d <= bestDist) {
            bestDist = d;
            best = p;
            found = true;
        }
    }
    if (snapped)
        *snapped = found;
    return best;
}

bool moveClips(Timeline& tl, const std::vector<std::string>& clipIds, double deltaTime, int deltaLayer)
{
    if (clipIds.empty())
        return false;
    const int layerCount = static_cast<int>(tl.layers.size());
    double minStart = 1e300;
    std::vector<ClipLocation> locs;
    for (const auto& id : clipIds) {
        const ClipLocation loc = findClip(tl, id);
        if (!loc.valid())
            return false;
        const int target = loc.layer + deltaLayer;
        if (target < 0 || target >= layerCount)
            return false;
        if (tl.layers[static_cast<std::size_t>(loc.layer)].locked || tl.layers[static_cast<std::size_t>(target)].locked)
            return false;
        minStart = std::min(minStart, tl.layers[static_cast<std::size_t>(loc.layer)].clips[static_cast<std::size_t>(loc.index)].start);
        locs.push_back(loc);
    }
    if (minStart + deltaTime < 0.0)
        deltaTime = -minStart; // 0초 앞으로는 이동하지 않음

    // 클립을 꺼내서 옮긴 뒤 대상 레이어에 넣음
    std::vector<std::pair<int, TimelineClip>> moved;
    for (const auto& id : clipIds) {
        const ClipLocation loc = findClip(tl, id);
        auto& clips = tl.layers[static_cast<std::size_t>(loc.layer)].clips;
        TimelineClip c = clips[static_cast<std::size_t>(loc.index)];
        clips.erase(clips.begin() + loc.index);
        c.start = std::max(0.0, c.start + deltaTime);
        moved.emplace_back(loc.layer + deltaLayer, std::move(c));
    }
    for (auto& [layer, clip] : moved)
        tl.layers[static_cast<std::size_t>(layer)].clips.push_back(std::move(clip));
    for (auto& layer : tl.layers)
        std::stable_sort(layer.clips.begin(), layer.clips.end(),
                         [](const TimelineClip& a, const TimelineClip& b) { return a.start < b.start; });
    return true;
}

bool trimClipStart(Timeline& tl, const std::string& clipId, double newStart)
{
    const ClipLocation loc = findClip(tl, clipId);
    if (!loc.valid() || tl.layers[static_cast<std::size_t>(loc.layer)].locked)
        return false;
    TimelineClip& c = tl.layers[static_cast<std::size_t>(loc.layer)].clips[static_cast<std::size_t>(loc.index)];
    const double end = c.end();
    // 자르기 지점이 0 미만이 되지 않게, 최소 길이 유지
    newStart = std::max(newStart, c.start - c.inPoint);
    newStart = std::max(newStart, 0.0);
    newStart = std::min(newStart, end - kMinClipDuration);
    const double delta = newStart - c.start;
    c.inPoint = std::max(0.0, c.inPoint + delta);
    c.start = newStart;
    c.duration = end - newStart;
    return true;
}

bool trimClipEnd(Timeline& tl, const std::string& clipId, double newEnd, double maxDuration)
{
    const ClipLocation loc = findClip(tl, clipId);
    if (!loc.valid() || tl.layers[static_cast<std::size_t>(loc.layer)].locked)
        return false;
    TimelineClip& c = tl.layers[static_cast<std::size_t>(loc.layer)].clips[static_cast<std::size_t>(loc.index)];
    double duration = std::max(kMinClipDuration, newEnd - c.start);
    if (maxDuration > 0.0)
        duration = std::min(duration, std::max(kMinClipDuration, maxDuration));
    c.duration = duration;
    return true;
}

int deleteClips(Timeline& tl, const std::vector<std::string>& clipIds)
{
    int removed = 0;
    for (auto& layer : tl.layers) {
        if (layer.locked)
            continue;
        const auto before = layer.clips.size();
        layer.clips.erase(std::remove_if(layer.clips.begin(), layer.clips.end(),
                                         [&](const TimelineClip& c) {
                                             return std::find(clipIds.begin(), clipIds.end(), c.id) != clipIds.end();
                                         }),
                          layer.clips.end());
        removed += static_cast<int>(before - layer.clips.size());
    }
    return removed;
}

bool addClip(Timeline& tl, int layer, const TimelineClip& clip)
{
    if (layer < 0 || layer >= static_cast<int>(tl.layers.size()) || tl.layers[static_cast<std::size_t>(layer)].locked)
        return false;
    auto& clips = tl.layers[static_cast<std::size_t>(layer)].clips;
    clips.push_back(clip);
    std::stable_sort(clips.begin(), clips.end(),
                     [](const TimelineClip& a, const TimelineClip& b) { return a.start < b.start; });
    return true;
}

} // namespace led
