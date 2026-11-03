#include "render/Scene.h"

namespace led {

void buildDrawItems(const std::vector<SceneLayer>& layers, const RectI& outputCanvasRect,
                    std::vector<DrawItem>& out)
{
    out.clear();
    const RectF bounds{double(outputCanvasRect.x), double(outputCanvasRect.y), double(outputCanvasRect.w),
                       double(outputCanvasRect.h)};
    for (const SceneLayer& layer : layers) {
        if (!layer.frame || layer.dst.isEmpty() || layer.opacity <= 0.0f)
            continue;
        if (!intersects(layer.dst, bounds))
            continue;
        DrawItem item;
        item.key = layer.key;
        item.dst = RectF{layer.dst.x - outputCanvasRect.x, layer.dst.y - outputCanvasRect.y, layer.dst.w,
                         layer.dst.h};
        item.uv = layer.uv;
        item.opacity = layer.opacity;
        item.frame = layer.frame;
        out.push_back(std::move(item));
    }
}

} // namespace led
