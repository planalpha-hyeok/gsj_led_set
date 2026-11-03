#include "core/Log.h"

#include <cstdio>
#include <mutex>

namespace led {

namespace {
std::mutex& logMutex()
{
    static std::mutex m;
    return m;
}

LogSink& logSink()
{
    static LogSink s;
    return s;
}
} // namespace

void setLogSink(LogSink sink)
{
    std::lock_guard<std::mutex> lock(logMutex());
    logSink() = std::move(sink);
}

void logMessage(LogLevel level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(logMutex());
    if (logSink()) {
        logSink()(level, message);
        return;
    }
    const char* tag = level == LogLevel::Error ? "ERROR" : level == LogLevel::Warning ? "WARN" : "INFO";
    std::fprintf(stderr, "[%s] %s\n", tag, message.c_str());
}

} // namespace led
