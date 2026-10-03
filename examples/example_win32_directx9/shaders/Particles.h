#pragma once

#include "imgui.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <iostream>
#include <vector>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;

namespace shader {

using namespace DirectX;

inline const char* PARTICLES_HLSL_SRC = R"(
    cbuffer ParticlesProps : register(b0) {
        row_major matrix modelMatrix;
        row_major matrix viewMatrix;
        row_major matrix projectionMatrix;
    
        float uTime;
        float uSpread;
        float uBaseSize;
        float uSizeRandomness;
    
        float uAlphaParticles;
        float2 iResolution;
        float padding;
    };

    struct VS_INPUT {
        float3 position : POSITION;
        float4 random : TEXCOORD0;
        float3 color : COLOR;
    };

    struct GS_INPUT {
        float4 pos : POSITION;
        float size : PSIZE;
        float4 random : TEXCOORD0;
        float3 color : COLOR;
    };

    struct PS_INPUT {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;
        float4 random : TEXCOORD1;
        float3 color : COLOR;
    };

    GS_INPUT vs_main(VS_INPUT input) {
        GS_INPUT output;
    
        float3 pos = input.position * uSpread;
        pos.z *= 10.0f;
    
        float4 mPos = mul(float4(pos, 1.0f), modelMatrix);
        float t = uTime;
        mPos.x += sin(t * input.random.z + 6.28f * input.random.w) * lerp(0.1f, 1.5f, input.random.x);
        mPos.y += sin(t * input.random.y + 6.28f * input.random.x) * lerp(0.1f, 1.5f, input.random.w);
        mPos.z += sin(t * input.random.w + 6.28f * input.random.y) * lerp(0.1f, 1.5f, input.random.z);
    
        float4 mvPos = mul(mPos, viewMatrix);
    
        float size = 0.0f;
        if (uSizeRandomness == 0.0f) {
            size = uBaseSize;
        } else {
            size = (uBaseSize * (1.0f + uSizeRandomness * (input.random.x - 0.5f))) / length(mvPos.xyz);
        }
    
        output.pos = mul(mvPos, projectionMatrix);
        output.size = size;
        output.random = input.random;
        output.color = input.color;
    
        return output;
    }

    [maxvertexcount(4)]
    void gs_main(point GS_INPUT input[1], inout TriangleStream<PS_INPUT> spriteStream) {
        PS_INPUT output;
        output.random = input[0].random;
        output.color = input[0].color;
    
        float halfSize = input[0].size * 0.5f;
        float dx = halfSize / iResolution.x * 2.0f;
        float dy = halfSize / iResolution.y * 2.0f;
    
        float4 basePos = input[0].pos;
        float w = basePos.w;
    
        float2 offsets[4] = {
            float2(-1.0f,  1.0f),
            float2( 1.0f,  1.0f),
            float2(-1.0f, -1.0f),
            float2( 1.0f, -1.0f)
        };
    
        float2 uvs[4] = {
            float2(0.0f, 0.0f),
            float2(1.0f, 0.0f),
            float2(0.0f, 1.0f),
            float2(1.0f, 1.0f)
        };
    
        for (int i = 0; i < 4; i++) {
            output.pos = basePos;
            output.pos.x += offsets[i].x * dx * w;
            output.pos.y += offsets[i].y * dy * w;
            output.uv = uvs[i];
            spriteStream.Append(output);
        }
        spriteStream.RestartStrip();
    }

