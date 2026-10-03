#pragma once

#include "imgui.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;

namespace shader {

using namespace DirectX;

inline const char* PLASMA_HLSL_SRC = R"(
    cbuffer PlasmaProps : register(b0) {
        float2 iResolution;
        float iTime;
        float uUseCustomColor;
    
        float3 uCustomColor;
        float uSpeed;
    
        float uDirection;
        float uScale;
        float uOpacity;
        float uMouseInteractive;
    
        float2 uMouse;
        float2 padding;
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

    float sanitize1(float x) {
        return (isnan(x) || isinf(x)) ? 0.0f : x;
    }

    float3 sanitize(float3 c) {
        return float3(sanitize1(c.x), sanitize1(c.y), sanitize1(c.z));
    }

    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float2 C = float2(input.pos.x, iResolution.y - input.pos.y);
        float2 center = iResolution.xy * 0.5f;
        C = (C - center) / max(uScale, 0.001f) + center;
    
        float2 mouseOffset = (uMouse - center) * 0.0002f;
        if (uMouseInteractive > 0.5f) {
            C += mouseOffset * length(C - center);
        }
    
        float d = 0.0f, z = 0.0f;
        float T = iTime * uSpeed * uDirection;
        float3 O = float3(0.0f, 0.0f, 0.0f);
        float3 p = float3(0.0f, 0.0f, 0.0f);
        float3 S = float3(0.0f, 0.0f, 0.0f);
        float4 o = float4(0.0f, 0.0f, 0.0f, 0.0f);

        float2 r = iResolution.xy;
        float2 Q = float2(0.0f, 0.0f);

        for (int iter = 0; iter < 60; ++iter) {
            p = z * normalize(float3(C - 0.5f * r, r.y)); 
            p.z -= 4.0f; 
            S = p;
            d = p.y - T;
        
            p.x += 0.4f * (1.0f + p.y) * sin(d + p.x * 0.1f) * cos(0.34f * d + p.x * 0.05f); 
        
            float angle1 = cos(p.y - T);
            float angle2 = cos(p.y + 11.0f - T);
            float angle3 = cos(p.y + 33.0f - T);
            float angle4 = cos(p.y - T);
        
            float new_x = p.x * angle1 + p.z * angle2;
            float new_z = p.x * angle3 + p.z * angle4;
            p.x = new_x;
            p.z = new_z;
            Q = float2(p.x, p.z);
        
            float lenQQ = sqrt(Q.x*Q.x*Q.x*Q.x + Q.y*Q.y*Q.y*Q.y);
            d = abs(sqrt(lenQQ) - 0.25f * (5.0f + S.y)) / 3.0f + 8e-4f; 
            z += d; 
        
            o = 1.0f + sin(S.y + p.z * 0.5f + S.z - length(S - p) + float4(2.0f, 1.0f, 0.0f, 8.0f));
            O += (o.w / d) * o.xyz;
        }
    
        o.xyz = tanh(O / 1e4f);
        float3 rgb = sanitize(o.xyz);
    
        float intensity = (rgb.x + rgb.y + rgb.z) / 3.0f;
        float3 customColor = intensity * uCustomColor;
        float3 finalColor = uUseCustomColor > 0.5f ? customColor : rgb;
    
        float alpha = length(rgb) * uOpacity;
        return float4(finalColor, alpha);
    }
    )";

    __declspec(align(16)) struct PlasmaProps {
        XMFLOAT2 iResolution;
        float iTime;
        float uUseCustomColor;
    
        XMFLOAT3 uCustomColor;
        float uSpeed;
    
        float uDirection;
        float uScale;
        float uOpacity;
        float uMouseInteractive;
    
        XMFLOAT2 uMouse;
        XMFLOAT2 padding;
    };

    class Plasma {
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
    
        PlasmaProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;
    
        ImVec2 smoothMouse = {0.0f, 0.0f};

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
            D3DCompile(PLASMA_HLSL_SRC, strlen(PLASMA_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            D3DCompile(PLASMA_HLSL_SRC, strlen(PLASMA_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(PlasmaProps);
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
            props.iResolution = { (float)w, (float)h };
        }

        void Update(float dt_sec, ImVec2 winSize, ImVec2 winPos) {
            if (!initialized) Init();
        
            if (winSize.x <= 0 || winSize.y <= 0) return;
            Resize((int)winSize.x, (int)winSize.y);

            timeAcc += dt_sec;

            props.iTime = timeAcc;
            props.uSpeed = 0.6f * 0.4f;
            props.uDirection = 1.0f;
        
            props.uUseCustomColor = 1.0f;
            props.uCustomColor = { 1.0f, 0.42f, 0.208f }; // #ff6b35
        
            props.uScale = 1.1f;
            props.uOpacity = 0.8f;
            props.uMouseInteractive = 1.0f;

            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = mousePos.x - winPos.x;
            float relY = mousePos.y - winPos.y;
        
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                smoothMouse.x = relX;
                smoothMouse.y = relY;
            }
        
            props.uMouse = { smoothMouse.x, smoothMouse.y };

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
