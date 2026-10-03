#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "examples/example_win32_directx9/blur.hpp"
#include <wrl/client.h>
#include <cassert>
#include <cstdio>
#include "font_replacement.hpp"
namespace blur { void on_device_reset(); void shutdown(); HRESULT last_result(); }
int main() {
    using Microsoft::WRL::ComPtr;
    WNDCLASSW wc{};
    wc.lpfnWndProc=DefWindowProcW; wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=L"AuditHiddenD3D9";
    if (!RegisterClassW(&wc)) return 2;
    HWND window=CreateWindowW(wc.lpszClassName,L"audit",WS_OVERLAPPEDWINDOW,
                             0,0,256,256,nullptr,nullptr,wc.hInstance,nullptr);
    if (!window) return 3;
    ComPtr<IDirect3D9> api;
    api.Attach(Direct3DCreate9(D3D_SDK_VERSION));
    if (!api) return 4;
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=pp.BackBufferHeight=128;
    pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9> d;
    HRESULT hr=api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,d.GetAddressOf());
    if(FAILED(hr)) { std::printf("create_device=%08lx\n",hr); return 5; }
    ImGui::CreateContext();
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DeltaTime=1.0f/60;
    assert(ImGui_ImplDX9_Init(d.Get()));
    blur::device=d.Get();
    auditfix::Fonts fonts;
    assert(auditfix::RebuildFonts(1.0f,L"audit/nonexistent-font-directory",fonts));
    assert(io.Fonts->Fonts.Size==7);
    std::printf("missing_fonts_fallback_slots=%d\n",io.Fonts->Fonts.Size);
    for(int size : {128,256}) {
        if(size==256) {
            blur::on_device_reset();
            ImGui_ImplDX9_InvalidateDeviceObjects();
            pp.BackBufferWidth=pp.BackBufferHeight=256;
            hr=d->Reset(&pp);
            std::printf("reset=%08lx\n",hr); assert(SUCCEEDED(hr));
        }
        io.DisplaySize=ImVec2(static_cast<float>(size),static_cast<float>(size));
        ImGui_ImplDX9_NewFrame(); ImGui::NewFrame();
        auto* draw=ImGui::GetBackgroundDrawList();
        draw->AddRectFilled(ImVec2(0,0),io.DisplaySize,IM_COL32(30,50,80,255));
        draw_blur(draw,0.8f); draw_blur(draw,0.5f);
        draw->AddRectFilled(ImVec2(5,5),ImVec2(20,20),IM_COL32_WHITE);
        ImGui::Render();
        assert(SUCCEEDED(d->Clear(0,nullptr,D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1,0)));
        assert(SUCCEEDED(d->BeginScene()));
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        assert(SUCCEEDED(d->EndScene()));
        std::printf("blur_%d=%08lx\n",size,blur::last_result());
        assert(blur::last_result()==S_OK);
    }
    blur::shutdown(); ImGui_ImplDX9_Shutdown(); ImGui::DestroyContext();
    d.Reset(); api.Reset(); DestroyWindow(window); UnregisterClassW(wc.lpszClassName,wc.hInstance);
}
