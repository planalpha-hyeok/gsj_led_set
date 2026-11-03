#include "display/DisplayInfo.h"

namespace led {

const DisplayInfo* findDisplay(const std::vector<DisplayInfo>& displays, const std::string& id)
{
    for (const auto& d : displays)
        if (d.id == id)
            return &d;
    return nullptr;
}

} // namespace led
