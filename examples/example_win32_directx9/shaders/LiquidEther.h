#pragma once

#include "imgui.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <chrono>
#include <random>
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

namespace shader {

using namespace DirectX;

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

inline const char* HLSL_SRC = R"(
    cbuffer SimProps : register(b0) {
        float2 px;
        float2 boundarySpace;
        float2 fboSize;
        float dt;
        float isBFECC;
        float2 force;
        float2 center;
        float2 scale;
        float v;
        float padding;
        float4 bgColor;
    };

    SamplerState samplerLinear : register(s0);

    Texture2D tex0 : register(t0);
    Texture2D tex1 : register(t1);
    Texture2D tex2 : register(t2);

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

    float4 ps_advection(VS_OUTPUT input) : SV_Target {
        float2 ratio = max(fboSize.x, fboSize.y) / fboSize;
        if (isBFECC == 0.0f) {
            float2 vel = tex0.SampleLevel(samplerLinear, input.uv, 0).xy;
            float2 uv2 = input.uv - vel * dt * ratio;
            float2 newVel = tex0.SampleLevel(samplerLinear, uv2, 0).xy;
            return float4(newVel, 0.0f, 0.0f);
        } else {
            float2 spot_new = input.uv;
            float2 vel_old = tex0.SampleLevel(samplerLinear, input.uv, 0).xy;
            float2 spot_old = spot_new - vel_old * dt * ratio;
            float2 vel_new1 = tex0.SampleLevel(samplerLinear, spot_old, 0).xy;
            float2 spot_new2 = spot_old + vel_new1 * dt * ratio;
            float2 error = spot_new2 - spot_new;
            float2 spot_new3 = spot_new - error / 2.0f;
            float2 vel_2 = tex0.SampleLevel(samplerLinear, spot_new3, 0).xy;
            float2 spot_old2 = spot_new3 - vel_2 * dt * ratio;
            float2 newVel2 = tex0.SampleLevel(samplerLinear, spot_old2, 0).xy;
            return float4(newVel2, 0.0f, 0.0f);
        }
    }

    float4 ps_externalForce(VS_OUTPUT input) : SV_Target {
        float2 clipSpace = input.uv * 2.0f - 1.0f;
        clipSpace.y = -clipSpace.y;
        float2 position_xy = (clipSpace - center) / (scale * 2.0f * px);
        float2 circle = position_xy * 2.0f;
        float len = length(circle);
        if (len > 1.0f) discard;
        float d = 1.0f - len;
        d *= d;
        return float4(force * d, 0.0f, 1.0f);
    }

    float4 ps_viscous(VS_OUTPUT input) : SV_Target {
        float2 old = tex0.SampleLevel(samplerLinear, input.uv, 0).xy;
        float2 new0 = tex1.SampleLevel(samplerLinear, input.uv + float2(px.x * 2.0f, 0.0f), 0).xy;
        float2 new1 = tex1.SampleLevel(samplerLinear, input.uv - float2(px.x * 2.0f, 0.0f), 0).xy;
        float2 new2 = tex1.SampleLevel(samplerLinear, input.uv + float2(0.0f, px.y * 2.0f), 0).xy;
        float2 new3 = tex1.SampleLevel(samplerLinear, input.uv - float2(0.0f, px.y * 2.0f), 0).xy;
        float2 newv = 4.0f * old + v * dt * (new0 + new1 + new2 + new3);
        newv /= 4.0f * (1.0f + v * dt);
        return float4(newv, 0.0f, 0.0f);
    }

    float4 ps_divergence(VS_OUTPUT input) : SV_Target {
        float x0 = tex0.SampleLevel(samplerLinear, input.uv - float2(px.x, 0.0f), 0).x;
        float x1 = tex0.SampleLevel(samplerLinear, input.uv + float2(px.x, 0.0f), 0).x;
        float y0 = tex0.SampleLevel(samplerLinear, input.uv - float2(0.0f, px.y), 0).y;
        float y1 = tex0.SampleLevel(samplerLinear, input.uv + float2(0.0f, px.y), 0).y;
        float divergence = (x1 - x0 + y1 - y0) / 2.0f;
        return float4(divergence / dt, 0.0f, 0.0f, 0.0f);
    }

