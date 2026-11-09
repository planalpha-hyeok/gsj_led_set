#include "playback/RegionView.h"

#include <algorithm>
#include <atomic>

namespace led {

std::uint64_t nextRenderKey()
{
    static std::atomic<std::uint64_t> counter{1};
    return counter++;
}

double ClipFade::alphaAt(double t) const
{
    if (t < clipStart || t >= clipEnd)
        return 0.0;
    double a = 1.0;
    if (fadeIn > 0.0)
        a = std::min(a, (t - clipStart) / fadeIn);
    if (fadeOut > 0.0)
        a = std::min(a, (clipEnd - t) / fadeOut);
    return clipOpacity * std::clamp(a, 0.0, 1.0);
}

} // namespace led
