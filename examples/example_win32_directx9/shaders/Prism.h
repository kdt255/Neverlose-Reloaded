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

inline const char* PRISM_HLSL_SRC = R"(
    cbuffer PrismProps : register(b0) {
        float2 iResolution;
        float iTime;
        float uHeight;
    
        float uBaseHalf;
        int uUseBaseWobble;
        float uGlow;
        float uNoise;
    
        float2 uOffsetPx;
        float uSaturation;
        float uScale;
    
        float uHueShift;
        float uColorFreq;
        float uBloom;
        float uCenterShift;
    
        float uInvBaseHalf;
        float uInvHeight;
        float uMinAxis;
        float uPxScale;
    
        float uTimeScale;
        float3 padding;
    
        float4 uRot[3];
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

    float rand(float2 co){
        return frac(sin(dot(co, float2(12.9898f, 78.233f))) * 43758.5453123f);
    }

    float sdOctaAnisoInv(float3 p){
        float3 q = float3(abs(p.x) * uInvBaseHalf, abs(p.y) * uInvHeight, abs(p.z) * uInvBaseHalf);
        float m = q.x + q.y + q.z - 1.0f;
        return m * uMinAxis * 0.5773502691896258f;
    }

    float sdPyramidUpInv(float3 p){
        float oct = sdOctaAnisoInv(p);
        float halfSpace = -p.y;
        return max(oct, halfSpace);
    }

    float3x3 hueRotation(float a){
        float c = cos(a), s = sin(a);
        float3x3 W = float3x3(
            0.299f, 0.299f, 0.299f,
            0.587f, 0.587f, 0.587f,
            0.114f, 0.114f, 0.114f
        );
        float3x3 U = float3x3(
            0.701f, -0.299f, -0.300f,
            -0.587f, 0.413f, -0.588f,
            -0.114f, -0.114f, 0.886f
        );
        float3x3 V = float3x3(
            0.168f, 0.328f, -0.497f,
            -0.331f, 0.035f, 0.296f,
            0.500f, -0.500f, 0.201f
        );
        return W + U * c + V * s;
    }

    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float2 fragCoord = float2(input.pos.x, iResolution.y - input.pos.y);
        float2 f = (fragCoord - 0.5f * iResolution.xy - uOffsetPx) * uPxScale;

        float z = 5.0f;
        float d = 0.0f;

        float3 p;
        float4 o = float4(0.0f, 0.0f, 0.0f, 0.0f);

        float centerShift = uCenterShift;
        float cf = uColorFreq;

        const int STEPS = 100;
        for (int i = 0; i < STEPS; i++) {
            p = float3(f, z);
        
            if (uUseBaseWobble == 1) {
                float t = iTime * uTimeScale;
                float c0 = cos(t + 0.0f);
                float c1 = cos(t + 33.0f);
                float c2 = cos(t + 11.0f);
                float px = p.x * c0 + p.z * c1;
                float pz = p.x * c2 + p.z * c0;
                p.x = px;
                p.z = pz;
            }
        
            p = float3(dot(uRot[0].xyz, p), dot(uRot[1].xyz, p), dot(uRot[2].xyz, p));
        
            float3 q = p;
            q.y += centerShift;
            d = 0.1f + 0.2f * abs(sdPyramidUpInv(q));
            z -= d;
            o += (sin((p.y + z) * cf + float4(0.0f, 1.0f, 2.0f, 3.0f)) + 1.0f) / d;
        }

        o = tanh(o * o * (uGlow * uBloom) / 1e5f);

        float3 col = o.rgb;
        float n = rand(fragCoord + float2(iTime, iTime));
        col += (n - 0.5f) * uNoise;
        col = clamp(col, 0.0f, 1.0f);

        float L = dot(col, float3(0.2126f, 0.7152f, 0.0722f));
        col = clamp(lerp(float3(L, L, L), col, uSaturation), 0.0f, 1.0f);

        if(abs(uHueShift) > 0.0001f){
            col = clamp(mul(hueRotation(uHueShift), col), 0.0f, 1.0f);
        }

        return float4(col, o.a);
    }
    )";

    __declspec(align(16)) struct PrismProps {
        XMFLOAT2 iResolution;
        float iTime;
        float uHeight;
    
        float uBaseHalf;
        int uUseBaseWobble;
        float uGlow;
        float uNoise;
    
        XMFLOAT2 uOffsetPx;
        float uSaturation;
        float uScale;
    
        float uHueShift;
        float uColorFreq;
        float uBloom;
        float uCenterShift;
    
        float uInvBaseHalf;
        float uInvHeight;
        float uMinAxis;
        float uPxScale;
    
        float uTimeScale;
        XMFLOAT3 padding;
    
        XMFLOAT4 uRot[3];
    };

    class Prism {
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
    
        PrismProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
        
            D3DCompile(PRISM_HLSL_SRC, strlen(PRISM_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            D3DCompile(PRISM_HLSL_SRC, strlen(PRISM_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(PrismProps);
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

            float h = 3.5f;
            float baseWidth = 5.5f;
            float glow = 1.0f;
            float noise = 0.0f;
            float scale = 3.6f;
            float hueShift = 0.0f;
            float colorFrequency = 1.0f;
            float bloom = 1.0f;
            float timeScale = 0.5f;
        
            float BASE_HALF = baseWidth * 0.5f;

            props.iTime = timeAcc;
            props.uHeight = h;
            props.uBaseHalf = BASE_HALF;
            props.uUseBaseWobble = 1; 
            props.uGlow = glow;
            props.uNoise = noise;
            props.uOffsetPx = { 0.0f, 0.0f };
            props.uSaturation = 1.5f; 
            props.uScale = scale;
            props.uHueShift = hueShift;
            props.uColorFreq = colorFrequency;
            props.uBloom = bloom;
            props.uCenterShift = h * 0.25f;
            props.uInvBaseHalf = 1.0f / BASE_HALF;
            props.uInvHeight = 1.0f / h;
            props.uMinAxis = min(BASE_HALF, h);
            props.uPxScale = 1.0f / ((float)height * 0.1f * scale);
            props.uTimeScale = timeScale;

            props.uRot[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
            props.uRot[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
            props.uRot[2] = { 0.0f, 0.0f, 1.0f, 0.0f };

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
