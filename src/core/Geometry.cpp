#include "core/Geometry.h"

#include <algorithm>

namespace led {

Placement computePlacement(const RectI& region, int contentWidth, int contentHeight, FitMode mode)
{
    Placement p;
    if (region.isEmpty() || contentWidth <= 0 || contentHeight <= 0)
        return p;

    const double rw = region.w;
    const double rh = region.h;
    const double cw = contentWidth;
    const double ch = contentHeight;

    p.dst = RectF{double(region.x), double(region.y), rw, rh};
    p.uv = RectF{0.0, 0.0, 1.0, 1.0};

    switch (mode) {
    case FitMode::Stretch:
        break;

    case FitMode::Fit: {
        const double scale = std::min(rw / cw, rh / ch);
        const double dw = cw * scale;
        const double dh = ch * scale;
        p.dst.x = region.x + (rw - dw) / 2.0;
        p.dst.y = region.y + (rh - dh) / 2.0;
        p.dst.w = dw;
        p.dst.h = dh;
        break;
    }

    case FitMode::Fill: {
        const double scale = std::max(rw / cw, rh / ch);
        const double vw = rw / scale; // 콘텐츠에서 보이는 너비(콘텐츠 픽셀)
        const double vh = rh / scale;
        p.uv.x = (cw - vw) / 2.0 / cw;
        p.uv.y = (ch - vh) / 2.0 / ch;
        p.uv.w = vw / cw;
        p.uv.h = vh / ch;
        break;
    }

    case FitMode::Original: {
        const double dw = std::min(cw, rw);
        const double dh = std::min(ch, rh);
        p.dst.w = dw;
        p.dst.h = dh;
        p.uv.w = dw / cw;
        p.uv.h = dh / ch;
        break;
    }
    }

    p.visible = !p.dst.isEmpty();
    return p;
}

bool intersects(const RectF& a, const RectF& b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

} // namespace led
