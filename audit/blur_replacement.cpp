// Replace examples/example_win32_directx9/blur.cpp with this translation unit.
// Add declarations for blur::on_device_reset() and blur::shutdown() to blur.hpp.
// All calls, callbacks, resets and shutdown must run on the render thread.
// Each frame's draw data must be consumed before queuing the next frame.
#include "examples/example_win32_directx9/blur.hpp"
#include "examples/example_win32_directx9/blur_x.hpp"
#include "examples/example_win32_directx9/blur_y.hpp"
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <deque>

namespace {
using Microsoft::WRL::ComPtr;
struct Request { float alpha; };
struct Vertex { float x, y, z, rhw; D3DCOLOR color; float u, v; };
constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
constexpr int kIterations = 8; // Preserve the existing kernel repetition count.
struct Resources {
    HRESULT last_result = S_FALSE;
    ComPtr<IDirect3DTexture9> a, b;
    ComPtr<IDirect3DSurface9> sa, sb;
    ComPtr<IDirect3DPixelShader9> x, y;
    UINT width = 0, height = 0;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    int frame = -1;
    std::deque<Request> requests;
    void Reset() {
        sa.Reset(); sb.Reset(); a.Reset(); b.Reset();
        x.Reset(); y.Reset(); width = height = 0;
        format = D3DFMT_UNKNOWN; frame = -1; requests.clear();
    }
} resources;

template<std::size_t N>
HRESULT CreateShader(IDirect3DDevice9* d, const std::array<char, N>& bytes,
                     ComPtr<IDirect3DPixelShader9>& shader) {
    static_assert(N % sizeof(DWORD) == 0);
    std::array<DWORD, N / sizeof(DWORD)> words{};
    std::memcpy(words.data(), bytes.data(), N);
    return d->CreatePixelShader(words.data(), shader.ReleaseAndGetAddressOf());
}

HRESULT Prepare(IDirect3DDevice9* d) {
    ComPtr<IDirect3DSurface9> back;
    HRESULT hr = d->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, back.GetAddressOf());
    if (FAILED(hr)) return hr;
    D3DSURFACE_DESC desc{};
    if (FAILED(hr = back->GetDesc(&desc))) return hr;
    if (resources.a && resources.b && resources.x && resources.y &&
        resources.width == desc.Width && resources.height == desc.Height &&
        resources.format == desc.Format) return S_OK;

    // Prepare runs before any callback for this frame is queued.
    ComPtr<IDirect3DTexture9> a, b;
    ComPtr<IDirect3DSurface9> sa, sb;
    ComPtr<IDirect3DPixelShader9> x, y;
    if (FAILED(hr = d->CreateTexture(desc.Width, desc.Height, 1,
        D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, a.GetAddressOf(), nullptr))) return hr;
    if (FAILED(hr = d->CreateTexture(desc.Width, desc.Height, 1,
        D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, b.GetAddressOf(), nullptr))) return hr;
    if (FAILED(hr = a->GetSurfaceLevel(0, sa.GetAddressOf()))) return hr;
    if (FAILED(hr = b->GetSurfaceLevel(0, sb.GetAddressOf()))) return hr;
    if (FAILED(hr = CreateShader(d, blur_x, x))) return hr;
    if (FAILED(hr = CreateShader(d, blur_y, y))) return hr;
    resources.a = std::move(a); resources.b = std::move(b);
    resources.sa = std::move(sa); resources.sb = std::move(sb);
    resources.x = std::move(x); resources.y = std::move(y);
    resources.width = desc.Width; resources.height = desc.Height;
    resources.format = desc.Format;
    return S_OK;
}

