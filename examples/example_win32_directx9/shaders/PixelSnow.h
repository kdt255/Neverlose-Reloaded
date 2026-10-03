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

inline const char* SNOW_HLSL_SRC = R"(
    #define PI 3.14159265f
    #define PI_OVER_6 0.5235988f
    #define PI_OVER_3 1.0471976f
    #define F0 2.3283064e-10f

    #define M1 1597334677U
    #define M2 3812015801U
    #define M3 3299493293U

    static const float3 camK = float3(0.57735027f, 0.57735027f, 0.57735027f);
    static const float3 camI = float3(0.70710678f, 0.0f, -0.70710678f);
    static const float3 camJ = float3(-0.40824829f, 0.81649658f, -0.40824829f);
    static const float2 b1d = float2(0.574f, 0.819f);

    cbuffer SnowProps : register(b0) {
        float uTime;
        float uFlakeSize;
        float uMinFlakeSize;
        float uPixelResolution;

        float2 uResolution;
        float uSpeed;
        float uDepthFade;

        float uFarPlane;
        float uBrightness;
        float uGamma;
        float uDensity;

        float3 uColor;
        float uVariant;

        float uDirection;
        float3 padding;
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

    uint hash(uint n) { return n * (n ^ (n >> 15)); }

    uint coord3(float3 p) {
        uint3 up = (uint3)(int3)p;
        return (up.x * M1) ^ (up.y * M2) ^ (up.z * M3);
    }

    float3 hash3(uint n) {
        uint3 hashed = hash(n) * uint3(1U, 511U, 262143U);
        return float3(hashed) * F0;
    }

    float glsl_mod(float x, float y) {
        return x - y * floor(x / y);
    }

    float snowflakeDist(float2 p) {
        float r = length(p);
        float a = atan2(p.y, p.x);
        a = abs(glsl_mod(a + PI_OVER_6, PI_OVER_3) - PI_OVER_6);
        float2 q = r * float2(cos(a), sin(a));
        float dMain = max(abs(q.y), max(-q.x, q.x - 1.0f));
        float b1t = clamp(dot(q - float2(0.4f, 0.0f), b1d), 0.0f, 0.4f);
        float dB1 = length(q - float2(0.4f, 0.0f) - b1t * b1d);
        float b2t = clamp(dot(q - float2(0.7f, 0.0f), b1d), 0.0f, 0.25f);
        float dB2 = length(q - float2(0.7f, 0.0f) - b2t * b1d);
        return min(dMain, min(dB1, dB2)) * 10.0f;
    }

    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float invPixelRes = 1.0f / uPixelResolution;
        float pixelSize = max(1.0f, floor(0.5f + uResolution.x * invPixelRes));
        float invPixelSize = 1.0f / pixelSize;
    
        float2 fragCoord = floor(float2(input.pos.x, uResolution.y - input.pos.y) * invPixelSize);
        float2 res = uResolution * invPixelSize;
        float invResX = 1.0f / res.x;

        float3 ray = normalize(float3((fragCoord - res * 0.5f) * invResX, 1.0f));
        ray = ray.x * camI + ray.y * camJ + ray.z * camK;

        float timeSpeed = uTime * uSpeed;
        float windX = cos(uDirection) * 0.4f;
        float windY = sin(uDirection) * 0.4f;
        float3 camPos = (windX * camI + windY * camJ + 0.1f * camK) * timeSpeed;
        float3 pos = camPos;

        float3 absRay = max(abs(ray), float3(0.001f, 0.001f, 0.001f));
        float3 strides = 1.0f / absRay;
        float3 raySign = step(ray, float3(0.0f, 0.0f, 0.0f));
        float3 phase = frac(pos) * strides;
        phase = lerp(strides - phase, phase, raySign);

        float rayDotCamK = dot(ray, camK);
        float invRayDotCamK = 1.0f / rayDotCamK;
        float invDepthFade = 1.0f / uDepthFade;
        float halfInvResX = 0.5f * invResX;
        float3 timeAnim = timeSpeed * 0.1f * float3(7.0f, 8.0f, 5.0f);

        float t = 0.0f;
        for (int i = 0; i < 128; i++) {
            if (t >= uFarPlane) break;
        
            float3 fpos = floor(pos);
            uint cellCoord = coord3(fpos);
            float cellHash = hash3(cellCoord).x;

            if (cellHash < uDensity) {
                float3 h = hash3(cellCoord);
            
                float3 sinArg1 = fpos.yzx * 0.073f;
                float3 sinArg2 = fpos.zxy * 0.27f;
                float3 flakePos = 0.5f - 0.5f * cos(4.0f * sin(sinArg1) + 4.0f * sin(sinArg2) + 2.0f * h + timeAnim);
                flakePos = flakePos * 0.8f + 0.1f + fpos;

                float toIntersection = dot(flakePos - pos, camK) * invRayDotCamK;
            
                if (toIntersection > 0.0f) {
                    float3 testPos = pos + ray * toIntersection - flakePos;
                    float testX = dot(testPos, camI);
                    float testY = dot(testPos, camJ);
                    float2 testUV = abs(float2(testX, testY));
                
                    float depth = dot(flakePos - camPos, camK);
                    float flakeSize = max(uFlakeSize, uMinFlakeSize * depth * halfInvResX);
                
                    float dist;
                    if (uVariant < 0.5f) {
                        dist = max(testUV.x, testUV.y);
                    } else if (uVariant < 1.5f) {
                        dist = length(testUV);
                    } else {
                        float invFlakeSize = 1.0f / flakeSize;
                        dist = snowflakeDist(float2(testX, testY) * invFlakeSize) * flakeSize;
                    }

                    if (dist < flakeSize) {
                        float flakeSizeRatio = uFlakeSize / flakeSize;
                        float intensity = exp2(-(t + toIntersection) * invDepthFade) * min(1.0f, flakeSizeRatio * flakeSizeRatio) * uBrightness;
                    
                        float3 colorPow = pow(float3(intensity, intensity, intensity), float3(uGamma, uGamma, uGamma));
                        return float4(uColor * colorPow, 1.0f);
                    }
                }
            }

            float nextStep = min(min(phase.x, phase.y), phase.z);
            float3 sel = step(phase, float3(nextStep, nextStep, nextStep));
            phase = phase - nextStep + strides * sel;
            t += nextStep;
            pos = lerp(pos + ray * nextStep, floor(pos + ray * nextStep + 0.5f), sel);
        }

        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    )";

    __declspec(align(16)) struct SnowProps {
        float uTime;
        float uFlakeSize;
        float uMinFlakeSize;
        float uPixelResolution;

        XMFLOAT2 uResolution;
        float uSpeed;
        float uDepthFade;

        float uFarPlane;
        float uBrightness;
        float uGamma;
        float uDensity;

        XMFLOAT3 uColor;
        float uVariant;

        float uDirection;
        XMFLOAT3 padding;
    };

    class PixelSnow {
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
    
        SnowProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
        
            D3DCompile(SNOW_HLSL_SRC, strlen(SNOW_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            D3DCompile(SNOW_HLSL_SRC, strlen(SNOW_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(SnowProps);
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
            props.uResolution = { (float)w, (float)h };
        }

        void Update(float dt_sec, ImVec2 winSize, ImVec2 winPos) {
            if (!initialized) Init();
        
            if (winSize.x <= 0 || winSize.y <= 0) return;
            Resize((int)winSize.x, (int)winSize.y);

            timeAcc += dt_sec;

            props.uTime = timeAcc;
            props.uFlakeSize = 0.01f;
            props.uMinFlakeSize = 1.25f;
            props.uPixelResolution = 200.0f;
            props.uSpeed = 1.25f;
            props.uDepthFade = 8.0f;
            props.uFarPlane = 20.0f;
            props.uBrightness = 1.0f;
            props.uGamma = 0.4545f;
            props.uDensity = 0.3f;
            props.uVariant = 0.0f;
            props.uDirection = 125.0f * 3.14159265f / 180.0f;
            props.uColor = { 1.0f, 1.0f, 1.0f }; // #ffffff

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