    float4 ps_poisson(VS_OUTPUT input) : SV_Target {
        float p0 = tex1.SampleLevel(samplerLinear, input.uv + float2(px.x * 2.0f, 0.0f), 0).r;
        float p1 = tex1.SampleLevel(samplerLinear, input.uv - float2(px.x * 2.0f, 0.0f), 0).r;
        float p2 = tex1.SampleLevel(samplerLinear, input.uv + float2(0.0f, px.y * 2.0f), 0).r;
        float p3 = tex1.SampleLevel(samplerLinear, input.uv - float2(0.0f, px.y * 2.0f), 0).r;
        float div = tex0.SampleLevel(samplerLinear, input.uv, 0).r;
        float newP = (p0 + p1 + p2 + p3) / 4.0f - div;
        return float4(newP, 0.0f, 0.0f, 0.0f);
    }

    float4 ps_pressure(VS_OUTPUT input) : SV_Target {
        float p0 = tex1.SampleLevel(samplerLinear, input.uv + float2(px.x, 0.0f), 0).r;
        float p1 = tex1.SampleLevel(samplerLinear, input.uv - float2(px.x, 0.0f), 0).r;
        float p2 = tex1.SampleLevel(samplerLinear, input.uv + float2(0.0f, px.y), 0).r;
        float p3 = tex1.SampleLevel(samplerLinear, input.uv - float2(0.0f, px.y), 0).r;
        float2 vel = tex0.SampleLevel(samplerLinear, input.uv, 0).xy;
        float2 gradP = float2(p0 - p1, p2 - p3) * 0.5f;
        vel = vel - gradP * dt;
        return float4(vel, 0.0f, 1.0f);
    }

