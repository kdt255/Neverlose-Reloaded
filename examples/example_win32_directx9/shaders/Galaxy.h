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

inline const char* GALAXY_HLSL_SRC = R"(
    cbuffer GalaxyProps : register(b0) {
        float3 uResolution;
        float uTime;
    
        float2 uFocal;
        float2 uRotation;
    
        float uStarSpeed;
        float uDensity;
        float uHueShift;
        float uSpeed;
    
        float2 uMouse;
        float uGlowIntensity;
        float uSaturation;
    
        int uMouseRepulsion;
        float uTwinkleIntensity;
        float uRotationSpeed;
        float uRepulsionStrength;
    
        float uMouseActiveFactor;
        float uAutoCenterRepulsion;
        int uTransparent;
        float padding;
    };

    struct VS_OUTPUT {
        float4 pos : SV_POSITION;
    };

    VS_OUTPUT vs_main(uint id : SV_VertexID) {
        VS_OUTPUT output;
        float2 uv = float2((id << 1) & 2, id & 2);
        output.pos = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
        return output;
    }

    #define NUM_LAYER 4.0f
    #define STAR_COLOR_CUTOFF 0.2f
    #define PERIOD 3.0f

    float2 rotate(float2 v, float a) {
        float s = sin(a);
        float c = cos(a);
        return float2(v.x * c - v.y * s, v.x * s + v.y * c);
    }

    float Hash21(float2 p) {
        p = frac(p * float2(123.34f, 456.21f));
        p += dot(p, p + 45.32f);
        return frac(p.x * p.y);
    }

    float tri(float x) {
        return abs(frac(x) * 2.0f - 1.0f);
    }

    float tris(float x) {
        float t = frac(x);
        return 1.0f - smoothstep(0.0f, 1.0f, abs(2.0f * t - 1.0f));
    }

    float trisn(float x) {
        float t = frac(x);
        return 2.0f * (1.0f - smoothstep(0.0f, 1.0f, abs(2.0f * t - 1.0f))) - 1.0f;
    }

    float3 hsv2rgb(float3 c) {
        float4 K = float4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);
        float3 p = abs(frac(c.xxx + K.xyz) * 6.0f - K.www);
        return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0f, 1.0f), c.y);
    }

    float Star(float2 uv, float flare) {
        float d = length(uv);
        float m = (0.05f * uGlowIntensity) / max(d, 0.0001f);
        float rays = smoothstep(0.0f, 1.0f, 1.0f - abs(uv.x * uv.y * 1000.0f));
        m += rays * flare * uGlowIntensity;
        uv = rotate(uv, -0.785398f);
        rays = smoothstep(0.0f, 1.0f, 1.0f - abs(uv.x * uv.y * 1000.0f));
        m += rays * 0.3f * flare * uGlowIntensity;
        m *= smoothstep(1.0f, 0.2f, d);
        return m;
    }

    float3 StarLayer(float2 uv) {
        float3 col = float3(0.0f, 0.0f, 0.0f);

        float2 gv = frac(uv) - 0.5f; 
        float2 id = floor(uv);

        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                float2 offset = float2((float)x, (float)y);
                float2 si = id + offset;
                float seed = Hash21(si);
                float size = frac(seed * 345.32f);
                float glossLocal = tri(uStarSpeed / (PERIOD * seed + 1.0f));
                float flareSize = smoothstep(0.9f, 1.0f, size) * glossLocal;

                float red = smoothstep(STAR_COLOR_CUTOFF, 1.0f, Hash21(si + 1.0f)) + STAR_COLOR_CUTOFF;
                float blu = smoothstep(STAR_COLOR_CUTOFF, 1.0f, Hash21(si + 3.0f)) + STAR_COLOR_CUTOFF;
                float grn = min(red, blu) * seed;
                float3 base = float3(red, grn, blu);
            
                float hue = atan2(base.g - base.r, base.b - base.r) / (2.0f * 3.14159f) + 0.5f;
                hue = frac(hue + uHueShift / 360.0f);
                float luma = dot(base, float3(0.299f, 0.587f, 0.114f));
                float sat = length(base - float3(luma, luma, luma)) * uSaturation;
                float val = max(max(base.r, base.g), base.b);
                base = hsv2rgb(float3(hue, sat, val));

                float2 pad = float2(tris(seed * 34.0f + uTime * uSpeed / 10.0f), tris(seed * 38.0f + uTime * uSpeed / 30.0f)) - 0.5f;

                float star = Star(gv - offset - pad, flareSize);
                float3 color = base;

                float twinkle = trisn(uTime * uSpeed + seed * 6.2831f) * 0.5f + 1.0f;
                twinkle = lerp(1.0f, twinkle, uTwinkleIntensity);
                star *= twinkle;
            
                col += star * size * color;
            }
        }

        return col;
    }

    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float2 vUv = float2(input.pos.x / uResolution.x, 1.0f - input.pos.y / uResolution.y);
        float2 focalPx = uFocal * uResolution.xy;
        float2 uv = (vUv * uResolution.xy - focalPx) / uResolution.y;

        float2 mouseNorm = uMouse - float2(0.5f, 0.5f);
    
        if (uAutoCenterRepulsion > 0.0f) {
            float2 centerUV = float2(0.0f, 0.0f);
            float centerDist = length(uv - centerUV);
            float2 repulsion = normalize(uv - centerUV) * (uAutoCenterRepulsion / (centerDist + 0.1f));
            uv += repulsion * 0.05f;
        } else if (uMouseRepulsion > 0) {
            float2 mousePosUV = (uMouse * uResolution.xy - focalPx) / uResolution.y;
            float mouseDist = length(uv - mousePosUV);
            float2 repulsion = normalize(uv - mousePosUV) * (uRepulsionStrength / (mouseDist + 0.1f));
            uv += repulsion * 0.05f * uMouseActiveFactor;
        } else {
            float2 mouseOffset = mouseNorm * 0.1f * uMouseActiveFactor;
            uv += mouseOffset;
        }

        float autoRotAngle = uTime * uRotationSpeed;
        uv = rotate(uv, autoRotAngle);
    
        uv = float2(
            uv.x * uRotation.x - uv.y * uRotation.y,
            uv.x * uRotation.y + uv.y * uRotation.x
        );

        float3 col = float3(0.0f, 0.0f, 0.0f);

        for (float i = 0.0f; i < 1.0f; i += 1.0f / NUM_LAYER) {
            float depth = frac(i + uStarSpeed * uSpeed);
            float scale = lerp(20.0f * uDensity, 0.5f * uDensity, depth);
            float fade = depth * smoothstep(1.0f, 0.9f, depth);
            col += StarLayer(uv * scale + i * 453.32f) * fade;
        }

        if (uTransparent > 0) {
            float alpha = length(col);
            alpha = smoothstep(0.0f, 0.3f, alpha);
            alpha = min(alpha, 1.0f);
            return float4(col, alpha);
        } else {
            return float4(col, 1.0f);
        }
    }
    )";

    __declspec(align(16)) struct GalaxyProps {
        XMFLOAT3 uResolution;
        float uTime;
    
        XMFLOAT2 uFocal;
        XMFLOAT2 uRotation;
    
        float uStarSpeed;
        float uDensity;
        float uHueShift;
        float uSpeed;
    
        XMFLOAT2 uMouse;
        float uGlowIntensity;
        float uSaturation;
    
        int uMouseRepulsion;
        float uTwinkleIntensity;
        float uRotationSpeed;
        float uRepulsionStrength;
    
        float uMouseActiveFactor;
        float uAutoCenterRepulsion;
        int uTransparent;
        float padding;
    };

    class Galaxy {
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
    
        GalaxyProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;
    
        ImVec2 targetMousePos = {0.5f, 0.5f};
        ImVec2 smoothMousePos = {0.5f, 0.5f};
        float targetMouseActive = 0.0f;
        float smoothMouseActive = 0.0f;

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
        
            D3DCompile(GALAXY_HLSL_SRC, strlen(GALAXY_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            D3DCompile(GALAXY_HLSL_SRC, strlen(GALAXY_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(GalaxyProps);
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

            timeAcc += dt_sec;

            float starSpeed = 0.5f;
            props.uTime = timeAcc;
            props.uStarSpeed = (timeAcc * starSpeed) / 10.0f;
            props.uDensity = 1.0f;
            props.uHueShift = 140.0f;
            props.uSpeed = 1.0f;
            props.uGlowIntensity = 0.3f;
            props.uSaturation = 0.0f;
            props.uMouseRepulsion = 1;
            props.uTwinkleIntensity = 0.3f;
            props.uRotationSpeed = 0.1f;
            props.uRepulsionStrength = 2.0f;
            props.uAutoCenterRepulsion = 0.0f;
            props.uTransparent = 1;
            props.uFocal = { 0.5f, 0.5f };
            props.uRotation = { 1.0f, 0.0f };

            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = (mousePos.x - winPos.x) / winSize.x;
            float relY = (mousePos.y - winPos.y) / winSize.y;

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                targetMousePos.x = relX;
                targetMousePos.y = 1.0f - relY;
                targetMouseActive = 1.0f;
            } else {
                targetMouseActive = 0.0f;
            }

            float lerpFactor = 0.05f;
            smoothMousePos.x += (targetMousePos.x - smoothMousePos.x) * lerpFactor;
            smoothMousePos.y += (targetMousePos.y - smoothMousePos.y) * lerpFactor;
            smoothMouseActive += (targetMouseActive - smoothMouseActive) * lerpFactor;

            props.uMouse = { smoothMousePos.x, smoothMousePos.y };
            props.uMouseActiveFactor = smoothMouseActive;

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
