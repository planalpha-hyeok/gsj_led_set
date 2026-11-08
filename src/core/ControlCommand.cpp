#include "core/ControlCommand.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace led {

namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string lower(const std::string& s)
{
    std::string out = s;
    for (char& c : out)
        if (static_cast<unsigned char>(c) < 0x80)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// 감싼 따옴표 제거: "abc" -> abc
std::string unquote(const std::string& s)
{
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
        return s.substr(1, s.size() - 2);
    return s;
}

// 첫 인수를 떼어 냄 (따옴표 지원). rest에는 나머지
std::string takeArg(std::string& rest)
{
    rest = trimText(rest);
    if (rest.empty())
        return {};
    std::string arg;
    if (rest[0] == '"' || rest[0] == '\'') {
        const char q = rest[0];
        const std::size_t end = rest.find(q, 1);
        if (end == std::string::npos) {
            arg = rest.substr(1);
            rest.clear();
        } else {
            arg = rest.substr(1, end - 1);
            rest = rest.substr(end + 1);
        }
        return arg;
    }
    std::size_t i = 0;
    while (i < rest.size() && !isSpace(rest[i]))
        ++i;
    arg = rest.substr(0, i);
    rest = rest.substr(i);
    return arg;
}

} // namespace

bool parseTimeText(const std::string& input, double& seconds)
{
    const std::string text = trimText(input);
    if (text.empty())
        return false;
    double total = 0.0;
    int parts = 0;
    std::size_t start = 0;
    while (true) {
        const std::size_t colon = text.find(':', start);
        const std::string part = text.substr(start, colon == std::string::npos ? std::string::npos : colon - start);
        if (part.empty())
            return false;
        for (char c : part)
            if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.'))
                return false;
        char* end = nullptr;
        const double v = std::strtod(part.c_str(), &end);
        if (!end || *end != '\0' || v < 0.0)
            return false;
        // 마지막 부분이 아닌 시/분은 정수, 분/초는 60 미만
        if (colon != std::string::npos && part.find('.') != std::string::npos)
            return false;
        if (parts > 0 && v >= 60.0)
            return false;
        total = total * 60.0 + v;
        ++parts;
        if (parts > 3)
            return false;
        if (colon == std::string::npos)
            break;
        start = colon + 1;
    }
    seconds = total;
    return true;
}

std::string formatTimeText(double seconds)
{
    if (seconds < 0.0)
        seconds = 0.0;
    const long long tenths = std::llround(seconds * 10.0);
    const long long h = tenths / 36000;
    const long long m = (tenths / 600) % 60;
    const double s = static_cast<double>(tenths % 600) / 10.0;
    char buf[32];
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%04.1f", h, m, s);
    else
        std::snprintf(buf, sizeof(buf), "%lld:%04.1f", m, s);
    return buf;
}

std::string trimText(const std::string& s)
{
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && isSpace(s[b]))
        ++b;
    while (e > b && isSpace(s[e - 1]))
        --e;
    return s.substr(b, e - b);
}

bool parseCommand(const std::string& line, ControlCommand& out, std::string& error)
{
    std::string rest = trimText(line);
    if (rest.empty()) {
        error = "빈 명령";
        return false;
    }
    const std::string word = lower(takeArg(rest));
    rest = trimText(rest);
    const std::string target = trimText(unquote(rest));

    out = ControlCommand{};
    auto needTarget = [&](CommandType type, const char* what) {
        if (target.empty()) {
            error = std::string(what) + " 이름이 필요함";
            return false;
        }
        out.type = type;
        out.target = target;
        return true;
    };

    if (word == "scene" || word == "장면")
        return needTarget(CommandType::Scene, "장면");
    if (word == "play" || word == "재생") {
        out.type = CommandType::Play;
        return true;
    }
    if (word == "stop" || word == "정지") {
        out.type = CommandType::Stop;
        return true;
    }
    if (word == "reset" || word == "초기화") {
        out.type = CommandType::Reset;
        return true;
    }
    if (word == "pause" || word == "일시정지") {
        out.type = CommandType::Pause;
        return true;
    }
    if (word == "go" || word == "resume" || word == "계속") {
        out.type = CommandType::Go;
        return true;
    }
    if (word == "seek" || word == "이동") {
        out.type = CommandType::Seek;
        if (!parseTimeText(target, out.seconds)) {
            error = "시각 형식이 잘못됨 (예: 90, 1:30): " + target;
            return false;
        }
        return true;
    }
    if (word == "next" || word == "다음")
        return needTarget(CommandType::Next, "영역");
    if (word == "prev" || word == "previous" || word == "이전")
        return needTarget(CommandType::Prev, "영역");
    if (word == "show" || word == "표시")
        return needTarget(CommandType::Show, "영역");
    if (word == "hide" || word == "숨김")
        return needTarget(CommandType::Hide, "영역");
    if (word == "trigger" || word == "트리거")
        return needTarget(CommandType::Trigger, "트리거");
    if (word == "text" || word == "자막") {
        out.type = CommandType::Text;
        out.target = takeArg(rest);
        if (out.target.empty()) {
            error = "영역 이름이 필요함";
            return false;
        }
        // 나머지 전체가 문구. '|'로 여러 문구
        const std::string text = unquote(trimText(rest));
        std::size_t start = 0;
        while (start <= text.size()) {
            const std::size_t bar = text.find('|', start);
            const std::string part = trimText(text.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
            if (!part.empty())
                out.lines.push_back(part);
            if (bar == std::string::npos)
                break;
            start = bar + 1;
        }
        return true; // 문구가 비면 자막을 비움
    }
    error = "알 수 없는 명령: " + word;
    return false;
}

} // namespace led
