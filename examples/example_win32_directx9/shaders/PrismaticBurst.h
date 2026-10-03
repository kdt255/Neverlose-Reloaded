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

inline const char* PRISMATIC_HLSL_SRC = R"(
    cbuffer PrismaticProps : register(b0) {
        float2 uResolution;
        float uTime;
        float uIntensity;
    
        float uSpeed;
        int uAnimType;
        float2 uMouse;
    
        int uColorCount;
        float uDistort;
        float2 uOffset;
    
        float uNoiseAmount;
        int uRayCount;
        float2 padding;
    
        float4 uColors[8];
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

    float fmod(float x, float y) {
        return x - y * floor(x / y);
    }
    float2 fmod2(float2 x, float y) {
        return float2(fmod(x.x, y), fmod(x.y, y));
    }

    float hash21(float2 p){
        p = floor(p);
        float f = 52.9829189f * frac(dot(p, float2(0.065f, 0.005f)));
        return frac(f);
    }

    float layeredNoise(float2 fragPx){
        float2 p = fmod2(fragPx + float2(uTime * 30.0f, -uTime * 21.0f), 1024.0f);
        float2 q = float2(0.8f * p.x + 0.5f * p.y, -0.5f * p.x + 0.8f * p.y);
        float n = 0.0f;
        n += 0.40f * hash21(q);
        n += 0.25f * hash21(q * 2.0f + 17.0f);
        n += 0.20f * hash21(q * 4.0f + 47.0f);
        n += 0.10f * hash21(q * 8.0f + 113.0f);
        n += 0.05f * hash21(q * 16.0f + 191.0f);
        return n;
    }

    float3 rayDir(float2 frag, float2 res, float2 offset, float dist){
        float focal = res.y * max(dist, 1e-3f);
        return normalize(float3(2.0f * (frag - offset) - res, focal));
    }

    float edgeFade(float2 frag, float2 res, float2 offset){
        float2 toC = frag - 0.5f * res - offset;
        float r = length(toC) / (0.5f * min(res.x, res.y));
        float x = clamp(r, 0.0f, 1.0f);
        float q = x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
        float s = q * 0.5f;
        s = pow(s, 1.5f);
        float tail = 1.0f - pow(1.0f - s, 2.0f);
        s = lerp(s, tail, 0.2f);
        float dn = (layeredNoise(frag * 0.15f) - 0.5f) * 0.0015f * s;
        return clamp(s + dn, 0.0f, 1.0f);
    }

    float3 mulRotX(float3 v, float a) { float c = cos(a), s = sin(a); return float3(v.x, c * v.y - s * v.z, s * v.y + c * v.z); }
    float3 mulRotY(float3 v, float a) { float c = cos(a), s = sin(a); return float3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z); }
    float3 mulRotZ(float3 v, float a) { float c = cos(a), s = sin(a); return float3(c * v.x - s * v.y, s * v.x + c * v.y, v.z); }

    float2 rot2(float2 v, float a){
        float s = sin(a), c = cos(a);
        return float2(c * v.x - s * v.y, s * v.x + c * v.y);
    }

    float3 sampleGradient(float t, int count) {
        if (count <= 1) return uColors[0].xyz;
        t = clamp(t, 0.0f, 1.0f);
        float scaled = t * (float)(count - 1);
        int idx = clamp((int)floor(scaled), 0, count - 2);
        float f = frac(scaled);
        return lerp(uColors[idx].xyz, uColors[idx + 1].xyz, f);
    }

    float bendAngle(float3 q, float t){
        float a = 0.8f * sin(q.x * 0.55f + t * 0.6f)
                + 0.7f * sin(q.y * 0.50f - t * 0.5f)
                + 0.6f * sin(q.z * 0.60f + t * 0.7f);
        return a;
    }

    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float2 frag = float2(input.pos.x, uResolution.y - input.pos.y);
        float t = uTime * uSpeed;
        float jitterAmp = 0.1f * clamp(uNoiseAmount, 0.0f, 1.0f);
        float3 dir = rayDir(frag, uResolution, uOffset, 1.0f);
        float marchT = 0.0f;
        float3 col = float3(0.0f, 0.0f, 0.0f);
        float n = layeredNoise(frag);
        float4 c = cos(t * 0.2f + float4(0.0f, 33.0f, 11.0f, 0.0f));
        float amp = clamp(uDistort, 0.0f, 50.0f) * 0.15f;

        for (int i = 0; i < 44; ++i) {
            float3 P = marchT * dir;
            P.z -= 2.0f;
            float rad = length(P);
            float3 Pl = P * (10.0f / max(rad, 1e-6f));

            if(uAnimType == 0){
                Pl.xz = float2(c.x * Pl.x + c.z * Pl.z, c.y * Pl.x + c.w * Pl.z);
            } else if(uAnimType == 1){
                float3 ang = float3(t * 0.31f, t * 0.21f, t * 0.17f);
                Pl = mulRotX(Pl, ang.x);
                Pl = mulRotY(Pl, ang.y);
                Pl = mulRotZ(Pl, ang.z);
            } else {
                float2 m = uMouse * 2.0f - 1.0f;
                float3 ang = float3(m.y * 0.6f, m.x * 0.6f, 0.0f);
                Pl = mulRotX(Pl, ang.x);
                Pl = mulRotY(Pl, ang.y);
            }

            float stepLen = min(rad - 0.3f, n * jitterAmp) + 0.1f;

            float grow = smoothstep(0.35f, 3.0f, marchT);
            float a1 = amp * grow * bendAngle(Pl * 0.6f, t);
            float a2 = 0.5f * amp * grow * bendAngle(float3(Pl.z, Pl.y, Pl.x) * 0.5f + 3.1f, t * 0.9f);
            float3 Pb = Pl;
            Pb.xz = rot2(Pb.xz, a1);
            Pb.xy = rot2(Pb.xy, a2);

            float rayPattern = smoothstep(
                0.5f, 0.7f,
                sin(Pb.x + cos(Pb.y) * cos(Pb.z)) *
                sin(Pb.z + sin(Pb.y) * cos(Pb.x + t))
            );

            if (uRayCount > 0) {
                float ang = atan2(Pb.y, Pb.x);
                float comb = 0.5f + 0.5f * cos((float)uRayCount * ang);
                comb = pow(comb, 3.0f);
                rayPattern *= smoothstep(0.15f, 0.95f, comb);
            }

            float3 spectralDefault = 1.0f + float3(
                cos(marchT * 3.0f + 0.0f),
                cos(marchT * 3.0f + 1.0f),
                cos(marchT * 3.0f + 2.0f)
            );

            float saw = frac(marchT * 0.25f);
            float tRay = saw * saw * (3.0f - 2.0f * saw);
            float3 userGradient = 2.0f * sampleGradient(tRay, uColorCount);
            float3 spectral = (uColorCount > 0) ? userGradient : spectralDefault;
            float3 base = (0.05f / (0.4f + stepLen))
                      * smoothstep(5.0f, 0.0f, rad)
                      * spectral;

            col += base * rayPattern;
            marchT += stepLen;
        }

        col *= edgeFade(frag, uResolution, uOffset);
        col *= uIntensity;

        return float4(clamp(col, 0.0f, 1.0f), 1.0f);
    }
    )";

    __declspec(align(16)) struct PrismaticProps {
        XMFLOAT2 uResolution;
        float uTime;
        float uIntensity;
    
        float uSpeed;
        int uAnimType;
        XMFLOAT2 uMouse;
    
        int uColorCount;
        float uDistort;
        XMFLOAT2 uOffset;
    
        float uNoiseAmount;
        int uRayCount;
        XMFLOAT2 padding;
    
        XMFLOAT4 uColors[8];
    };

    class PrismaticBurst {
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
    
        PrismaticProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;
    
        ImVec2 targetMousePos = {0.5f, 0.5f};
        ImVec2 smoothMousePos = {0.5f, 0.5f};

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
        
            D3DCompile(PRISMATIC_HLSL_SRC, strlen(PRISMATIC_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            D3DCompile(PRISMATIC_HLSL_SRC, strlen(PRISMATIC_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(PrismaticProps);
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
            props.uIntensity = 2.0f;
            props.uSpeed = 0.5f;
            props.uAnimType = 1;
            props.uDistort = 0.0f;
            props.uOffset = { 0.0f, 0.0f };
            props.uNoiseAmount = 0.8f;
            props.uRayCount = 0;
        
            props.uColorCount = 3;
            props.uColors[0] = { 1.0f, 0.0f, 0.478f, 1.0f }; // #ff007a
            props.uColors[1] = { 0.302f, 0.239f, 1.0f, 1.0f }; // #4d3dff
            props.uColors[2] = { 1.0f, 1.0f, 1.0f, 1.0f }; // #ffffff

            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = (mousePos.x - winPos.x) / winSize.x;
            float relY = (mousePos.y - winPos.y) / winSize.y;

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                targetMousePos.x = relX;
                targetMousePos.y = 1.0f - relY;
            }

            float hoverDampness = 0.25f;
            float tau = 0.02f + hoverDampness * 0.5f;
            float alpha = 1.0f - exp(-dt_sec / tau);
        
            smoothMousePos.x += (targetMousePos.x - smoothMousePos.x) * alpha;
            smoothMousePos.y += (targetMousePos.y - smoothMousePos.y) * alpha;

            props.uMouse = { smoothMousePos.x, smoothMousePos.y };

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
