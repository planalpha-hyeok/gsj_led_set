#pragma once

// 타임라인 시계: 재생/일시정지/이동. 여러 스레드에서 읽음

#include <chrono>
#include <mutex>

namespace led {

class TimelineClock {
public:
    using Clock = std::chrono::steady_clock;

    double time() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return timeLocked(Clock::now());
    }

    bool running() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_running;
    }

    void start(double at)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_anchorTime = at;
        m_anchor = Clock::now();
        m_running = true;
    }

    void pause()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_anchorTime = timeLocked(Clock::now());
        m_running = false;
    }

    // 재생 상태는 그대로 두고 위치만 이동
    void seek(double t)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_anchorTime = t;
        m_anchor = Clock::now();
    }

    void pauseAt(double t)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_anchorTime = t;
        m_running = false;
    }

private:
    double timeLocked(Clock::time_point now) const
    {
        if (!m_running)
            return m_anchorTime;
        return m_anchorTime + std::chrono::duration<double>(now - m_anchor).count();
    }

    mutable std::mutex m_mutex;
    bool m_running = false;
    Clock::time_point m_anchor = Clock::now();
    double m_anchorTime = 0.0;
};

} // namespace led
