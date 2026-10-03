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

inline const char* RAYS_HLSL_SRC = R"(
    cbuffer RayProps : register(b0) {
        float iTime;
        float raysSpeed;
        float lightSpread;
        float rayLength;
    
        float2 iResolution;
        float pulsating;
        float fadeDistance;
    
        float2 rayPos;
        float2 rayDir;
    
        float3 raysColor;
        float saturation;
    
        float2 mousePos;
        float mouseInfluence;
        float noiseAmount;
    
        float distortion;
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

    float noise(float2 st) {
        return frac(sin(dot(st.xy, float2(12.9898f, 78.233f))) * 43758.5453123f);
    }

    float rayStrength(float2 raySource, float2 rayRefDirection, float2 coord, float seedA, float seedB, float speed) {
        float2 sourceToCoord = coord - raySource;
        float2 dirNorm = normalize(sourceToCoord);
        float cosAngle = dot(dirNorm, rayRefDirection);

        float distortedAngle = cosAngle + distortion * sin(iTime * 2.0f + length(sourceToCoord) * 0.01f) * 0.2f;
        float spreadFactor = pow(max(distortedAngle, 0.0f), 1.0f / max(lightSpread, 0.001f));

        float distance = length(sourceToCoord);
        float maxDistance = iResolution.x * rayLength;
        float lengthFalloff = clamp((maxDistance - distance) / maxDistance, 0.0f, 1.0f);

        float fadeFalloff = clamp((iResolution.x * fadeDistance - distance) / max(1.0f, iResolution.x * fadeDistance), 0.5f, 1.0f);
        float pulse = pulsating > 0.5f ? (0.8f + 0.2f * sin(iTime * speed * 3.0f)) : 1.0f;

        float baseStrength = clamp(
            (0.45f + 0.15f * sin(distortedAngle * seedA + iTime * speed)) +
            (0.3f + 0.2f * cos(-distortedAngle * seedB + iTime * speed)),
            0.0f, 1.0f
        );

        return baseStrength * lengthFalloff * fadeFalloff * spreadFactor * pulse;
    }

    float4 ps_main(VS_OUTPUT input) : SV_Target {
        float2 coord = input.pos.xy;
    
        float2 finalRayDir = rayDir;
        if (mouseInfluence > 0.0f) {
            float2 mouseScreenPos = mousePos * iResolution.xy;
            float2 mouseDirection = normalize(mouseScreenPos - rayPos);
            finalRayDir = normalize(lerp(rayDir, mouseDirection, mouseInfluence));
        }

        float rays1 = rayStrength(rayPos, finalRayDir, coord, 36.2214f, 21.11349f, 1.5f * raysSpeed);
        float rays2 = rayStrength(rayPos, finalRayDir, coord, 22.3991f, 18.0234f, 1.1f * raysSpeed);

        float4 fragColor = float4(1.0f, 1.0f, 1.0f, 1.0f) * (rays1 * 0.5f + rays2 * 0.4f);

        if (noiseAmount > 0.0f) {
            float n = noise(coord * 0.01f + iTime * 0.1f);
            fragColor.rgb *= (1.0f - noiseAmount + noiseAmount * n);
        }

        float brightness = 1.0f - (coord.y / iResolution.y);
        fragColor.r *= 0.1f + brightness * 0.8f;
        fragColor.g *= 0.3f + brightness * 0.6f;
        fragColor.b *= 0.5f + brightness * 0.5f;

        if (saturation != 1.0f) {
            float gray = dot(fragColor.rgb, float3(0.299f, 0.587f, 0.114f));
            fragColor.rgb = lerp(float3(gray, gray, gray), fragColor.rgb, saturation);
        }

        fragColor.rgb *= raysColor;
        return fragColor;
    }
    )";

    __declspec(align(16)) struct RayProps {
        float iTime;
        float raysSpeed;
        float lightSpread;
        float rayLength;

        XMFLOAT2 iResolution;
        float pulsating;
        float fadeDistance;

        XMFLOAT2 rayPos;
        XMFLOAT2 rayDir;

        XMFLOAT3 raysColor;
        float saturation;

        XMFLOAT2 mousePos;
        float mouseInfluence;
        float noiseAmount;

        float distortion;
        XMFLOAT3 padding;
    };

    class LightRays {
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
    
        RayProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool initialized = false;
    
        ImVec2 smoothMouse = {0.5f, 0.5f};

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
            D3DCompile(RAYS_HLSL_SRC, strlen(RAYS_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            D3DCompile(RAYS_HLSL_SRC, strlen(RAYS_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(RayProps);
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

            float speed = 1.0f;
            timeAcc += dt_sec * speed;

            props.iTime = timeAcc;
            props.raysColor = { 1.0f, 1.0f, 1.0f }; // #ffffff
            props.raysSpeed = 1.0f;
            props.lightSpread = 0.5f;
            props.rayLength = 3.0f;
            props.pulsating = 0.0f;
            props.fadeDistance = 1.0f;
            props.saturation = 1.0f;
            props.mouseInfluence = 0.1f;
            props.noiseAmount = 0.0f;
            props.distortion = 0.0f;

            float outside = 0.2f;
            props.rayPos = { 0.5f * width, -outside * height };
            props.rayDir = { 0.0f, 1.0f };

            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = (mousePos.x - winPos.x) / winSize.x;
            float relY = (mousePos.y - winPos.y) / winSize.y;
        
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                float smoothing = 0.92f;
                smoothMouse.x = smoothMouse.x * smoothing + relX * (1.0f - smoothing);
                smoothMouse.y = smoothMouse.y * smoothing + relY * (1.0f - smoothing);
            }
        
            props.mousePos = { smoothMouse.x, smoothMouse.y };

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
