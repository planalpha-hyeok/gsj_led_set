// Windows 전용: Direct3D 11 렌더러 실제 동작 검사
// 숨긴 창에 그린 뒤 화면 내용을 읽어 색/위치/불투명도가 맞는지 확인

#include "TestUtil.h"

#include "display/DisplayInfo.h"
#include "render/d3d11/D3D11Renderer.h"

#include <cstdio>
#include <cstdlib>

using namespace led;

namespace {

std::shared_ptr<Frame> solidFrame(std::uint8_t b, std::uint8_t g, std::uint8_t r, std::uint8_t a)
{
    auto f = std::make_shared<Frame>();
    f->width = 2;
    f->height = 2;
    f->stride = 8;
    f->premultiplied = true;
    f->serial = nextFrameSerial();
    for (int i = 0; i < 4; ++i) {
        f->pixels.push_back(b);
        f->pixels.push_back(g);
        f->pixels.push_back(r);
        f->pixels.push_back(a);
    }
    return f;
}

bool pixelNear(const std::vector<std::uint8_t>& img, int width, int x, int y, int b, int g, int r)
{
    const std::size_t i = (static_cast<std::size_t>(y) * width + x) * 4;
    const bool ok = std::abs(img[i] - b) <= 2 && std::abs(img[i + 1] - g) <= 2 && std::abs(img[i + 2] - r) <= 2;
    if (!ok)
        std::fprintf(stderr, "픽셀(%d,%d) = B%d G%d R%d, 기대값 B%d G%d R%d\n", x, y, img[i], img[i + 1], img[i + 2], b,
                     g, r);
    return ok;
}

LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return DefWindowProcW(h, m, w, l);
}

} // namespace

int main()
{
    const auto displays = enumerateDisplays();
    std::printf("디스플레이 %d개\n", static_cast<int>(displays.size()));
    for (const auto& d : displays)
        std::printf("  %s %dx%d @ %d,%d [%s]\n", d.id.c_str(), d.width, d.height, d.x, d.y, d.adapter.c_str());

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"LedSmokeTest";
    RegisterClassExW(&wc);

    const int width = 64;
    const int height = 32;
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"smoke", WS_POPUP, 0, 0, width, height, nullptr,
                                nullptr, wc.hInstance, nullptr);
    CHECK(hwnd != nullptr);
    if (!hwnd)
        return test::finish("test_d3d11_smoke");

    {
        D3D11Renderer renderer;
        std::string error;
        const bool ok = renderer.initialize(hwnd, width, height, error);
        if (!ok)
            std::fprintf(stderr, "초기화 실패: %s\n", error.c_str());
        CHECK(ok);
        if (ok) {
            std::printf("그래픽카드: %s\n", renderer.adapterName().c_str());
            renderer.setCaptureEnabled(true);

            std::vector<DrawItem> items(2);
            items[0].key = 1; // 왼쪽 절반: 빨강 100%
            items[0].dst = RectF{0, 0, 32, 32};
            items[0].uv = RectF{0, 0, 1, 1};
            items[0].frame = solidFrame(0, 0, 255, 255);
            items[1].key = 2; // 오른쪽 절반: 파랑 50%
            items[1].dst = RectF{32, 0, 32, 32};
            items[1].uv = RectF{0, 0, 1, 1};
            items[1].opacity = 0.5f;
            items[1].frame = solidFrame(255, 0, 0, 255);

            for (int i = 0; i < 3; ++i) {
                const RenderResult r = renderer.render(items, false);
                CHECK(r != RenderResult::DeviceLost);
            }
            const auto& img = renderer.lastCapture();
            CHECK(img.size() == static_cast<std::size_t>(width * height * 4));
            if (img.size() == static_cast<std::size_t>(width * height * 4)) {
                CHECK(pixelNear(img, width, 8, 8, 0, 0, 255));
                CHECK(pixelNear(img, width, 31, 31, 0, 0, 255)); // 경계 픽셀까지 정확히
                CHECK(pixelNear(img, width, 32, 0, 128, 0, 0));
                CHECK(pixelNear(img, width, 48, 16, 128, 0, 0));
            }

            // 프레임 교체 시 텍스처 갱신 확인
            items[0].frame = solidFrame(0, 255, 0, 255);
            renderer.render(items, false);
            const auto& img2 = renderer.lastCapture();
            if (img2.size() == static_cast<std::size_t>(width * height * 4))
                CHECK(pixelNear(img2, width, 8, 8, 0, 255, 0));

            // 아무것도 없으면 검은색
            renderer.render({}, false);
            const auto& img3 = renderer.lastCapture();
            if (img3.size() == static_cast<std::size_t>(width * height * 4))
                CHECK(pixelNear(img3, width, 8, 8, 0, 0, 0));
        }
        renderer.shutdown();
    }
    DestroyWindow(hwnd);
    return test::finish("test_d3d11_smoke");
}