    float4 ps_color(VS_OUTPUT input) : SV_Target {
        float2 vel = tex0.SampleLevel(samplerLinear, input.uv, 0).xy;
        float lenv = clamp(length(vel), 0.0f, 1.0f);
        float3 c = tex1.SampleLevel(samplerLinear, float2(lenv, 0.5f), 0).rgb;
    
        // Aggressively cut down the faint glow/shine around the edges
        float mixFactor = smoothstep(0.05f, 1.0f, lenv);
    
        float3 outRGB = lerp(bgColor.rgb, c, mixFactor);
        float outA = lerp(bgColor.a, 1.0f, mixFactor);
        return float4(outRGB, outA);
    }
    )";

    __declspec(align(16)) struct SimProps {
        XMFLOAT2 px;
        XMFLOAT2 boundarySpace;
        XMFLOAT2 fboSize;
        float dt;
        float isBFECC;
        XMFLOAT2 force;
        XMFLOAT2 center;
        XMFLOAT2 scale;
        float v;
        float padding;
        XMFLOAT4 bgColor;
    };

    class LiquidEther {
    public:
        int width = 0;
        int height = 0;
        float resolution = 0.5f;

        FBO vel_0, vel_1, vel_viscous0, vel_viscous1, div, pressure_0, pressure_1, outputFBO;
        ID3D11Texture2D* paletteTex = nullptr;
        ID3D11ShaderResourceView* paletteSRV = nullptr;

        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps_adv = nullptr;
        ID3D11PixelShader* ps_ext = nullptr;
        ID3D11PixelShader* ps_vis = nullptr;
        ID3D11PixelShader* ps_div = nullptr;
        ID3D11PixelShader* ps_poi = nullptr;
        ID3D11PixelShader* ps_pre = nullptr;
        ID3D11PixelShader* ps_col = nullptr;

        ID3D11Buffer* cbSimProps = nullptr;
        ID3D11SamplerState* samplerLinear = nullptr;
        ID3D11BlendState* blendAdditive = nullptr;
        ID3D11BlendState* blendOpaque = nullptr;

        bool initialized = false;
        SimProps props = {};

        void Init() {
            if (initialized) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
            D3DCompile(HLSL_SRC, strlen(HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) err->Release();
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
            blob->Release();

            auto CompilePS = [&](const char* ep, ID3D11PixelShader** ps) {
                ID3DBlob* b = nullptr;
                ID3DBlob* e = nullptr;
                D3DCompile(HLSL_SRC, strlen(HLSL_SRC), nullptr, nullptr, nullptr, ep, "ps_5_0", 0, 0, &b, &e);
                if (e) {
                    std::cout << (char*)e->GetBufferPointer() << std::endl;
                    e->Release();
                }
                g_pd3dDevice->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, ps);
                b->Release();
            };

            CompilePS("ps_advection", &ps_adv);
            CompilePS("ps_externalForce", &ps_ext);
            CompilePS("ps_viscous", &ps_vis);
            CompilePS("ps_divergence", &ps_div);
            CompilePS("ps_poisson", &ps_poi);
            CompilePS("ps_pressure", &ps_pre);
            CompilePS("ps_color", &ps_col);

            D3D11_BUFFER_DESC cbDesc = {};
            cbDesc.ByteWidth = sizeof(SimProps);
            cbDesc.Usage = D3D11_USAGE_DYNAMIC;
            cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            g_pd3dDevice->CreateBuffer(&cbDesc, nullptr, &cbSimProps);

            D3D11_SAMPLER_DESC sampDesc = {};
            sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            g_pd3dDevice->CreateSamplerState(&sampDesc, &samplerLinear);

            D3D11_BLEND_DESC blendDesc = {};
            blendDesc.RenderTarget[0].BlendEnable = TRUE;
            blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
            blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            g_pd3dDevice->CreateBlendState(&blendDesc, &blendAdditive);

            blendDesc.RenderTarget[0].BlendEnable = FALSE;
            g_pd3dDevice->CreateBlendState(&blendDesc, &blendOpaque);

            uint8_t pal[12] = { 0x52, 0x27, 0xFF, 0xFF, 0xFF, 0x9F, 0xFC, 0xFF, 0xB4, 0x97, 0xCF, 0xFF };
            D3D11_TEXTURE2D_DESC texDesc = {};
            texDesc.Width = 3;
            texDesc.Height = 1;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = pal;
            initData.SysMemPitch = 12;
            g_pd3dDevice->CreateTexture2D(&texDesc, &initData, &paletteTex);
            g_pd3dDevice->CreateShaderResourceView(paletteTex, nullptr, &paletteSRV);

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
            int fw = max(1, (int)(w * resolution));
            int fh = max(1, (int)(h * resolution));

            vel_0.Release(); vel_1.Release();
            vel_viscous0.Release(); vel_viscous1.Release();
            div.Release(); pressure_0.Release(); pressure_1.Release();
            outputFBO.Release();

            vel_0 = CreateFBO(fw, fh, DXGI_FORMAT_R16G16B16A16_FLOAT);
            vel_1 = CreateFBO(fw, fh, DXGI_FORMAT_R16G16B16A16_FLOAT);
            vel_viscous0 = CreateFBO(fw, fh, DXGI_FORMAT_R16G16B16A16_FLOAT);
            vel_viscous1 = CreateFBO(fw, fh, DXGI_FORMAT_R16G16B16A16_FLOAT);
            div = CreateFBO(fw, fh, DXGI_FORMAT_R16G16B16A16_FLOAT);
            pressure_0 = CreateFBO(fw, fh, DXGI_FORMAT_R16G16B16A16_FLOAT);
            pressure_1 = CreateFBO(fw, fh, DXGI_FORMAT_R16G16B16A16_FLOAT);
            outputFBO = CreateFBO(w, h, DXGI_FORMAT_R8G8B8A8_UNORM);

            props.fboSize = { (float)fw, (float)fh };
            props.px = { 1.0f / fw, 1.0f / fh };
        }

        void BindFBO(FBO& fbo) {
            g_pd3dDeviceContext->OMSetRenderTargets(1, &fbo.rtv, nullptr);
        }

        void DrawQuad() {
            g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_pd3dDeviceContext->Draw(3, 0);
        }

        void UpdateProps() {
            D3D11_MAPPED_SUBRESOURCE mapped;
            g_pd3dDeviceContext->Map(cbSimProps, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            memcpy(mapped.pData, &props, sizeof(SimProps));
            g_pd3dDeviceContext->Unmap(cbSimProps, 0);
            g_pd3dDeviceContext->PSSetConstantBuffers(0, 1, &cbSimProps);
        }

        void Update(float dt_sec, ImVec2 winSize, ImVec2 winPos) {
            if (!initialized) Init();
        
            if (winSize.x <= 0 || winSize.y <= 0) return;
            Resize((int)winSize.x, (int)winSize.y);

            static auto lastInteractionTime = std::chrono::steady_clock::now();
            static XMFLOAT2 coords = {0,0};
            static XMFLOAT2 coords_old = {0,0};
            static XMFLOAT2 diff = {0,0};
            static XMFLOAT2 autoCurrent = {0,0};
            static XMFLOAT2 autoTarget = {0,0};
            static bool autoActive = false;
        
            auto now = std::chrono::steady_clock::now();
            float idleTime = std::chrono::duration<float>(now - lastInteractionTime).count();

            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = mousePos.x - winPos.x;
            float relY = mousePos.y - winPos.y;
        
            static ImVec2 lastMousePos = ImVec2(relX, relY);
            static ImVec2 lastWinPos = winPos;
            bool winMoved = (winPos.x != lastWinPos.x || winPos.y != lastWinPos.y);
            lastWinPos = winPos;

            bool isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            bool mouseMoved = (relX != lastMousePos.x || relY != lastMousePos.y);
            lastMousePos = ImVec2(relX, relY);

            static bool wasHovered = false;
            if (isHovered && !wasHovered) {
                coords_old = { (relX / winSize.x) * 2.0f - 1.0f, -((relY / winSize.y) * 2.0f - 1.0f) };
            }
            wasHovered = isHovered;

            if (isHovered && mouseMoved && !winMoved) {
                lastInteractionTime = now;
                if (autoActive) {
                    autoActive = false;
                    coords_old = { (relX / winSize.x) * 2.0f - 1.0f, -((relY / winSize.y) * 2.0f - 1.0f) };
                }
                coords = { (relX / winSize.x) * 2.0f - 1.0f, -((relY / winSize.y) * 2.0f - 1.0f) };
                diff = {coords.x - coords_old.x, coords.y - coords_old.y};
                coords_old = coords;
            } else if (!autoActive) {
                diff = {0, 0};
            }

            if (idleTime > 3.0f) {
                if (!autoActive) {
                    autoActive = true;
                    autoCurrent = {0.0f, 0.0f};
                    autoTarget = { ((float)rand() / RAND_MAX * 2 - 1) * 0.8f, ((float)rand() / RAND_MAX * 2 - 1) * 0.8f };
                    coords_old = autoCurrent;
                }
                XMFLOAT2 dir = {autoTarget.x - autoCurrent.x, autoTarget.y - autoCurrent.y};
                float dist = sqrt(dir.x*dir.x + dir.y*dir.y);
                if (dist < 0.01f) {
                    autoTarget = { ((float)rand() / RAND_MAX * 2 - 1) * 0.8f, ((float)rand() / RAND_MAX * 2 - 1) * 0.8f };
                } else {
                    float move = min(0.1f * dt_sec, dist); 
                    autoCurrent.x += (dir.x / dist) * move;
                    autoCurrent.y += (dir.y / dist) * move;
                    coords = autoCurrent;
                }
                diff = {coords.x - coords_old.x, coords.y - coords_old.y};
                coords_old = coords;
            }

            if (autoActive) { diff.x *= 0.8f; diff.y *= 0.8f; } 

            float diffLen = sqrt(diff.x*diff.x + diff.y*diff.y);
            if (diffLen > 0.05f) {
                diff.x = (diff.x / diffLen) * 0.05f;
                diff.y = (diff.y / diffLen) * 0.05f;
            }

            float timeScale = max(0.1f, min(dt_sec * 60.0f, 5.0f));
            props.dt = 0.014f * timeScale;
            props.isBFECC = 1.0f;
            props.boundarySpace = {0, 0};
            props.bgColor = {0.06f, 0.06f, 0.06f, 1.0f};
        
            ID3D11ShaderResourceView* nullSRV[3] = { nullptr, nullptr, nullptr };

            D3D11_VIEWPORT vp = {};
            vp.Width = props.fboSize.x;
            vp.Height = props.fboSize.y;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            g_pd3dDeviceContext->RSSetViewports(1, &vp);

            UpdateProps();
            BindFBO(vel_1);
            g_pd3dDeviceContext->VSSetShader(vs, nullptr, 0);
            g_pd3dDeviceContext->PSSetShader(ps_adv, nullptr, 0);
            g_pd3dDeviceContext->PSSetSamplers(0, 1, &samplerLinear);
            g_pd3dDeviceContext->PSSetShaderResources(0, 1, &vel_0.srv);
            DrawQuad();
            g_pd3dDeviceContext->PSSetShaderResources(0, 3, nullSRV);

            float cursor_size = autoActive ? 30.0f : 100.0f;
            float forceScale = 20.0f;
            props.force = { (diff.x / 2.0f) * forceScale, (diff.y / 2.0f) * forceScale };
            props.scale = { cursor_size, cursor_size };
        
            float cursorSizeX = cursor_size * props.px.x;
            float cursorSizeY = cursor_size * props.px.y;
            float cx = max(-1.0f + cursorSizeX + props.px.x * 2, min(coords.x, 1.0f - cursorSizeX - props.px.x * 2));
            float cy = max(-1.0f + cursorSizeY + props.px.y * 2, min(coords.y, 1.0f - cursorSizeY - props.px.y * 2));
            props.center = { cx, cy };

            UpdateProps();
            g_pd3dDeviceContext->OMSetBlendState(blendAdditive, nullptr, 0xFFFFFFFF);
            g_pd3dDeviceContext->PSSetShader(ps_ext, nullptr, 0);
            DrawQuad();
            g_pd3dDeviceContext->OMSetBlendState(blendOpaque, nullptr, 0xFFFFFFFF);

            FBO* curVel = &vel_1;
            FBO* fbo_out = nullptr;
            props.v = 30.0f;
            for (int i = 0; i < 32; i++) {
                FBO* fbo_in = (i % 2 == 0) ? &vel_viscous0 : &vel_viscous1;
                fbo_out = (i % 2 == 0) ? &vel_viscous1 : &vel_viscous0;
                UpdateProps();
                BindFBO(*fbo_out);
                g_pd3dDeviceContext->PSSetShader(ps_vis, nullptr, 0);
                g_pd3dDeviceContext->PSSetShaderResources(0, 1, &curVel->srv);
                g_pd3dDeviceContext->PSSetShaderResources(1, 1, &fbo_in->srv);
                DrawQuad();
                g_pd3dDeviceContext->PSSetShaderResources(0, 3, nullSRV);
            }
            curVel = fbo_out;

            UpdateProps();
            BindFBO(div);
            g_pd3dDeviceContext->PSSetShader(ps_div, nullptr, 0);
            g_pd3dDeviceContext->PSSetShaderResources(0, 1, &curVel->srv);
            DrawQuad();
            g_pd3dDeviceContext->PSSetShaderResources(0, 3, nullSRV);

            FBO* curP = &pressure_0;
            FBO* p_out = nullptr;
            for (int i = 0; i < 32; i++) {
                FBO* p_in = (i % 2 == 0) ? &pressure_0 : &pressure_1;
                p_out = (i % 2 == 0) ? &pressure_1 : &pressure_0;
                BindFBO(*p_out);
                g_pd3dDeviceContext->PSSetShader(ps_poi, nullptr, 0);
                g_pd3dDeviceContext->PSSetShaderResources(0, 1, &div.srv);
                g_pd3dDeviceContext->PSSetShaderResources(1, 1, &p_in->srv);
                DrawQuad();
                g_pd3dDeviceContext->PSSetShaderResources(0, 3, nullSRV);
            }
            curP = p_out;

            BindFBO(vel_0);
            g_pd3dDeviceContext->PSSetShader(ps_pre, nullptr, 0);
            g_pd3dDeviceContext->PSSetShaderResources(0, 1, &curVel->srv);
            g_pd3dDeviceContext->PSSetShaderResources(1, 1, &curP->srv);
            DrawQuad();
            g_pd3dDeviceContext->PSSetShaderResources(0, 3, nullSRV);

            vp.Width = (float)width;
            vp.Height = (float)height;
            g_pd3dDeviceContext->RSSetViewports(1, &vp);

            BindFBO(outputFBO);
            g_pd3dDeviceContext->PSSetShader(ps_col, nullptr, 0);
            g_pd3dDeviceContext->PSSetShaderResources(0, 1, &vel_0.srv);
            g_pd3dDeviceContext->PSSetShaderResources(1, 1, &paletteSRV);
            DrawQuad();
            g_pd3dDeviceContext->PSSetShaderResources(0, 3, nullSRV);

            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        }
    };
}
