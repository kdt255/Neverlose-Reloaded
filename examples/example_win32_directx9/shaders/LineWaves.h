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

inline const char* WAVES_HLSL_SRC = R"(
    #define HALF_PI 1.5707963f

    cbuffer WavesProps : register(b0) {
        float uTime;
        float uSpeed;
        float uInnerLines;
        float uOuterLines;
    
        float uWarpIntensity;
        float uRotation;
        float uEdgeFadeWidth;
        float uColorCycleSpeed;
    
        float uBrightness;
        float uMouseInfluence;
        int uEnableMouse;
        float padding1;
    
        float3 uColor1;
        float padding2;
    
        float3 uColor2;
        float padding3;
    
        float3 uColor3;
        float padding4;
    
        float2 uMouse;
        float2 padding5;
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

    float hashF(float n) {
        return frac(sin(n * 127.1f) * 43758.5453123f);
    }

    float smoothNoise(float x) {
        float i = floor(x);
        float f = frac(x);
        float u = f * f * (3.0f - 2.0f * f);
        return lerp(hashF(i), hashF(i + 1.0f), u);
    }

    float displaceA(float coord, float t) {
        float result = sin(coord * 2.123f) * 0.2f;
        result += sin(coord * 3.234f + t * 4.345f) * 0.1f;
        result += sin(coord * 0.589f + t * 0.934f) * 0.5f;
        return result;
    }

    float displaceB(float coord, float t) {
        float result = sin(coord * 1.345f) * 0.3f;
        result += sin(coord * 2.734f + t * 3.345f) * 0.2f;
        result += sin(coord * 0.189f + t * 0.934f) * 0.3f;
        return result;
    }

    float2 rotate2D(float2 p, float angle) {
        float c = cos(angle);
        float s = sin(angle);
        return float2(p.x * c - p.y * s, p.x * s + p.y * c);
    }

    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float2 coords = float2(input.uv.x, 1.0f - input.uv.y);
        coords = coords * 2.0f - 1.0f;
        coords = rotate2D(coords, uRotation);

        float halfT = uTime * uSpeed * 0.5f;
        float fullT = uTime * uSpeed;

        float mouseWarp = 0.0f;
        if (uEnableMouse > 0) {
            float2 mPos = rotate2D(uMouse * 2.0f - 1.0f, uRotation);
            float mDist = length(coords - mPos);
            mouseWarp = uMouseInfluence * exp(-mDist * mDist * 4.0f);
        }

        float warpAx = coords.x + displaceA(coords.y, halfT) * uWarpIntensity + mouseWarp;
        float warpAy = coords.y - displaceA(coords.x * cos(fullT) * 1.235f, halfT) * uWarpIntensity;
        float warpBx = coords.x + displaceB(coords.y, halfT) * uWarpIntensity + mouseWarp;
        float warpBy = coords.y - displaceB(coords.x * sin(fullT) * 1.235f, halfT) * uWarpIntensity;

        float2 fieldA = float2(warpAx, warpAy);
        float2 fieldB = float2(warpBx, warpBy);
        float2 mixFactor = lerp(fieldA, fieldB, 0.5f);
        float2 blended = lerp(fieldA, fieldB, mixFactor);

        float fadeTop = smoothstep(uEdgeFadeWidth, uEdgeFadeWidth + 0.4f, blended.y);
        float fadeBottom = smoothstep(-uEdgeFadeWidth, -(uEdgeFadeWidth + 0.4f), blended.y);
        float vMask = 1.0f - max(fadeTop, fadeBottom);

        float tileCount = lerp(uOuterLines, uInnerLines, vMask);
        float scaledY = blended.y * tileCount;
        float nY = smoothNoise(abs(scaledY));

        float ridge = pow(
            step(abs(nY - blended.x) * 2.0f, HALF_PI) * cos(2.0f * (nY - blended.x)),
            5.0f
        );

        float lines = 0.0f;
        for (float i = 1.0f; i < 3.0f; i += 1.0f) {
            lines += pow(max(frac(scaledY), frac(-scaledY)), i * 2.0f);
        }

        float pattern = vMask * lines;

        float cycleT = fullT * uColorCycleSpeed;
        float rChannel = (pattern + lines * ridge) * (cos(blended.y + cycleT * 0.234f) * 0.5f + 1.0f);
        float gChannel = (pattern + vMask * ridge) * (sin(blended.x + cycleT * 1.745f) * 0.5f + 1.0f);
        float bChannel = (pattern + lines * ridge) * (cos(blended.x + cycleT * 0.534f) * 0.5f + 1.0f);

        float3 col = (rChannel * uColor1 + gChannel * uColor2 + bChannel * uColor3) * uBrightness;
        float alpha = clamp(length(col), 0.0f, 1.0f);

        return float4(col, alpha);
    }
    )";

    __declspec(align(16)) struct WavesProps {
        float uTime;
        float uSpeed;
        float uInnerLines;
        float uOuterLines;
    
        float uWarpIntensity;
        float uRotation;
        float uEdgeFadeWidth;
        float uColorCycleSpeed;
    
        float uBrightness;
        float uMouseInfluence;
        int uEnableMouse;
        float padding1;
    
        XMFLOAT3 uColor1;
        float padding2;
    
        XMFLOAT3 uColor2;
        float padding3;
    
        XMFLOAT3 uColor3;
        float padding4;
    
        XMFLOAT2 uMouse;
        XMFLOAT2 padding5;
    };

    class LineWaves {
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
    
        WavesProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;
    
        ImVec2 smoothMouse = {0.5f, 0.5f};

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
            D3DCompile(WAVES_HLSL_SRC, strlen(WAVES_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            D3DCompile(WAVES_HLSL_SRC, strlen(WAVES_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(WavesProps);
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
        }

        void Update(float dt_sec, ImVec2 winSize, ImVec2 winPos) {
            if (!initialized) Init();
        
            if (winSize.x <= 0 || winSize.y <= 0) return;
            Resize((int)winSize.x, (int)winSize.y);

            timeAcc += dt_sec;

            props.uTime = timeAcc;
            props.uSpeed = 0.3f;
            props.uInnerLines = 32.0f;
            props.uOuterLines = 36.0f;
            props.uWarpIntensity = 1.0f;
            props.uRotation = -45.0f * 3.14159f / 180.0f;
            props.uEdgeFadeWidth = 0.0f;
            props.uColorCycleSpeed = 1.0f;
            props.uBrightness = 0.2f;
        
            props.uColor1 = { 1.0f, 1.0f, 1.0f };
            props.uColor2 = { 1.0f, 1.0f, 1.0f };
            props.uColor3 = { 1.0f, 1.0f, 1.0f };
        
            props.uEnableMouse = 1;
            props.uMouseInfluence = 2.0f;

            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = (mousePos.x - winPos.x) / winSize.x;
            float relY = (mousePos.y - winPos.y) / winSize.y;
        
            float targetX = 0.5f;
            float targetY = 0.5f;
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                targetX = relX;
                targetY = 1.0f - relY;
            }
        
            float smoothing = 0.05f;
            smoothMouse.x = smoothMouse.x + (targetX - smoothMouse.x) * smoothing;
            smoothMouse.y = smoothMouse.y + (targetY - smoothMouse.y) * smoothing;
        
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
