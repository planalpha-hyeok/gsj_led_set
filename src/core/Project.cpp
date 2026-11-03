#include "core/Project.h"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <random>

namespace led {

Region* Project::findRegion(const std::string& id)
{
    for (auto& r : regions)
        if (r.id == id)
            return &r;
    return nullptr;
}

const Region* Project::findRegion(const std::string& id) const
{
    for (const auto& r : regions)
        if (r.id == id)
            return &r;
    return nullptr;
}

OutputConfig* Project::findOutput(const std::string& id)
{
    for (auto& o : outputs)
        if (o.id == id)
            return &o;
    return nullptr;
}

const OutputConfig* Project::findOutput(const std::string& id) const
{
    for (const auto& o : outputs)
        if (o.id == id)
            return &o;
    return nullptr;
}

std::string generateId()
{
    static std::mutex mutex;
    static std::mt19937_64 rng(
        std::random_device{}() ^
        static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));

    unsigned long long value;
    {
        std::lock_guard<std::mutex> lock(mutex);
        value = rng();
    }
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", value);
    return std::string(buf);
}

const char* fitModeKey(FitMode mode)
{
    switch (mode) {
    case FitMode::Stretch: return "stretch";
    case FitMode::Fit: return "fit";
    case FitMode::Fill: return "fill";
    case FitMode::Original: return "original";
    }
    return "stretch";
}

bool fitModeFromKey(const std::string& key, FitMode& out)
{
    if (key == "stretch") { out = FitMode::Stretch; return true; }
    if (key == "fit") { out = FitMode::Fit; return true; }
    if (key == "fill") { out = FitMode::Fill; return true; }
    if (key == "original") { out = FitMode::Original; return true; }
    return false;
}

} // namespace led
