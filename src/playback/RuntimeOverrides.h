#pragma once

// 외부 명령으로 바꾼 재생 상태 (저장되지 않음)
//  - 영역 표시/숨김
//  - 자막 문구
// 재생 엔진에는 apply()로 만든 사본 프로젝트를 넘김

#include "core/Project.h"

#include <map>
#include <string>
#include <vector>

namespace led {

class RuntimeOverrides {
public:
    void setVisible(const std::string& regionId, bool visible) { m_visible[regionId] = visible; }
    void setTickerLines(const std::string& regionId, std::vector<std::string> lines)
    {
        m_tickerLines[regionId] = std::move(lines);
    }
    void clear()
    {
        m_visible.clear();
        m_tickerLines.clear();
    }
    bool empty() const { return m_visible.empty() && m_tickerLines.empty(); }

    // project 사본에 덮어쓰기를 적용해 반환
    Project apply(const Project& project) const;

private:
    std::map<std::string, bool> m_visible;
    std::map<std::string, std::vector<std::string>> m_tickerLines;
};

} // namespace led
