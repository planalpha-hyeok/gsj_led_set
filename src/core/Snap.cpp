#include "core/Snap.h"

#include <algorithm>
#include <cmath>

namespace led {

namespace {

// value에서 가장 가까운 기준선까지의 거리(부호 포함). threshold 안에 없으면 false
bool nearest(double value, const std::vector<double>& lines, double threshold, double& delta)
{
    bool found = false;
    double best = threshold;
    for (double line : lines) {
        const double d = line - value;
        if (std::fabs(d) <= best) {
            best = std::fabs(d);
            delta = d;
            found = true;
        }
    }
    return found;
}

int roundInt(double v)
{
    return static_cast<int>(std::lround(v));
}

} // namespace

SnapResult snapRect(const RectF& input, bool move, int edges, const std::vector<double>& xs,
                    const std::vector<double>& ys, double threshold)
{
    SnapResult out;
    double left = input.x;
    double right = input.x + input.w;
    double top = input.y;
    double bottom = input.y + input.h;

    if (threshold > 0.0) {
        if (move) {
            // 왼쪽/오른쪽 중 더 가까운 쪽에 맞춰 전체 이동
            double dl = 0.0;
            double dr = 0.0;
            const bool hl = nearest(left, xs, threshold, dl);
            const bool hr = nearest(right, xs, threshold, dr);
            if (hl || hr) {
                const double d = (hl && (!hr || std::fabs(dl) <= std::fabs(dr))) ? dl : dr;
                out.guideX = (hl && d == dl) ? left + d : right + d;
                left += d;
                right += d;
                out.snappedX = true;
            }
            double dt = 0.0;
            double db = 0.0;
            const bool ht = nearest(top, ys, threshold, dt);
            const bool hb = nearest(bottom, ys, threshold, db);
            if (ht || hb) {
                const double d = (ht && (!hb || std::fabs(dt) <= std::fabs(db))) ? dt : db;
                out.guideY = (ht && d == dt) ? top + d : bottom + d;
                top += d;
                bottom += d;
                out.snappedY = true;
            }
        } else {
            double d = 0.0;
            if ((edges & SnapLeft) && nearest(left, xs, threshold, d)) {
                left += d;
                out.snappedX = true;
                out.guideX = left;
            } else if ((edges & SnapRight) && nearest(right, xs, threshold, d)) {
                right += d;
                out.snappedX = true;
                out.guideX = right;
            }
            if ((edges & SnapTop) && nearest(top, ys, threshold, d)) {
                top += d;
                out.snappedY = true;
                out.guideY = top;
            } else if ((edges & SnapBottom) && nearest(bottom, ys, threshold, d)) {
                bottom += d;
                out.snappedY = true;
                out.guideY = bottom;
            }
        }
    }

    // 정수 픽셀로 맞추고 최소 크기 1 보장
    if (move) {
        const int w = std::max(1, roundInt(input.w));
        const int h = std::max(1, roundInt(input.h));
        out.rect = RectI{roundInt(left), roundInt(top), w, h};
    } else {
        int l = roundInt(left);
        int r = roundInt(right);
        int t = roundInt(top);
        int b = roundInt(bottom);
        if (r - l < 1) {
            if (edges & SnapLeft)
                l = r - 1;
            else
                r = l + 1;
        }
        if (b - t < 1) {
            if (edges & SnapTop)
                t = b - 1;
            else
                b = t + 1;
        }
        out.rect = RectI{l, t, r - l, b - t};
    }
    return out;
}

} // namespace led
