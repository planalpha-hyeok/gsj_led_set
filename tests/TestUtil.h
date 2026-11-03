#pragma once

// 외부 라이브러리 없이 쓰는 간단한 검사 도구

#include <cmath>
#include <cstdio>

namespace test {

inline int& failures()
{
    static int count = 0;
    return count;
}

inline void check(bool condition, const char* expr, const char* file, int line)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL %s:%d  %s\n", file, line, expr);
        ++failures();
    }
}

inline bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

inline int finish(const char* name)
{
    if (failures() == 0)
        std::printf("%s: 모두 통과\n", name);
    else
        std::printf("%s: 실패 %d건\n", name, failures());
    return failures() == 0 ? 0 : 1;
}

} // namespace test

#define CHECK(expr) ::test::check((expr), #expr, __FILE__, __LINE__)
