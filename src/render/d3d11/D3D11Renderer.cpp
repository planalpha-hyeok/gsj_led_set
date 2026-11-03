#include "render/d3d11/D3D11Renderer.h"

#include "core/Log.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace led {

namespace {

// 정점 버퍼 없이 SV_VertexID로 사각형 1개(삼각형 스트립 4정점)를 생성
const char kShaderSource[] = R"(
cbuffer Params : register(b0)
{
    float4 dstRect;  // NDC x0, y0, x1, y1
    float4 uvRect;   // u0, v0, u1, v1
    float4 misc;     // x: 불투명도, y: 1=premultiplied
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOut vs_main(uint id : SV_VertexID)
{
    float2 t = float2(float(id & 1u), float(id >> 1u));
    VSOut o;
    o.pos = float4(lerp(dstRect.x, dstRect.z, t.x), lerp(dstRect.y, dstRect.w, t.y), 0.0, 1.0);
    o.uv  = float2(lerp(uvRect.x, uvRect.z, t.x), lerp(uvRect.y, uvRect.w, t.y));
    return o;
}

Texture2D tex0 : register(t0);
SamplerState smp0 : register(s0);

float4 ps_main(VSOut i) : SV_Target
{
    float4 c = tex0.Sample(smp0, i.uv);
    if (misc.y < 0.5)
        c.rgb *= c.a;
    return c * misc.x;
}
)";

struct ShaderConstants {
    float dstRect[4];
    float uvRect[4];
    float misc[4];
};
static_assert(sizeof(ShaderConstants) % 16 == 0, "상수 버퍼 크기는 16의 배수여야 함");

constexpr std::uint64_t kTextureEvictFrames = 120;

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

std::string hrText(HRESULT hr)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

} // namespace

D3D11Renderer::~D3D11Renderer()
{
    shutdown();
}

bool D3D11Renderer::initialize(HWND window, int width, int height, std::string& error)
{
    shutdown();
    m_width = width;
    m_height = height;
    if (!createDevice(window, error))
        return false;
    if (!createSwapChain(window, error))
        return false;
    if (!createPipeline(error))
        return false;
    return true;
}

void D3D11Renderer::shutdown()
{
    if (m_context) {
        m_context->ClearState();
        m_context->Flush();
    }
    m_textures.clear();
    m_reportedOversize.clear();
    m_rasterizer.Reset();
    m_blend.Reset();
    m_sampler.Reset();
    m_constants.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_renderTarget.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

bool D3D11Renderer::createDevice(HWND window, std::string& error)
{
    // 창이 위치한 모니터에 연결된 그래픽카드 찾기
    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    ComPtr<IDXGIFactory1> factory;
    ComPtr<IDXGIAdapter1> chosen;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf())))) {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT a = 0; !chosen && factory->EnumAdapters1(a, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND;
             ++a) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;
            ComPtr<IDXGIOutput> output;
            for (UINT o = 0; adapter->EnumOutputs(o, output.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++o) {
                DXGI_OUTPUT_DESC outDesc{};
                if (SUCCEEDED(output->GetDesc(&outDesc)) && outDesc.Monitor == monitor) {
                    chosen = adapter;
                    m_adapterName = toUtf8(desc.Description);
                    break;
                }
            }
        }
    }

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                        D3D_FEATURE_LEVEL_10_0};
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_10_0;

    auto tryCreate = [&](IDXGIAdapter* adapter, D3D_DRIVER_TYPE type) -> HRESULT {
        HRESULT hr = D3D11CreateDevice(adapter, type, nullptr, flags, levels, UINT(std::size(levels)),
                                       D3D11_SDK_VERSION, m_device.ReleaseAndGetAddressOf(), &obtained,
                                       m_context.ReleaseAndGetAddressOf());
        if (hr == E_INVALIDARG) // D3D 11.1 미지원 런타임: 11.1 제외하고 재시도
            hr = D3D11CreateDevice(adapter, type, nullptr, flags, levels + 1, UINT(std::size(levels) - 1),
                                   D3D11_SDK_VERSION, m_device.ReleaseAndGetAddressOf(), &obtained,
                                   m_context.ReleaseAndGetAddressOf());
        return hr;
    };

    HRESULT hr = chosen ? tryCreate(chosen.Get(), D3D_DRIVER_TYPE_UNKNOWN) : E_FAIL;
    if (FAILED(hr)) {
        if (chosen)
            logWarning("지정 그래픽카드에서 장치 생성 실패 (" + hrText(hr) + "), 기본 그래픽카드로 재시도");
        m_adapterName = "기본 그래픽카드";
        hr = tryCreate(nullptr, D3D_DRIVER_TYPE_HARDWARE);
    }
    if (FAILED(hr)) {
        logWarning("하드웨어 가속 장치 생성 실패 (" + hrText(hr) + "), 소프트웨어(WARP)로 재시도");
        m_adapterName = "WARP (소프트웨어)";
        hr = tryCreate(nullptr, D3D_DRIVER_TYPE_WARP);
    }
    if (FAILED(hr)) {
        error = "Direct3D 11 장치를 만들 수 없음 (" + hrText(hr) + ")";
        return false;
    }

    m_maxTextureSize = obtained >= D3D_FEATURE_LEVEL_11_0 ? D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION : 8192;
    return true;
}

