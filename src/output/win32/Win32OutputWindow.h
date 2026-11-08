#pragma once

// Windows 출력 창: 테두리 없는 Win32 창 + 전용 렌더 스레드 + Direct3D 11
// 출력마다 스레드를 따로 두어 수직동기(VSync) 대기가 서로 영향을 주지 않음

#include "output/OutputWindow.h"

#include <atomic>
#include <future>
#include <mutex>
#include <thread>

namespace led {

class Win32OutputWindow final : public OutputWindow {
public:
    Win32OutputWindow() = default;
    ~Win32OutputWindow() override;

    bool start(const OutputSpec& spec, SceneProvider* scene, std::string& error) override;
    void stop() override;
    void setOverlay(std::shared_ptr<const Frame> overlay) override;
    std::string adapterName() const override;
    void setClickHandler(ClickHandler handler) override { m_clickHandler = std::move(handler); }

    // 창 메시지 처리에서 호출
    void notifyClick(int x, int y)
    {
        if (m_clickHandler)
            m_clickHandler(x, y);
    }

private:
    void threadMain(std::promise<std::string> initResult);

    OutputSpec m_spec;
    SceneProvider* m_scene = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_stopRequested{false};

    mutable std::mutex m_mutex;
    std::shared_ptr<const Frame> m_overlay;
    std::string m_adapterName;
    ClickHandler m_clickHandler;
};

} // namespace led