    float4 ps_main(PS_INPUT input) : SV_Target {
        float2 uv = input.uv;
        float d = length(uv - float2(0.5f, 0.5f));
    
        if (uAlphaParticles < 0.5f) {
            if (d > 0.5f) {
                discard;
            }
            float3 col = input.color + 0.2f * sin(uv.yxx + uTime + input.random.y * 6.28f);
            return float4(col, 1.0f);
        } else {
            float circle = smoothstep(0.5f, 0.4f, d) * 0.8f;
            float3 col = input.color + 0.2f * sin(uv.yxx + uTime + input.random.y * 6.28f);
            return float4(col, circle);
        }
    }
    )";

    __declspec(align(16)) struct ParticlesProps {
        XMMATRIX modelMatrix;
        XMMATRIX viewMatrix;
        XMMATRIX projectionMatrix;
    
        float uTime;
        float uSpread;
        float uBaseSize;
        float uSizeRandomness;
    
        float uAlphaParticles;
        XMFLOAT2 iResolution;
        float padding;
    };

    struct ParticleVertex {
        XMFLOAT3 position;
        XMFLOAT4 random;
        XMFLOAT3 color;
    };

    class Particles {
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
        float elapsed = 0;
        float rz = 0;
    
        ParticlesProps props = {};
        ID3D11Buffer* cbProps = nullptr;
        ID3D11Buffer* vertexBuffer = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11GeometryShader* gs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        ID3D11InputLayout* inputLayout = nullptr;
    
        bool initialized = false;
        ImVec2 smoothMouse = {0.0f, 0.0f};

        void Init() {
            if (!g_pd3dDevice) return;

            ID3DBlob* blob = nullptr;
            ID3DBlob* err = nullptr;
        
            D3DCompile(PARTICLES_HLSL_SRC, strlen(PARTICLES_HLSL_SRC), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
        
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
            };
            g_pd3dDevice->CreateInputLayout(layout, 3, blob->GetBufferPointer(), blob->GetBufferSize(), &inputLayout);
            blob->Release();

            D3DCompile(PARTICLES_HLSL_SRC, strlen(PARTICLES_HLSL_SRC), nullptr, nullptr, nullptr, "gs_main", "gs_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &gs);
            blob->Release();

            D3DCompile(PARTICLES_HLSL_SRC, strlen(PARTICLES_HLSL_SRC), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &blob, &err);
            if (err) { std::cout << (char*)err->GetBufferPointer() << std::endl; err->Release(); }
            g_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
            blob->Release();

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(ParticlesProps);
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            g_pd3dDevice->CreateBuffer(&bd, nullptr, &cbProps);

            const int count = 200;
            std::vector<ParticleVertex> vertices(count);
            for (int i = 0; i < count; i++) {
                float x, y, z, len;
                do {
                    x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
                    y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
                    z = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
                    len = x * x + y * y + z * z;
                } while (len > 1.0f || len == 0.0f);
            
                float r = pow((float)rand() / RAND_MAX, 1.0f / 3.0f);
                vertices[i].position = { x * r, y * r, z * r };
                vertices[i].random = { 
                    (float)rand() / RAND_MAX, 
                    (float)rand() / RAND_MAX, 
                    (float)rand() / RAND_MAX, 
                    (float)rand() / RAND_MAX 
                };
                vertices[i].color = { 1.0f, 1.0f, 1.0f }; // #ffffff
            }
        
            D3D11_BUFFER_DESC vbd = {};
            vbd.Usage = D3D11_USAGE_IMMUTABLE;
            vbd.ByteWidth = sizeof(ParticleVertex) * count;
            vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = vertices.data();
            g_pd3dDevice->CreateBuffer(&vbd, &initData, &vertexBuffer);

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

            float speed = 0.1f;
            float delta = dt_sec * 1000.0f;
            elapsed += delta * speed;
        
            props.uTime = elapsed * 0.001f;
            props.uSpread = 10.0f;
            props.uBaseSize = 100.0f;
            props.uSizeRandomness = 1.0f;
            props.uAlphaParticles = 0.0f;
        
            ImVec2 mousePos = ImGui::GetMousePos();
            float relX = (mousePos.x - winPos.x) / winSize.x;
            float relY = (mousePos.y - winPos.y) / winSize.y;
        
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                smoothMouse.x = relX * 2.0f - 1.0f;
                smoothMouse.y = -(relY * 2.0f - 1.0f);
            } else {
                smoothMouse.x = 0;
                smoothMouse.y = 0;
            }

            float px = -smoothMouse.x * 1.0f;
            float py = -smoothMouse.y * 1.0f;
        
            float rx = sin(elapsed * 0.0002f) * 0.1f;
            float ry = cos(elapsed * 0.0005f) * 0.15f;
            rz += 0.01f * speed * (delta / 16.666f);
        
            XMMATRIX rot = XMMatrixRotationRollPitchYaw(rx, ry, rz);
            XMMATRIX trans = XMMatrixTranslation(px, py, 0.0f);
            props.modelMatrix = XMMatrixMultiply(rot, trans);
        
            props.viewMatrix = XMMatrixLookAtRH(XMVectorSet(0, 0, 20, 1), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
            props.projectionMatrix = XMMatrixPerspectiveFovRH(15.0f * XM_PI / 180.0f, (float)width / (float)height, 0.1f, 100.0f);

            g_pd3dDeviceContext->UpdateSubresource(cbProps, 0, nullptr, &props, 0, 0);

            g_pd3dDeviceContext->OMSetRenderTargets(1, &outputFBO.rtv, nullptr);
            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            g_pd3dDeviceContext->ClearRenderTargetView(outputFBO.rtv, clearColor);
        
            D3D11_VIEWPORT vp = {};
            vp.Width = (float)width;
            vp.Height = (float)height;
            vp.MaxDepth = 1.0f;
            g_pd3dDeviceContext->RSSetViewports(1, &vp);

            UINT stride = sizeof(ParticleVertex);
            UINT offset = 0;
            g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
            g_pd3dDeviceContext->IASetInputLayout(inputLayout);
        
            g_pd3dDeviceContext->VSSetShader(vs, nullptr, 0);
            g_pd3dDeviceContext->GSSetShader(gs, nullptr, 0);
            g_pd3dDeviceContext->PSSetShader(ps, nullptr, 0);
        
            g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &cbProps);
            g_pd3dDeviceContext->GSSetConstantBuffers(0, 1, &cbProps);
            g_pd3dDeviceContext->PSSetConstantBuffers(0, 1, &cbProps);

            g_pd3dDeviceContext->Draw(200, 0);
        
            g_pd3dDeviceContext->GSSetShader(nullptr, nullptr, 0);
        }
    };
}
