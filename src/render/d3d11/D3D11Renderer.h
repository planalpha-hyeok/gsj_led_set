#pragma once

// Direct3D 11 렌더러 (Windows 전용)
// 출력 창 1개당 1개. 창이 위치한 모니터의 그래픽카드에서 장치를 생성함 (멀티 GPU 대응)

#include "render/Scene.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace led {

enum class RenderResult {
    Ok,
    Occluded,    // 창이 가려짐/최소화됨 (잠시 대기 필요)
    DeviceLost   // 그래픽 장치 재생성 필요
};

class D3D11Renderer {
public:
    D3D11Renderer() = default;
    ~D3D11Renderer();

    D3D11Renderer(const D3D11Renderer&) = delete;
    D3D11Renderer& operator=(const D3D11Renderer&) = delete;

    bool initialize(HWND window, int width, int height, std::string& error);
    void shutdown();

    RenderResult render(const std::vector<DrawItem>& items, bool vsync);

    const std::string& adapterName() const { return m_adapterName; }

    // 검사용: 켜면 매 프레임 화면 내용을 BGRA로 복사해 둠 (성능 저하, 평소에는 끔)
    void setCaptureEnabled(bool enabled) { m_captureEnabled = enabled; }
    const std::vector<std::uint8_t>& lastCapture() const { return m_capture; }

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct TextureEntry {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> view;
        int width = 0;
        int height = 0;
        std::uint64_t serial = 0;
        std::uint64_t lastUsedFrame = 0;
    };

    bool createDevice(HWND window, std::string& error);
    bool createSwapChain(HWND window, std::string& error);
    bool createPipeline(std::string& error);
    TextureEntry* prepareTexture(const DrawItem& item);
    void evictUnusedTextures();
    void captureBackBuffer();

    int m_width = 0;
    int m_height = 0;
    int m_maxTextureSize = 8192;
    std::uint64_t m_frameCounter = 0;
    std::string m_adapterName;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTarget;
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11Buffer> m_constants;
    ComPtr<ID3D11SamplerState> m_sampler;
    ComPtr<ID3D11BlendState> m_blend;
    ComPtr<ID3D11RasterizerState> m_rasterizer;

    std::unordered_map<std::uint64_t, TextureEntry> m_textures;
    std::unordered_set<std::uint64_t> m_reportedOversize;

    bool m_captureEnabled = false;
    std::vector<std::uint8_t> m_capture;
};

} // namespace led
