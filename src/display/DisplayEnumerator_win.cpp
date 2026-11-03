// Windows 전용: Win32 + DXGI로 디스플레이와 그래픽카드 조회
// 프로세스가 Per-Monitor DPI Aware 상태여야 물리 픽셀 좌표가 반환됨 (Qt 6 기본값)

#include "display/DisplayInfo.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>

namespace led {

namespace {

std::string toUtf8(const wchar_t* text)
{
    if (!text || !*text)
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1)
        return {};
    std::string out(static_cast<std::size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &out[0], len, nullptr, nullptr);
    return out;
}

BOOL CALLBACK monitorEnumProc(HMONITOR monitor, HDC, LPRECT, LPARAM param)
{
    auto* list = reinterpret_cast<std::vector<DisplayInfo>*>(param);

    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
        return TRUE;

    DisplayInfo d;
    d.id = toUtf8(info.szDevice);
    d.x = info.rcMonitor.left;
    d.y = info.rcMonitor.top;
    d.width = info.rcMonitor.right - info.rcMonitor.left;
    d.height = info.rcMonitor.bottom - info.rcMonitor.top;
    d.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    if (EnumDisplayDevicesW(info.szDevice, 0, &device, 0))
        d.name = toUtf8(device.DeviceString);

    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode))
        d.refreshHz = static_cast<int>(mode.dmDisplayFrequency);

    list->push_back(std::move(d));
    return TRUE;
}

void fillAdapterNames(std::vector<DisplayInfo>& list)
{
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()))))
        return;

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT a = 0; factory->EnumAdapters1(a, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++a) {
        DXGI_ADAPTER_DESC1 adapterDesc{};
        adapter->GetDesc1(&adapterDesc);
        const std::string adapterName = toUtf8(adapterDesc.Description);

        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        for (UINT o = 0; adapter->EnumOutputs(o, output.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++o) {
            DXGI_OUTPUT_DESC outDesc{};
            if (FAILED(output->GetDesc(&outDesc)))
                continue;
            const std::string deviceName = toUtf8(outDesc.DeviceName);
            for (auto& d : list)
                if (d.id == deviceName)
                    d.adapter = adapterName;
        }
    }
}

} // namespace

std::vector<DisplayInfo> enumerateDisplays()
{
    std::vector<DisplayInfo> list;
    EnumDisplayMonitors(nullptr, nullptr, monitorEnumProc, reinterpret_cast<LPARAM>(&list));
    fillAdapterNames(list);
    std::sort(list.begin(), list.end(), [](const DisplayInfo& a, const DisplayInfo& b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    return list;
}

} // namespace led
