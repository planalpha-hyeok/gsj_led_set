#include "playback/RuntimeOverrides.h"

namespace led {

Project RuntimeOverrides::apply(const Project& project) const
{
    Project p = project;
    for (Region& r : p.regions) {
        const auto v = m_visible.find(r.id);
        if (v != m_visible.end())
            r.visible = v->second;
    }
    for (const auto& [regionId, lines] : m_tickerLines) {
        if (!p.findRegion(regionId))
            continue;
        // 모든 장면에서 이 영역 레이어의 자막 클립 문구를 바꿈
        for (Scene& scene : p.scenes)
            for (TimelineLayer& layer : scene.timeline.layers) {
                if (layer.regionId != regionId)
                    continue;
                for (TimelineClip& clip : layer.clips) {
                    if (clip.kind != ClipKind::Ticker)
                        continue;
                    clip.ticker.lines = lines;
                    clip.ticker.textFile.clear(); // 외부 명령 문구가 파일보다 우선
                }
            }
    }
    return p;
}

bool hasTickerClip(const Project& project, const std::string& regionId)
{
    for (const Scene& scene : project.scenes)
        for (const TimelineLayer& layer : scene.timeline.layers)
            if (layer.regionId == regionId)
                for (const TimelineClip& clip : layer.clips)
                    if (clip.kind == ClipKind::Ticker)
                        return true;
    return false;
}

} // namespace led
