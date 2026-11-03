#include "output/win32/Win32OutputWindow.h"

#include "core/Log.h"
#include "render/d3d11/D3D11Renderer.h"

#include <chrono>
#include <limits>
#include <mutex>
#include <vector>

namespace led {

namespace {

const wchar_t kWindowClass[] = L"GsjLedOutputWindow";
constexpr int kMaxDeviceRecoveries = 5;
constexpr int kStartTimeoutMs = 10000;
constexpr std::uint64_t kOverlayKey = std::numeric_limits<std::uint64_t>::max();

std::wstring toWide(const std::string& text)
{
    if (text.empty())
        return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (len <= 1)
        return {};
    std::wstring out(static_cast<std::size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &out[0], len);
    return out;
}

std::string lastErrorText()
{
    return std::to_string(static_cast<unsigned long>(GetLastError()));
}

// 이 스레드의 좌표를 물리 픽셀로 고정 (Windows 10 1607 이상에서만 동작, 그 외는 프로세스 설정을 따름)
void makeThreadPerMonitorDpiAware()
{
    using SetContextFn = HANDLE(WINAPI*)(HANDLE);
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32)
        return;
    const auto fn = reinterpret_cast<SetContextFn>(
        reinterpret_cast<void*>(GetProcAddress(user32, "SetThreadDpiAwarenessContext")));
    if (fn)
        fn(reinterpret_cast<HANDLE>(static_cast<LONG_PTR>(-4))); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
}

LRESULT CALLBACK outputWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(nullptr); // 출력 화면 위에서 마우스 커서 숨김
            return TRUE;
        }
        break;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE; // 클릭해도 편집 창의 포커스를 뺏지 않음
    case WM_CLOSE:
        return 0;             // Alt+F4 등으로 닫히지 않게 함 (프로그램에서만 닫음)
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool registerWindowClass()
{
    static std::once_flag flag;
    static bool ok = false;
    std::call_once(flag, [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = outputWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = nullptr;
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = kWindowClass;
        ok = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    });
    return ok;
}

} // namespace

std::unique_ptr<OutputWindow> createOutputWindow()
{
    return std::make_unique<Win32OutputWindow>();
}

Win32OutputWindow::~Win32OutputWindow()
{
    stop();
}

bool Win32OutputWindow::start(const OutputSpec& spec, SceneProvider* scene, std::string& error)
{
    stop();
    if (spec.canvasRect.isEmpty()) {
        error = "출력 크기가 0";
        return false;
    }
    m_spec = spec;
    m_scene = scene;
    m_stopRequested = false;

    std::promise<std::string> promise;
    std::future<std::string> result = promise.get_future();
    m_thread = std::thread([this, p = std::move(promise)]() mutable { threadMain(std::move(p)); });

    if (result.wait_for(std::chrono::milliseconds(kStartTimeoutMs)) != std::future_status::ready) {
        error = "출력 창 시작 시간 초과";
        stop();
        return false;
    }
    error = result.get();
    if (!error.empty()) {
        stop();
        return false;
    }
    return true;
}

void Win32OutputWindow::stop()
{
    m_stopRequested = true;
    if (m_thread.joinable())
        m_thread.join();
}

void Win32OutputWindow::setOverlay(std::shared_ptr<const Frame> overlay)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_overlay = std::move(overlay);
}

std::string Win32OutputWindow::adapterName() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_adapterName;
}

void Win32OutputWindow::threadMain(std::promise<std::string> initResult)
{
    makeThreadPerMonitorDpiAware();

    if (!registerWindowClass()) {
        initResult.set_value("창 클래스 등록 실패 (오류 " + lastErrorText() + ")");
        return;
    }

    const int width = m_spec.canvasRect.w;
    const int height = m_spec.canvasRect.h;
    const std::wstring title = toWide("LED 출력 - " + m_spec.name);

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | (m_spec.topmost ? WS_EX_TOPMOST : 0),
                                kWindowClass, title.c_str(), WS_POPUP, m_spec.windowX, m_spec.windowY, width, height,
                                nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        initResult.set_value("출력 창 생성 실패 (오류 " + lastErrorText() + ")");
        return;
    }
    // 정확한 물리 픽셀 위치/크기로 다시 지정
    SetWindowPos(hwnd, m_spec.topmost ? HWND_TOPMOST : HWND_TOP, m_spec.windowX, m_spec.windowY, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    D3D11Renderer renderer;
    std::string error;
    if (!renderer.initialize(hwnd, width, height, error)) {
        DestroyWindow(hwnd);
        initResult.set_value(error);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_adapterName = renderer.adapterName();
    }
    initResult.set_value(std::string());
    logInfo("출력 시작: " + m_spec.name + " (" + std::to_string(width) + "x" + std::to_string(height) + " @ " +
            std::to_string(m_spec.windowX) + "," + std::to_string(m_spec.windowY) + ", " + renderer.adapterName() +
            ")");

    std::vector<SceneLayer> layers;
    std::vector<DrawItem> items;
    int recoveries = 0;

    while (!m_stopRequested) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        std::shared_ptr<const Frame> overlay;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            overlay = m_overlay;
        }

        if (overlay) {
            items.clear();
            DrawItem item;
            item.key = kOverlayKey;
            item.dst = RectF{0.0, 0.0, double(width), double(height)};
            item.uv = RectF{0.0, 0.0, 1.0, 1.0};
            item.frame = std::move(overlay);
            items.push_back(std::move(item));
        } else {
            if (m_scene)
                m_scene->collect(layers);
            else
                layers.clear();
            buildDrawItems(layers, m_spec.canvasRect, items);
        }

        const RenderResult result = renderer.render(items, m_spec.vsync);
        if (result == RenderResult::Occluded) {
            Sleep(16);
        } else if (result == RenderResult::DeviceLost) {
            if (++recoveries > kMaxDeviceRecoveries) {
                logError("출력 중지: " + m_spec.name + " (그래픽 장치 복구 실패 반복)");
                break;
            }
            logWarning("그래픽 장치 재생성 시도: " + m_spec.name);
            Sleep(500);
            if (!renderer.initialize(hwnd, width, height, error)) {
                logError("그래픽 장치 재생성 실패: " + error);
                Sleep(1000);
            }
        }
        if (!m_spec.vsync)
            Sleep(1);
    }

    renderer.shutdown();
    DestroyWindow(hwnd);
    logInfo("출력 종료: " + m_spec.name);
}

} // namespace led