bool D3D11Renderer::createSwapChain(HWND window, std::string& error)
{
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    HRESULT hr = m_device.As(&dxgiDevice);
    if (SUCCEEDED(hr))
        hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (SUCCEEDED(hr))
        hr = adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        error = "DXGI 팩토리를 얻을 수 없음 (" + hrText(hr) + ")";
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(m_width);
    desc.Height = static_cast<UINT>(m_height);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    // Windows 10 이상: FLIP_DISCARD, 그 외에는 차례로 대체
    struct Mode {
        DXGI_SWAP_EFFECT effect;
        UINT buffers;
    };
    const Mode modes[] = {{DXGI_SWAP_EFFECT_FLIP_DISCARD, 2},
                          {DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2},
                          {DXGI_SWAP_EFFECT_DISCARD, 1}};
    for (const Mode& mode : modes) {
        desc.SwapEffect = mode.effect;
        desc.BufferCount = mode.buffers;
        hr = factory->CreateSwapChainForHwnd(m_device.Get(), window, &desc, nullptr, nullptr,
                                             m_swapChain.ReleaseAndGetAddressOf());
        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr)) {
        error = "스왑체인을 만들 수 없음 (" + hrText(hr) + ")";
        return false;
    }
    factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (SUCCEEDED(hr))
        hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTarget.GetAddressOf());
    if (FAILED(hr)) {
        error = "렌더 타깃을 만들 수 없음 (" + hrText(hr) + ")";
        return false;
    }
    return true;
}

