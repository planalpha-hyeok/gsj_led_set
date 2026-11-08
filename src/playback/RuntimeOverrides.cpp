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
        // 모든 장면에서 이 영역의 자막을 바꿈 (자막 영역이 아니던 곳은 자막으로 바뀜)
        for (Scene& scene : p.scenes) {
            RegionContent* c = scene.contentFor(regionId);
            if (!c) {
                RegionContent nc;
                nc.regionId = regionId;
                nc.kind = ContentKind::Ticker;
                scene.contents.push_back(nc);
                c = &scene.contents.back();
            }
            c->kind = ContentKind::Ticker;
            c->ticker.lines = lines;
            c->ticker.textFile.clear(); // 외부 명령 문구가 파일보다 우선
        }
    }
    return p;
}

} // namespace led
