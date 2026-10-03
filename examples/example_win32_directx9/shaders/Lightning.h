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

inline const char* LIGHTNING_HLSL_SRC = R"(
    cbuffer LightningProps : register(b0) {
        float2 iResolution;
        float iTime;
        float uHue;
        
        float uXOffset;
        float uSpeed;
        float uIntensity;
        float uSize;
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
    
    float3 hsv2rgb(float3 c) {
        float3 rgb = clamp(abs(fmod(c.x * 6.0f + float3(0.0f, 4.0f, 2.0f), 6.0f) - 3.0f) - 1.0f, 0.0f, 1.0f);
        return c.z * lerp(float3(1.0f, 1.0f, 1.0f), rgb, c.y);
    }
    
    float hash11(float p) {
        p = frac(p * 0.1031f);
        p *= p + 33.33f;
        p *= p + p;
        return frac(p);
    }
    
    float hash12(float2 p) {
        float3 p3 = frac(float3(p.x, p.y, p.x) * 0.1031f);
        p3 += dot(p3, float3(p3.y, p3.z, p3.x) + 33.33f);
        return frac((p3.x + p3.y) * p3.z);
    }
    
    float2 rotate(float2 v, float a) {
        float s = sin(a);
        float c = cos(a);
        return float2(v.x * c - v.y * s, v.x * s + v.y * c);
    }
    
    float noise(float2 p) {
        float2 ip = floor(p);
        float2 fp = frac(p);
        float a = hash12(ip);
        float b = hash12(ip + float2(1.0f, 0.0f));
        float c = hash12(ip + float2(0.0f, 1.0f));
        float d = hash12(ip + float2(1.0f, 1.0f));
        
        float2 t = smoothstep(0.0f, 1.0f, fp);
        return lerp(lerp(a, b, t.x), lerp(c, d, t.x), t.y);
    }
    
    float fbm(float2 p) {
        float value = 0.0f;
        float amplitude = 0.5f;
        for (int i = 0; i < 10; ++i) {
            value += amplitude * noise(p);
            p = rotate(p, 0.45f);
            p *= 2.0f;
            amplitude *= 0.5f;
        }
        return value;
    }
    
    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float2 fragCoord = float2(input.pos.x, iResolution.y - input.pos.y);
        float2 uv = fragCoord / iResolution.xy;
        uv = 2.0f * uv - 1.0f;
        uv.x *= iResolution.x / iResolution.y;
        uv.x += uXOffset;
        
        uv += 2.0f * fbm(uv * uSize + 0.8f * iTime * uSpeed) - 1.0f;
        
        float dist = abs(uv.x);
        float3 baseColor = hsv2rgb(float3(uHue / 360.0f, 0.7f, 0.8f));
        float3 col = baseColor * pow(lerp(0.0f, 0.07f, hash11(iTime * uSpeed)) / dist, 1.0f) * uIntensity;
        float a = clamp(max(col.r, max(col.g, col.b)), 0.0f, 1.0f);
        return float4(col, a);
    }
    )";
    
    __declspec(align(16)) struct LightningProps {
        XMFLOAT2 iResolution;
        float iTime;
        float uHue;
        
        float uXOffset;
        float uSpeed;
        float uIntensity;
        float uSize;
    };
    
    class Lightning {
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
        
        LightningProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;
    
        void Init() {
            if (!g_pd3dDevice) return;
    
            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
            
            D3DCompile(LIGHTNING_HLSL_SRC, strlen(LIGHTNING_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();
    
            D3DCompile(LIGHTNING_HLSL_SRC, strlen(LIGHTNING_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();
    
            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(LightningProps);
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
            props.uHue = 260.0f;
            props.uXOffset = 0.0f;
            props.uSpeed = 1.0f;
            props.uIntensity = 1.0f;
            props.uSize = 1.0f;
    
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