bool D3D11Renderer::createPipeline(std::string& error)
{
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errors;
    const UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;

    HRESULT hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "led_shader", nullptr, nullptr, "vs_main",
                            "vs_4_0", compileFlags, 0, vsBlob.GetAddressOf(), errors.ReleaseAndGetAddressOf());
    if (SUCCEEDED(hr))
        hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "led_shader", nullptr, nullptr, "ps_main",
                        "ps_4_0", compileFlags, 0, psBlob.GetAddressOf(), errors.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        error = "셰이더 컴파일 실패 (" + hrText(hr) + ")";
        if (errors)
            error += ": " + std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        return false;
    }

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr,
                                      m_vertexShader.GetAddressOf());
    if (SUCCEEDED(hr))
        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
                                         m_pixelShader.GetAddressOf());

    if (SUCCEEDED(hr)) {
        D3D11_BUFFER_DESC cb{};
        cb.ByteWidth = sizeof(ShaderConstants);
        cb.Usage = D3D11_USAGE_DYNAMIC;
        cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = m_device->CreateBuffer(&cb, nullptr, m_constants.GetAddressOf());
    }

    if (SUCCEEDED(hr)) {
        D3D11_SAMPLER_DESC s{};
        s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        s.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        s.ComparisonFunc = D3D11_COMPARISON_NEVER;
        s.MaxLOD = D3D11_FLOAT32_MAX;
        hr = m_device->CreateSamplerState(&s, m_sampler.GetAddressOf());
    }

    if (SUCCEEDED(hr)) {
        // premultiplied alpha 합성
        D3D11_BLEND_DESC b{};
        b.RenderTarget[0].BlendEnable = TRUE;
        b.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        b.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        b.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        b.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = m_device->CreateBlendState(&b, m_blend.GetAddressOf());
    }

    if (SUCCEEDED(hr)) {
        D3D11_RASTERIZER_DESC r{};
        r.FillMode = D3D11_FILL_SOLID;
        r.CullMode = D3D11_CULL_NONE;
        r.DepthClipEnable = TRUE;
        hr = m_device->CreateRasterizerState(&r, m_rasterizer.GetAddressOf());
    }

    if (FAILED(hr)) {
        error = "그래픽 파이프라인 생성 실패 (" + hrText(hr) + ")";
        return false;
    }
    return true;
}

D3D11Renderer::TextureEntry* D3D11Renderer::prepareTexture(const DrawItem& item)
{
    const Frame& frame = *item.frame;
    if (frame.width <= 0 || frame.height <= 0 || frame.pixels.empty())
        return nullptr;

    if (frame.width > m_maxTextureSize || frame.height > m_maxTextureSize) {
        if (m_reportedOversize.insert(item.key).second)
            logError("콘텐츠 해상도가 그래픽카드 한계(" + std::to_string(m_maxTextureSize) + "px)를 넘음: " +
                     std::to_string(frame.width) + "x" + std::to_string(frame.height));
        return nullptr;
    }

    TextureEntry& entry = m_textures[item.key];
    entry.lastUsedFrame = m_frameCounter;

    if (!entry.texture || entry.width != frame.width || entry.height != frame.height) {
        entry = TextureEntry{};
        entry.lastUsedFrame = m_frameCounter;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(frame.width);
        desc.Height = static_cast<UINT>(frame.height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, entry.texture.GetAddressOf());
        if (SUCCEEDED(hr))
            hr = m_device->CreateShaderResourceView(entry.texture.Get(), nullptr, entry.view.GetAddressOf());
        if (FAILED(hr)) {
            logError("텍스처 생성 실패 (" + hrText(hr) + ")");
            m_textures.erase(item.key);
            return nullptr;
        }
        entry.width = frame.width;
        entry.height = frame.height;
    }

    if (entry.serial != frame.serial) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT hr = m_context->Map(entry.texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr))
            return &entry; // 이전 내용으로 그림
        const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * 4;
        const auto* src = frame.pixels.data();
        auto* dst = static_cast<std::uint8_t*>(mapped.pData);
        if (mapped.RowPitch == rowBytes && static_cast<std::size_t>(frame.stride) == rowBytes) {
            std::memcpy(dst, src, rowBytes * static_cast<std::size_t>(frame.height));
        } else {
            for (int y = 0; y < frame.height; ++y)
                std::memcpy(dst + static_cast<std::size_t>(y) * mapped.RowPitch,
                            src + static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.stride), rowBytes);
        }
        m_context->Unmap(entry.texture.Get(), 0);
        entry.serial = frame.serial;
    }
    return &entry;
}

void D3D11Renderer::evictUnusedTextures()
{
    for (auto it = m_textures.begin(); it != m_textures.end();) {
        if (m_frameCounter - it->second.lastUsedFrame > kTextureEvictFrames)
            it = m_textures.erase(it);
        else
            ++it;
    }
}

