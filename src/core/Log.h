#pragma once

// 스레드 안전 로그 모듈 (표준 C++만 사용)
// 렌더 스레드, 디코딩 스레드에서도 호출 가능

#include <functional>
#include <string>

namespace led {

enum class LogLevel { Info, Warning, Error };

using LogSink = std::function<void(LogLevel, const std::string&)>;

// 싱크는 내부 뮤텍스 안에서 호출됨
void setLogSink(LogSink sink);
void logMessage(LogLevel level, const std::string& message);

inline void logInfo(const std::string& m) { logMessage(LogLevel::Info, m); }
inline void logWarning(const std::string& m) { logMessage(LogLevel::Warning, m); }
inline void logError(const std::string& m) { logMessage(LogLevel::Error, m); }

} // namespace led
