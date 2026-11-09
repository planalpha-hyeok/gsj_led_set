#include "core/Project.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <random>

namespace led {

// ---------------------------------------------------------------- 비교

bool PlaylistItem::operator==(const PlaylistItem& o) const
{
    return id == o.id && type == o.type && source == o.source && liveSize == o.liveSize && liveFps == o.liveFps &&
           duration == o.duration && transition == o.transition && transitionDuration == o.transitionDuration;
}

bool TickerConfig::operator==(const TickerConfig& o) const
{
    return lines == o.lines && textFile == o.textFile && fontFamily == o.fontFamily && fontSize == o.fontSize &&
           bold == o.bold && textColor == o.textColor && backgroundColor == o.backgroundColor &&
           outlineWidth == o.outlineWidth && outlineColor == o.outlineColor && shadow == o.shadow &&
           shadowOffset == o.shadowOffset && shadowColor == o.shadowColor && speed == o.speed &&
           direction == o.direction && gap == o.gap;
}

bool TimelineClip::operator==(const TimelineClip& o) const
{
    return id == o.id && kind == o.kind && source == o.source && liveSize == o.liveSize && liveFps == o.liveFps &&
           ticker == o.ticker && start == o.start && duration == o.duration && autoDuration == o.autoDuration &&
           inPoint == o.inPoint && fadeIn == o.fadeIn && fadeOut == o.fadeOut && opacity == o.opacity &&
           volume == o.volume && loop == o.loop;
}

bool TimelineLayer::operator==(const TimelineLayer& o) const
{
    return id == o.id && name == o.name && regionId == o.regionId && visible == o.visible && muted == o.muted &&
           locked == o.locked && repeat == o.repeat && holdLast == o.holdLast && clips == o.clips;
}

double TimelineLayer::contentEnd() const
{
    double end = 0.0;
    for (const auto& clip : clips)
        end = std::max(end, clip.end());
    return end;
}

double Timeline::length() const
{
    double len = 0.0;
    for (const auto& layer : layers)
        if (!layer.repeat && !layer.holdLast)
            len = std::max(len, layer.contentEnd());
    return len;
}

double Timeline::contentEnd() const
{
    double len = 0.0;
    for (const auto& layer : layers)
        len = std::max(len, layer.contentEnd());
    return len;
}

bool Timeline::operator==(const Timeline& o) const
{
    return layers == o.layers && cues == o.cues && endAction == o.endAction;
}

bool RegionContent::isEmpty() const
{
    if (kind == ContentKind::Playlist)
        return items.empty();
    return ticker.lines.empty() && ticker.textFile.empty();
}

bool RegionContent::operator==(const RegionContent& o) const
{
    return regionId == o.regionId && kind == o.kind && items == o.items && loopPlaylist == o.loopPlaylist &&
           ticker == o.ticker;
}

// ---------------------------------------------------------------- 검색

namespace {
template <typename T>
T* findById(std::vector<T>& list, const std::string& id)
{
    for (auto& v : list)
        if (v.id == id)
            return &v;
    return nullptr;
}
template <typename T>
const T* findById(const std::vector<T>& list, const std::string& id)
{
    for (const auto& v : list)
        if (v.id == id)
            return &v;
    return nullptr;
}
} // namespace

Region* Project::findRegion(const std::string& id) { return findById(regions, id); }
const Region* Project::findRegion(const std::string& id) const { return findById(regions, id); }
OutputConfig* Project::findOutput(const std::string& id) { return findById(outputs, id); }
const OutputConfig* Project::findOutput(const std::string& id) const { return findById(outputs, id); }
Scene* Project::findScene(const std::string& id) { return findById(scenes, id); }
const Scene* Project::findScene(const std::string& id) const { return findById(scenes, id); }

// ---------------------------------------------------------------- ID

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

// ---------------------------------------------------------------- 문자열 변환

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

const char* transitionKey(TransitionType t)
{
    return t == TransitionType::Fade ? "fade" : "cut";
}

bool transitionFromKey(const std::string& key, TransitionType& out)
{
    if (key == "cut") { out = TransitionType::Cut; return true; }
    if (key == "fade") { out = TransitionType::Fade; return true; }
    return false;
}

const char* timelineEndKey(TimelineEnd e)
{
    switch (e) {
    case TimelineEnd::Hold: return "hold";
    case TimelineEnd::Loop: return "loop";
    case TimelineEnd::Stop: return "stop";
    }
    return "hold";
}

bool timelineEndFromKey(const std::string& key, TimelineEnd& out)
{
    if (key == "hold") { out = TimelineEnd::Hold; return true; }
    if (key == "loop") { out = TimelineEnd::Loop; return true; }
    if (key == "stop") { out = TimelineEnd::Stop; return true; }
    return false;
}

const char* decodeModeKey(DecodeMode m)
{
    switch (m) {
    case DecodeMode::Default: return "default";
    case DecodeMode::Hardware: return "hardware";
    case DecodeMode::Software: return "software";
    }
    return "default";
}

bool decodeModeFromKey(const std::string& key, DecodeMode& out)
{
    if (key == "default") { out = DecodeMode::Default; return true; }
    if (key == "hardware") { out = DecodeMode::Hardware; return true; }
    if (key == "software") { out = DecodeMode::Software; return true; }
    return false;
}

bool effectiveHardwareDecoding(const Project& project, const Region& region)
{
    switch (region.decode) {
    case DecodeMode::Hardware: return true;
    case DecodeMode::Software: return false;
    case DecodeMode::Default: break;
    }
    return project.hardwareDecoding;
}

const char* clipKindKey(ClipKind k)
{
    switch (k) {
    case ClipKind::File: return "file";
    case ClipKind::Live: return "live";
    case ClipKind::Ticker: return "ticker";
    }
    return "file";
}

bool clipKindFromKey(const std::string& key, ClipKind& out)
{
    if (key == "file") { out = ClipKind::File; return true; }
    if (key == "live") { out = ClipKind::Live; return true; }
    if (key == "ticker") { out = ClipKind::Ticker; return true; }
    return false;
}

const char* tickerDirectionKey(TickerDirection d)
{
    switch (d) {
    case TickerDirection::Left: return "left";
    case TickerDirection::Right: return "right";
    case TickerDirection::Up: return "up";
    case TickerDirection::Down: return "down";
    }
    return "left";
}

bool tickerDirectionFromKey(const std::string& key, TickerDirection& out)
{
    if (key == "left") { out = TickerDirection::Left; return true; }
    if (key == "right") { out = TickerDirection::Right; return true; }
    if (key == "up") { out = TickerDirection::Up; return true; }
    if (key == "down") { out = TickerDirection::Down; return true; }
    return false;
}

std::string colorToString(std::uint32_t argb)
{
    char buf[10];
    std::snprintf(buf, sizeof(buf), "#%08X", static_cast<unsigned>(argb));
    return buf;
}

bool colorFromString(const std::string& text, std::uint32_t& out)
{
    if (text.size() != 7 && text.size() != 9)
        return false;
    if (text[0] != '#')
        return false;
    std::uint32_t value = 0;
    for (std::size_t i = 1; i < text.size(); ++i) {
        const char c = text[i];
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        value = (value << 4) | static_cast<std::uint32_t>(digit);
    }
    if (text.size() == 7)
        value |= 0xFF000000u; // #RRGGBB = 불투명
    out = value;
    return true;
}

std::string minuteToString(int minute)
{
    if (minute < 0)
        minute = 0;
    minute %= 1440;
    char buf[6];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", minute / 60, minute % 60);
    return buf;
}

bool minuteFromString(const std::string& text, int& out)
{
    // 형식: H:MM 또는 HH:MM
    const std::size_t colon = text.find(':');
    if (colon == std::string::npos || colon == 0 || colon > 2 || text.size() != colon + 3)
        return false;
    auto digits = [&](std::size_t from, std::size_t to, int& value) {
        value = 0;
        for (std::size_t i = from; i < to; ++i) {
            if (text[i] < '0' || text[i] > '9')
                return false;
            value = value * 10 + (text[i] - '0');
        }
        return true;
    };
    int h = 0;
    int m = 0;
    if (!digits(0, colon, h) || !digits(colon + 1, text.size(), m))
        return false;
    if (h < 0 || h > 23 || m < 0 || m > 59)
        return false;
    out = h * 60 + m;
    return true;
}

} // namespace led