void D3D11Renderer::captureBackBuffer()
{
    m_capture.clear();
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                      reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
        return;
    D3D11_TEXTURE2D_DESC desc{};
    backBuffer->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())))
        return;
    m_context->CopyResource(staging.Get(), backBuffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        return;
    const std::size_t rowBytes = static_cast<std::size_t>(m_width) * 4;
    m_capture.resize(rowBytes * static_cast<std::size_t>(m_height));
    for (int y = 0; y < m_height; ++y)
        std::memcpy(m_capture.data() + static_cast<std::size_t>(y) * rowBytes,
                    static_cast<const std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch,
                    rowBytes);
    m_context->Unmap(staging.Get(), 0);
}

RenderResult D3D11Renderer::render(const std::vector<DrawItem>& items, bool vsync)
{
    if (!m_swapChain)
        return RenderResult::DeviceLost;
    ++m_frameCounter;

    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    ID3D11RenderTargetView* rtv = m_renderTarget.Get();
    m_context->OMSetRenderTargets(1, &rtv, nullptr);
    m_context->ClearRenderTargetView(rtv, black);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);
    m_context->RSSetState(m_rasterizer.Get());

    const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_context->OMSetBlendState(m_blend.Get(), blendFactor, 0xFFFFFFFF);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    ID3D11Buffer* cb = m_constants.Get();
    m_context->VSSetConstantBuffers(0, 1, &cb);
    m_context->PSSetConstantBuffers(0, 1, &cb);
    ID3D11SamplerState* sampler = m_sampler.Get();
    m_context->PSSetSamplers(0, 1, &sampler);

    const double w = m_width;
    const double h = m_height;
    for (const DrawItem& item : items) {
        if (!item.frame)
            continue;
        TextureEntry* tex = prepareTexture(item);
        if (!tex)
            continue;

        ShaderConstants c{};
        c.dstRect[0] = static_cast<float>(item.dst.x / w * 2.0 - 1.0);
        c.dstRect[1] = static_cast<float>(1.0 - item.dst.y / h * 2.0);
        c.dstRect[2] = static_cast<float>((item.dst.x + item.dst.w) / w * 2.0 - 1.0);
        c.dstRect[3] = static_cast<float>(1.0 - (item.dst.y + item.dst.h) / h * 2.0);
        c.uvRect[0] = static_cast<float>(item.uv.x);
        c.uvRect[1] = static_cast<float>(item.uv.y);
        c.uvRect[2] = static_cast<float>(item.uv.x + item.uv.w);
        c.uvRect[3] = static_cast<float>(item.uv.y + item.uv.h);
        c.misc[0] = std::clamp(item.opacity, 0.0f, 1.0f);
        c.misc[1] = item.frame->premultiplied ? 1.0f : 0.0f;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(m_context->Map(m_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            continue;
        std::memcpy(mapped.pData, &c, sizeof(c));
        m_context->Unmap(m_constants.Get(), 0);

        ID3D11ShaderResourceView* view = tex->view.Get();
        m_context->PSSetShaderResources(0, 1, &view);
        m_context->Draw(4, 0);
    }

    ID3D11ShaderResourceView* nullView = nullptr;
    m_context->PSSetShaderResources(0, 1, &nullView);

    if ((m_frameCounter % 60) == 0)
        evictUnusedTextures();
    if (m_captureEnabled)
        captureBackBuffer();

    const HRESULT hr = m_swapChain->Present(vsync ? 1 : 0, 0);
    if (hr == DXGI_STATUS_OCCLUDED)
        return RenderResult::Occluded;
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        const HRESULT reason = m_device->GetDeviceRemovedReason();
        logError("그래픽 장치 손실 (" + hrText(hr) + ", 사유 " + hrText(reason) + ")");
        return RenderResult::DeviceLost;
    }
    if (FAILED(hr)) {
        logError("화면 출력(Present) 실패 (" + hrText(hr) + ")");
        return RenderResult::DeviceLost;
    }
    return RenderResult::Ok;
}

} // namespace led
