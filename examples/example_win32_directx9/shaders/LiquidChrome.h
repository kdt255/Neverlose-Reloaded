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

inline const char* CHROME_HLSL_SRC = R"(
    cbuffer ChromeProps : register(b0) {
        float uTime;
        float uAmplitude;
        float uFrequencyX;
        float uFrequencyY;
        float3 uResolution;
        float padding1;
        float3 uBaseColor;
        float padding2;
        float2 uMouse;
        float2 padding3;
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

    float4 renderImage(float2 uvCoord) {
        float2 fragCoord = uvCoord * uResolution.xy;
        float2 uv = (2.0f * fragCoord - uResolution.xy) / min(uResolution.x, uResolution.y);

        for (int i = 1; i < 10; i++){
            float fi = (float)i;
            uv.x += uAmplitude / fi * cos(fi * uFrequencyX * uv.y + uTime + uMouse.x * 3.14159f);
            uv.y += uAmplitude / fi * cos(fi * uFrequencyY * uv.x + uTime + uMouse.y * 3.14159f);
        }

        float2 diff = (uvCoord - uMouse);
        float dist = length(diff);
        float falloff = exp(-dist * 20.0f);
        float ripple = sin(10.0f * dist - uTime * 2.0f) * 0.03f;
        uv += (diff / (dist + 0.0001f)) * ripple * falloff;

        float3 color = uBaseColor / abs(sin(uTime - uv.y - uv.x));
        return float4(color, 1.0f);
    }

    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float4 col = float4(0.0f, 0.0f, 0.0f, 0.0f);
        int samples = 0;
    
        float2 ogl_uv = float2(input.uv.x, 1.0f - input.uv.y);
    
        for (int i = -1; i <= 1; i++){
            for (int j = -1; j <= 1; j++){
                float2 offset = float2((float)i, (float)j) * (1.0f / min(uResolution.x, uResolution.y));
                col += renderImage(ogl_uv + offset);
                samples++;
            }
        }
        return col / (float)samples;
    }
    )";

    __declspec(align(16)) struct ChromeProps {
        float uTime;
        float uAmplitude;
        float uFrequencyX;
        float uFrequencyY;
        XMFLOAT3 uResolution;
        float padding1;
        XMFLOAT3 uBaseColor;
        float padding2;
        XMFLOAT2 uMouse;
        XMFLOAT2 padding3;
    };

    class LiquidChrome {
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
    
        ChromeProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
            D3DCompile(CHROME_HLSL_SRC, strlen(CHROME_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            D3DCompile(CHROME_HLSL_SRC, strlen(CHROME_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(ChromeProps);
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

            props.uResolution = { (float)w, (float)h, (float)w / (float)h };
        }

        void Update(float dt_sec, ImVec2 winSize, ImVec2 winPos) {
            if (!initialized) Init();
        
            if (winSize.x <= 0 || winSize.y <= 0) return;
            Resize((int)winSize.x, (int)winSize.y);

            float speed = 1.0f;
            timeAcc += dt_sec * speed;

            props.uTime = timeAcc;
            props.uAmplitude = 0.6f;
            props.uFrequencyX = 3.0f;
            props.uFrequencyY = 3.0f;
            props.uBaseColor = { 0.1f, 0.1f, 0.1f };

            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = (mousePos.x - winPos.x) / winSize.x;
            float relY = 1.0f - ((mousePos.y - winPos.y) / winSize.y);
        
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                props.uMouse = { relX, relY };
            }

            g_pd3dDeviceContext->UpdateSubresource(cbProps, 0, nullptr, &props, 0, 0);

            g_pd3dDeviceContext->OMSetRenderTargets(1, &outputFBO.rtv, nullptr);
        
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