// The existing application has one viewport at (0,0), no MRT, and a 1:1 framebuffer.
// State blocks restore stream/index bindings changed by DrawPrimitiveUP as well.
void Render(const ImDrawList*, const ImDrawCmd* cmd) {
    resources.last_result = S_FALSE;
    auto* d = blur::device;
    if (!d || !resources.a || !resources.b) return;
    ComPtr<IDirect3DStateBlock9> state;
    ComPtr<IDirect3DSurface9> target, depth, back;
    D3DVIEWPORT9 viewport{};
    if (FAILED(d->CreateStateBlock(D3DSBT_ALL, state.GetAddressOf())) ||
        FAILED(state->Capture()) ||
        FAILED(d->GetRenderTarget(0, target.GetAddressOf())) ||
        FAILED(d->GetViewport(&viewport)) ||
        FAILED(d->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, back.GetAddressOf()))) return;
    const HRESULT depth_hr = d->GetDepthStencilSurface(depth.GetAddressOf());
    if (FAILED(depth_hr) && depth_hr != D3DERR_NOTFOUND) return;

    const float w = static_cast<float>(resources.width);
    const float h = static_cast<float>(resources.height);
    const Vertex quad[] = {
        {-0.5f, -0.5f, 0, 1, 0xffffffff, 0, 0},
        {w-0.5f, -0.5f, 0, 1, 0xffffffff, 1, 0},
        {-0.5f, h-0.5f, 0, 1, 0xffffffff, 0, 1},
        {w-0.5f, h-0.5f, 0, 1, 0xffffffff, 1, 1}
    };
    const D3DVIEWPORT9 full{0, 0, resources.width, resources.height, 0, 1};
    const auto run = [&]() -> HRESULT {
        HRESULT hr;
#define CHECK_D3D(expression) do { hr = (expression); if (FAILED(hr)) return hr; } while (false)
        CHECK_D3D(d->SetTexture(0, nullptr));
        CHECK_D3D(d->StretchRect(back.Get(), nullptr, resources.sa.Get(), nullptr, D3DTEXF_NONE));
        CHECK_D3D(d->SetDepthStencilSurface(nullptr));
        CHECK_D3D(d->SetVertexShader(nullptr));
        CHECK_D3D(d->SetFVF(kFvf));
        CHECK_D3D(d->SetRenderState(D3DRS_ZENABLE, FALSE));
        CHECK_D3D(d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE));
        CHECK_D3D(d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE));
        CHECK_D3D(d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
        CHECK_D3D(d->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
        CHECK_D3D(d->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
        CHECK_D3D(d->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
        CHECK_D3D(d->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
        CHECK_D3D(d->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
        const auto pass = [&](IDirect3DTexture9* source, IDirect3DSurface9* dest,
                              IDirect3DPixelShader9* shader, float step) -> HRESULT {
            CHECK_D3D(d->SetTexture(0, nullptr));
            CHECK_D3D(d->SetRenderTarget(0, dest));
            CHECK_D3D(d->SetViewport(&full));
            CHECK_D3D(d->SetPixelShader(shader));
            const float constants[4]{step, 0, 0, 0};
            CHECK_D3D(d->SetPixelShaderConstantF(0, constants, 1));
            CHECK_D3D(d->SetTexture(0, source));
            CHECK_D3D(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex)));
            return S_OK;
        };
        for (int i = 0; i < kIterations; ++i) {
            CHECK_D3D(pass(resources.a.Get(), resources.sb.Get(), resources.x.Get(), 2.5f / w));
            CHECK_D3D(pass(resources.b.Get(), resources.sa.Get(), resources.y.Get(), 2.5f / h));
        }
        CHECK_D3D(d->SetTexture(0, nullptr));
        CHECK_D3D(d->SetRenderTarget(0, target.Get()));
        CHECK_D3D(d->SetViewport(&viewport));
        CHECK_D3D(d->SetPixelShader(nullptr));
        CHECK_D3D(d->SetTexture(0, resources.a.Get()));
        CHECK_D3D(d->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE));
        RECT clip{
            static_cast<LONG>(std::clamp(cmd->ClipRect.x, 0.0f, w)),
            static_cast<LONG>(std::clamp(cmd->ClipRect.y, 0.0f, h)),
            static_cast<LONG>(std::clamp(cmd->ClipRect.z, 0.0f, w)),
            static_cast<LONG>(std::clamp(cmd->ClipRect.w, 0.0f, h))};
        if (clip.right <= clip.left || clip.bottom <= clip.top) return S_OK;
        CHECK_D3D(d->SetScissorRect(&clip));
        CHECK_D3D(d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
        CHECK_D3D(d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
        CHECK_D3D(d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
        // Use vertex alpha explicitly: backbuffer alpha may be undefined or zero.
        CHECK_D3D(d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1));
        CHECK_D3D(d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE));
        const auto alpha = static_cast<DWORD>(255 * static_cast<const Request*>(cmd->UserCallbackData)->alpha);
        const D3DCOLOR color = (alpha << 24) | 0x00ffffff;
        // Preserve the original full-screen seven-pixel rounded compositing quad.
        std::array<Vertex, 38> fan{};
        fan[0] = {w/2-0.5f, h/2-0.5f, 0, 1, color, 0.5f, 0.5f};
        const float radius = (std::min)(7.0f, (std::min)(w, h) * 0.5f);
        const float cx[4]{w-radius, radius, radius, w-radius};
        const float cy[4]{h-radius, h-radius, radius, radius};
        unsigned count = 1;
        for (unsigned corner = 0; corner < 4; ++corner)
            for (unsigned j = 0; j <= 8; ++j) {
                const float angle = (corner + j/8.0f) * 1.57079632679f;
                const float px = cx[corner] + std::cos(angle) * radius;
                const float py = cy[corner] + std::sin(angle) * radius;
                fan[count++] = {px-0.5f, py-0.5f, 0, 1, color, px/w, py/h};
            }
        fan[count++] = fan[1];
        CHECK_D3D(d->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, count-2, fan.data(), sizeof(Vertex)));
        return S_OK;
#undef CHECK_D3D
    };
    const HRESULT hr = run();
    d->SetTexture(0, nullptr);
    const HRESULT rt_hr = d->SetRenderTarget(0, target.Get());
    const HRESULT ds_hr = d->SetDepthStencilSurface(depth.Get());
    const HRESULT state_hr = state->Apply();
    const HRESULT vp_hr = d->SetViewport(&viewport);
    resources.last_result = FAILED(hr) ? hr : FAILED(rt_hr) ? rt_hr : FAILED(ds_hr) ? ds_hr : FAILED(state_hr) ? state_hr : vp_hr;
    if (FAILED(hr) || FAILED(rt_hr) || FAILED(ds_hr) || FAILED(state_hr) || FAILED(vp_hr))
        OutputDebugStringA("Blur pass or state restoration failed.\n");
}
} // namespace

namespace blur {
HRESULT last_result() { return resources.last_result; }
void on_device_reset() { resources.Reset(); }
void shutdown() { resources.Reset(); device = nullptr; }
}

void draw_blur(ImDrawList* draw, float alpha) {
    if (!draw || !blur::device || !std::isfinite(alpha) || alpha <= 0) return;
    const int frame = ImGui::GetFrameCount();
    if (resources.frame != frame) {
        resources.requests.clear();
        resources.frame = frame;
    }
    if (FAILED(Prepare(blur::device))) return;
    resources.requests.push_back({std::clamp(alpha, 0.0f, 1.0f)});
    draw->AddCallback(Render, &resources.requests.back());
}
