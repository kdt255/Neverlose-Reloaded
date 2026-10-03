#pragma once

#include "imgui.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;

namespace shader 
{
    using namespace DirectX;
    
    inline const char* BENDS_HLSL_SRC = R"(
    #define MAX_COLORS 8
    
    cbuffer BendsProps : register(b0) {
        float2 uCanvas;
        float uTime;
        float uSpeed;
        
        float2 uRot;
        int uColorCount;
        int uTransparent;
        
        float uScale;
        float uFrequency;
        float uWarpStrength;
        float uMouseInfluence;
        
        float2 uPointer;
        float uParallax;
        float uNoise;
        
        int uIterations;
        float uIntensity;
        float uBandWidth;
        float padding1;
        
        float4 uColors[MAX_COLORS];
    };
    
    struct VS_OUTPUT {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;
    };
    
    VS_OUTPUT vs_main(uint id : SV_VertexID) {
        VS_OUTPUT output;
        output.uv = float2((id << 1) & 2, id & 2);
        output.pos = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
        return output;
    }
    
    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float2 vUv = float2(input.uv.x, 1.0f - input.uv.y);
        float t = uTime * uSpeed;
        float2 p = vUv * 2.0f - 1.0f;
        p += uPointer * uParallax * 0.1f;
        
        float2 rp = float2(p.x * uRot.x - p.y * uRot.y, p.x * uRot.y + p.y * uRot.x);
        float2 q = float2(rp.x * (uCanvas.x / uCanvas.y), rp.y);
        
        q /= max(uScale, 0.0001f);
        q /= 0.5f + 0.2f * dot(q, q);
        q += 0.2f * cos(t) - 7.56f;
        
        float2 toward = (uPointer - rp);
        q += toward * uMouseInfluence * 0.2f;
    
        for (int j = 0; j < 5; j++) {
            if (j >= uIterations - 1) break;
            float2 rr = sin(1.5f * (q.yx * uFrequency) + 2.0f * cos(q * uFrequency));
            q += (rr - q) * 0.15f;
        }
    
        float3 col = float3(0.0f, 0.0f, 0.0f);
        float a = 1.0f;
    
        if (uColorCount > 0) {
            float2 s = q;
            float3 sumCol = float3(0.0f, 0.0f, 0.0f);
            float cover = 0.0f;
            
            for (int i = 0; i < MAX_COLORS; ++i) {
                if (i >= uColorCount) break;
                s -= 0.01f;
                float2 r = sin(1.5f * (s.yx * uFrequency) + 2.0f * cos(s * uFrequency));
                float m0 = length(r + sin(5.0f * r.y * uFrequency - 3.0f * t + (float)i) / 4.0f);
                float kBelow = clamp(uWarpStrength, 0.0f, 1.0f);
                float kMix = pow(kBelow, 0.3f);
                float gain = 1.0f + max(uWarpStrength - 1.0f, 0.0f);
                float2 disp = (r - s) * kBelow;
                float2 warped = s + disp * gain;
                float m1 = length(warped + sin(5.0f * warped.y * uFrequency - 3.0f * t + (float)i) / 4.0f);
                float m = lerp(m0, m1, kMix);
                float w = 1.0f - exp(-uBandWidth / exp(uBandWidth * m));
                sumCol += uColors[i].xyz * w;
                cover = max(cover, w);
            }
            col = clamp(sumCol, 0.0f, 1.0f);
            a = uTransparent > 0 ? cover : 1.0f;
        } else {
            float2 s = q;
            for (int k = 0; k < 3; ++k) {
                s -= 0.01f;
                float2 r = sin(1.5f * (s.yx * uFrequency) + 2.0f * cos(s * uFrequency));
                float m0 = length(r + sin(5.0f * r.y * uFrequency - 3.0f * t + (float)k) / 4.0f);
                float kBelow = clamp(uWarpStrength, 0.0f, 1.0f);
                float kMix = pow(kBelow, 0.3f);
                float gain = 1.0f + max(uWarpStrength - 1.0f, 0.0f);
                float2 disp = (r - s) * kBelow;
                float2 warped = s + disp * gain;
                float m1 = length(warped + sin(5.0f * warped.y * uFrequency - 3.0f * t + (float)k) / 4.0f);
                float m = lerp(m0, m1, kMix);
                if (k == 0) col.r = 1.0f - exp(-uBandWidth / exp(uBandWidth * m));
                else if (k == 1) col.g = 1.0f - exp(-uBandWidth / exp(uBandWidth * m));
                else col.b = 1.0f - exp(-uBandWidth / exp(uBandWidth * m));
            }
            a = uTransparent > 0 ? max(max(col.r, col.g), col.b) : 1.0f;
        }
    
        col *= uIntensity;
    
        if (uNoise > 0.0001f) {
            float n = frac(sin(dot(input.pos.xy + float2(uTime, uTime), float2(12.9898f, 78.233f))) * 43758.5453123f);
            col += (n - 0.5f) * uNoise;
            col = clamp(col, 0.0f, 1.0f);
        }
    
