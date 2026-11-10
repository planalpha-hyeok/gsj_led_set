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

// 이동: 앞/가운데/뒤 중 기준선에 가장 가까운 것에 맞춰 전체를 옮길 거리
bool moveDelta(double lo, double hi, const std::vector<double>& lines, double threshold, double& delta,
               double& guide)
{
    const double cand[3] = {lo, (lo + hi) / 2.0, hi};
    bool found = false;
    double bestAbs = threshold + 1.0;
    for (double v : cand) {
        double d = 0.0;
        if (nearest(v, lines, threshold, d) && std::fabs(d) < bestAbs) {
            bestAbs = std::fabs(d);
            delta = d;
            guide = v + d;
            found = true;
        }
    }
    return found;
}

// 크기 변경: 움직이는 테두리 1개를 붙임 (붙인 뒤 길이가 1px 미만이면 붙이지 않음)
bool edgeSnap(double& lo, double& hi, bool moveLo, bool moveHi, const std::vector<double>& lines,
              double threshold, double& guide)
{
    double d = 0.0;
    if (moveLo && nearest(lo, lines, threshold, d)) {
        if (roundInt(hi) - roundInt(lo + d) >= 1) {
            lo += d;
            guide = lo;
            return true;
        }
    } else if (moveHi && nearest(hi, lines, threshold, d)) {
        if (roundInt(hi + d) - roundInt(lo) >= 1) {
            hi += d;
            guide = hi;
            return true;
        }
    }
    return false;
}

} // namespace

SnapResult snapRect(const RectF& input, bool move, int edges, const std::vector<SnapLines>& groups, double threshold)
{
    SnapResult out;
    double left = input.x;
    double right = input.x + input.w;
    double top = input.y;
    double bottom = input.y + input.h;

    if (threshold > 0.0) {
        // 가로/세로 각각 우선순위가 높은 그룹부터 시도
        for (const SnapLines& g : groups) {
            if (out.snappedX)
                break;
            if (move) {
                double d = 0.0;
                if (moveDelta(left, right, g.xs, threshold, d, out.guideX)) {
                    left += d;
                    right += d;
                    out.snappedX = true;
                }
            } else {
                out.snappedX = edgeSnap(left, right, edges & SnapLeft, edges & SnapRight, g.xs, threshold, out.guideX);
            }
        }
        for (const SnapLines& g : groups) {
            if (out.snappedY)
                break;
            if (move) {
                double d = 0.0;
                if (moveDelta(top, bottom, g.ys, threshold, d, out.guideY)) {
                    top += d;
                    bottom += d;
                    out.snappedY = true;
                }
            } else {
                out.snappedY = edgeSnap(top, bottom, edges & SnapTop, edges & SnapBottom, g.ys, threshold, out.guideY);
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

SnapResult snapRect(const RectF& rect, bool move, int edges, const std::vector<double>& xs,
                    const std::vector<double>& ys, double threshold)
{
    return snapRect(rect, move, edges, std::vector<SnapLines>{SnapLines{xs, ys}}, threshold);
}

} // namespace led
