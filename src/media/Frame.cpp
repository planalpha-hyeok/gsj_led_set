#include "media/Frame.h"

#include <atomic>

namespace led {

std::uint64_t nextFrameSerial()
{
    static std::atomic<std::uint64_t> counter{0};
    return ++counter;
}

} // namespace led