        float3 rgb = (uTransparent > 0) ? col * a : col;
        return float4(rgb, a);
    }
    )";
    
    __declspec(align(16)) struct BendsProps {
        XMFLOAT2 uCanvas;
        float uTime;
        float uSpeed;
        
        XMFLOAT2 uRot;
        int uColorCount;
        int uTransparent;
        
        float uScale;
        float uFrequency;
        float uWarpStrength;
        float uMouseInfluence;
        
        XMFLOAT2 uPointer;
        float uParallax;
        float uNoise;
        
        int uIterations;
        float uIntensity;
        float uBandWidth;
        float padding1;
        
        XMFLOAT4 uColors[8];
    };
    
    class ColorBends {
    public:
        struct FBO {
            ID3D11Texture2D* tex = nullptr;
            ID3D11ShaderResourceView* srv = nullptr;
            ID3D11RenderTargetView* rtv = nullptr;
            void Release() {
                if (srv) { srv->Release(); srv = nullptr; }
                if (rtv) { rtv->Release(); rtv = nullptr; }
                if (tex) { tex->Release(); tex = nullptr; }
            }
        };
    
        FBO outputFBO;
        int width = 0;
        int height = 0;
        float timeAcc = 0;
        
        BendsProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;
        
        ImVec2 smoothMouse = {0.0f, 0.0f};
    
        void Init() {
            if (!g_pd3dDevice) return;
    
            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
            D3DCompile(BENDS_HLSL_SRC, strlen(BENDS_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();
    
            D3DCompile(BENDS_HLSL_SRC, strlen(BENDS_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();
    
            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(BendsProps);
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            g_pd3dDevice->CreateBuffer(&bd, nullptr, &cbProps);
    
            initialized = true;
        }
    
        FBO CreateFBO(int w, int h, DXGI_FORMAT fmt) {
            FBO fbo;
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = w;
            desc.Height = h;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = fmt;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            g_pd3dDevice->CreateTexture2D(&desc, nullptr, &fbo.tex);
            g_pd3dDevice->CreateShaderResourceView(fbo.tex, nullptr, &fbo.srv);
            g_pd3dDevice->CreateRenderTargetView(fbo.tex, nullptr, &fbo.rtv);
            return fbo;
        }
    
        void Resize(int w, int h) {
            if (width == w && height == h) return;
            width = w; height = h;
    
            outputFBO.Release();
            outputFBO = CreateFBO(w, h, DXGI_FORMAT_R8G8B8A8_UNORM);
            props.uCanvas = { (float)w, (float)h };
        }
    
        void Update(float dt_sec, ImVec2 winSize, ImVec2 winPos) {
            if (!initialized) Init();
            
            if (winSize.x <= 0 || winSize.y <= 0) return;
            Resize((int)winSize.x, (int)winSize.y);
    
            timeAcc += dt_sec;
    
            props.uTime = timeAcc;
            props.uSpeed = 0.2f;
            
            float rotation = 90.0f;
            float autoRotate = 0.0f;
            float deg = fmod(rotation, 360.0f) + autoRotate * timeAcc;
            float rad = deg * 3.14159f / 180.0f;
            props.uRot = { cos(rad), sin(rad) };
            
            props.uColorCount = 3;
            props.uColors[0] = { 1.0f, 0.360f, 0.478f, 0.0f }; // #ff5c7a
            props.uColors[1] = { 0.541f, 0.360f, 1.0f, 0.0f }; // #8a5cff
            props.uColors[2] = { 0.0f, 1.0f, 0.819f, 0.0f };   // #00ffd1
            
            props.uTransparent = 1;
            props.uScale = 1.0f;
            props.uFrequency = 1.0f;
            props.uWarpStrength = 1.0f;
            props.uMouseInfluence = 1.0f;
            props.uParallax = 0.5f;
            props.uNoise = 0.15f;
            props.uIterations = 1;
            props.uIntensity = 1.5f;
            props.uBandWidth = 6.0f;
    
            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = (mousePos.x - winPos.x) / winSize.x;
            float relY = (mousePos.y - winPos.y) / winSize.y;
            
            float targetX = 0;
            float targetY = 0;
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                targetX = relX * 2.0f - 1.0f;
                targetY = -(relY * 2.0f - 1.0f);
            }
            
            float amt = min(1.0f, dt_sec * 8.0f);
            smoothMouse.x = smoothMouse.x + (targetX - smoothMouse.x) * amt;
            smoothMouse.y = smoothMouse.y + (targetY - smoothMouse.y) * amt;
            
            props.uPointer = { smoothMouse.x, smoothMouse.y };
    
            g_pd3dDeviceContext->UpdateSubresource(cbProps, 0, nullptr, &props, 0, 0);
    
            g_pd3dDeviceContext->OMSetRenderTargets(1, &outputFBO.rtv, nullptr);
            
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            g_pd3dDeviceContext->ClearRenderTargetView(outputFBO.rtv, clearColor);
            
            D3D11_VIEWPORT vp = {};
            vp.Width = (float)width;
            vp.Height = (float)height;
            vp.MaxDepth = 1.0f;
            g_pd3dDeviceContext->RSSetViewports(1, &vp);
    
            g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            g_pd3dDeviceContext->VSSetShader(vs, nullptr, 0);
            g_pd3dDeviceContext->PSSetShader(ps, nullptr, 0);
            g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &cbProps);
            g_pd3dDeviceContext->PSSetConstantBuffers(0, 1, &cbProps);
    
            g_pd3dDeviceContext->Draw(3, 0);
        }
    };
}
