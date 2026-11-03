#include "core/TickerMotion.h"

#include <algorithm>
#include <cmath>

namespace led {

void tickerCopyPositions(double distance, double regionStart, double regionLength, double stripLength,
                         TickerDirection direction, std::vector<double>& out)
{
    out.clear();
    if (stripLength <= 0.0 || regionLength <= 0.0 || distance < 0.0)
        return;

    const double regionEnd = regionStart + regionLength;
    const bool forward = direction == TickerDirection::Left || direction == TickerDirection::Up;

    // 이미 들어온 사본 번호 k = 0 .. kMax
    const long long kMax = static_cast<long long>(std::floor(distance / stripLength));
    const long long kMin =
        std::max(0LL, static_cast<long long>(std::floor((distance - regionLength - stripLength) / stripLength)));

    for (long long k = kMin; k <= kMax; ++k) {
        double pos;
        if (forward) // 왼쪽/위로: 영역 끝에서 들어와 시작 쪽으로 이동
            pos = regionEnd - distance + static_cast<double>(k) * stripLength;
        else         // 오른쪽/아래로: 영역 시작에서 들어와 끝 쪽으로 이동
            pos = regionStart - stripLength + distance - static_cast<double>(k) * stripLength;
        if (pos < regionEnd && pos + stripLength > regionStart)
            out.push_back(pos);
    }
}

} // namespace led
