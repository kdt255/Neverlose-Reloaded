// blur.cpp - Direct3D 11 back-buffer blur.
//
// The back buffer is copied into a full-resolution capture, and a separable 5-tap linear-sampled
// gaussian then ping-pongs between two HALF-resolution targets - the first pass doubles as the
// downsample. Half resolution is four times fewer pixels per pass and doubles each pass's reach in
// screen space, so `k_iterations` passes here cover what 4x as many cost at full resolution; the
// result is only ever magnified back up, where the missing detail is the point.
//
// Compositing is left to ImGui (AddImageRounded on the result's SRV) so the rounded corners get
// the draw list's own antialiasing instead of a hand-tessellated fan.

#include "blur.hpp"

#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <deque>

namespace
{
    using Microsoft::WRL::ComPtr;

    struct Request
    {
        float   alpha;
        float   rounding;
        ImVec4  rect;
        ImColor tint;
    };

    __declspec( align( 16 ) ) struct Params
    {
        float dir[ 2 ];
        float pad[ 2 ];
    };

    // Four at half resolution lands on roughly the same radius the D3D9 build reached with eight
    // at full resolution, for an eighth of the fill.
    constexpr int k_iterations = 4;

    // One pixel shader for both axes: the direction arrives as a texel-sized step, so a pass is
    // horizontal or vertical purely by what is in the constant buffer.
    constexpr char k_hlsl[] = R"HLSL(
cbuffer Params : register(b0) { float2 uDir; float2 uPad; };

Texture2D    tex0  : register(t0);
SamplerState samp0 : register(s0);

struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VS_OUT vs_main(uint id : SV_VertexID) {
    VS_OUT o;
    o.uv  = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return o;
}

float4 ps_main(VS_OUT i) : SV_Target {
    const float w[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };
    const float o[3] = { 0.0f, 1.3846153846f, 3.2307692308f };

    float3 c = tex0.Sample(samp0, i.uv).rgb * w[0];
    [unroll] for (int n = 1; n < 3; ++n) {
        c += tex0.Sample(samp0, i.uv + uDir * o[n]).rgb * w[n];
        c += tex0.Sample(samp0, i.uv - uDir * o[n]).rgb * w[n];
    }
    // Opaque on purpose: the composite takes its alpha from the draw list's vertex color, the same
    // way the D3D9 build selected DIFFUSE for the alpha stage.
    return float4(c, 1.0f);
}
)HLSL";

    struct Resources
    {
        HRESULT last_result = S_FALSE;

        ComPtr<ID3D11Texture2D>          capture;  // full resolution, the CopyResource destination
        ComPtr<ID3D11ShaderResourceView> scapture;
        ComPtr<ID3D11Texture2D>          a, b;     // half resolution, the ping-pong pair
        ComPtr<ID3D11ShaderResourceView> sa, sb;
        ComPtr<ID3D11RenderTargetView>   ra, rb;

        ComPtr<ID3D11VertexShader>      vs;
        ComPtr<ID3D11PixelShader>       ps;
        ComPtr<ID3D11Buffer>            cb;
        ComPtr<ID3D11SamplerState>      sampler;
        ComPtr<ID3D11BlendState>        blend_off;
        ComPtr<ID3D11DepthStencilState> depth_off;
        ComPtr<ID3D11RasterizerState>   raster;

        UINT        width = 0, height = 0;   // back buffer, and the basis for the composite's UVs
        UINT        bw = 0, bh = 0;          // blur targets
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

        // Requests have to outlive the draw list they were recorded into, and that list is not
        // rendered until the end of the frame - so they are kept for the frame and dropped on the
        // next one. A deque, because a vector would invalidate the pointers already handed out.
        int                 frame = -1;
        std::deque<Request> requests;

        void Reset( )
        {
            capture.Reset( ); scapture.Reset( );
            a.Reset( ); b.Reset( );
            sa.Reset( ); sb.Reset( );
            ra.Reset( ); rb.Reset( );

            vs.Reset( ); ps.Reset( ); cb.Reset( );
            sampler.Reset( ); blend_off.Reset( ); depth_off.Reset( ); raster.Reset( );

            width = height = bw = bh = 0;
            format = DXGI_FORMAT_UNKNOWN;

            frame = -1;
            requests.clear( );
        }
    } resources;


    HRESULT CreatePipeline( ID3D11Device* device )
    {
        if ( resources.vs && resources.ps && resources.cb && resources.sampler &&
             resources.blend_off && resources.depth_off && resources.raster )
        {
            return S_OK;
        }

        ComPtr<ID3DBlob> code, error;

        HRESULT hr = D3DCompile( k_hlsl, sizeof( k_hlsl ) - 1, nullptr, nullptr, nullptr,
            "vs_main", "vs_4_0", 0, 0, code.GetAddressOf( ), error.GetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;
        hr = device->CreateVertexShader( code->GetBufferPointer( ), code->GetBufferSize( ), nullptr,
            resources.vs.ReleaseAndGetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;

        code.Reset( );
        error.Reset( );

        hr = D3DCompile( k_hlsl, sizeof( k_hlsl ) - 1, nullptr, nullptr, nullptr,
            "ps_main", "ps_4_0", 0, 0, code.GetAddressOf( ), error.GetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;
        hr = device->CreatePixelShader( code->GetBufferPointer( ), code->GetBufferSize( ), nullptr,
            resources.ps.ReleaseAndGetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;

        D3D11_BUFFER_DESC bd{ };
        bd.ByteWidth = sizeof( Params );
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        hr = device->CreateBuffer( &bd, nullptr, resources.cb.ReleaseAndGetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;

        D3D11_SAMPLER_DESC sd{ };
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState( &sd, resources.sampler.ReleaseAndGetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;

        D3D11_BLEND_DESC bl{ };
        bl.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = device->CreateBlendState( &bl, resources.blend_off.ReleaseAndGetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;

        D3D11_DEPTH_STENCIL_DESC ds{ };
        hr = device->CreateDepthStencilState( &ds, resources.depth_off.ReleaseAndGetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;

        D3D11_RASTERIZER_DESC rd{ };
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        return device->CreateRasterizerState( &rd, resources.raster.ReleaseAndGetAddressOf( ) );
    }


    HRESULT CreateTargets( ID3D11Device* device, UINT w, UINT h, DXGI_FORMAT fmt )
    {
        if ( resources.capture && resources.a && resources.b &&
             resources.width == w && resources.height == h && resources.format == fmt )
        {
            return S_OK;
        }

        resources.capture.Reset( ); resources.scapture.Reset( );
        resources.a.Reset( ); resources.b.Reset( );
        resources.sa.Reset( ); resources.sb.Reset( );
        resources.ra.Reset( ); resources.rb.Reset( );

        const UINT bw = ImMax( w / 2u, 1u ), bh = ImMax( h / 2u, 1u );

        D3D11_TEXTURE2D_DESC td{ };
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = fmt;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;

        // The capture only ever feeds the first pass, so it needs no render target view.
        td.Width = w;
        td.Height = h;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> capture;
        HRESULT hr = device->CreateTexture2D( &td, nullptr, capture.GetAddressOf( ) );
        if ( FAILED( hr ) )
            return hr;

        td.Width = bw;
        td.Height = bh;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> a, b;
        if ( FAILED( hr = device->CreateTexture2D( &td, nullptr, a.GetAddressOf( ) ) ) ) return hr;
        if ( FAILED( hr = device->CreateTexture2D( &td, nullptr, b.GetAddressOf( ) ) ) ) return hr;

        ComPtr<ID3D11ShaderResourceView> scapture, sa, sb;
        ComPtr<ID3D11RenderTargetView>   ra, rb;
        if ( FAILED( hr = device->CreateShaderResourceView( capture.Get( ), nullptr, scapture.GetAddressOf( ) ) ) ) return hr;
        if ( FAILED( hr = device->CreateShaderResourceView( a.Get( ), nullptr, sa.GetAddressOf( ) ) ) ) return hr;
        if ( FAILED( hr = device->CreateShaderResourceView( b.Get( ), nullptr, sb.GetAddressOf( ) ) ) ) return hr;
        if ( FAILED( hr = device->CreateRenderTargetView( a.Get( ), nullptr, ra.GetAddressOf( ) ) ) ) return hr;
        if ( FAILED( hr = device->CreateRenderTargetView( b.Get( ), nullptr, rb.GetAddressOf( ) ) ) ) return hr;

        resources.capture = std::move( capture ); resources.scapture = std::move( scapture );
        resources.a = std::move( a );   resources.b = std::move( b );
        resources.sa = std::move( sa ); resources.sb = std::move( sb );
        resources.ra = std::move( ra ); resources.rb = std::move( rb );

        resources.width = w;
        resources.height = h;
        resources.bw = bw;
        resources.bh = bh;
        resources.format = fmt;
        return S_OK;
    }


    // Reads back what the context is currently rendering into. The blur owns no swap chain on
    // purpose - it blurs whatever target it is invoked inside of.
    HRESULT CurrentTarget( ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC& out_desc,
                           ComPtr<ID3D11Texture2D>& out_tex )
    {
        ComPtr<ID3D11RenderTargetView> rtv;
        context->OMGetRenderTargets( 1, rtv.GetAddressOf( ), nullptr );
        if ( !rtv )
            return E_FAIL;

        ComPtr<ID3D11Resource> res;
        rtv->GetResource( res.GetAddressOf( ) );
        if ( !res )
            return E_FAIL;

        ComPtr<ID3D11Texture2D> tex;
        if ( FAILED( res.As( &tex ) ) || !tex )
            return E_FAIL;

        tex->GetDesc( &out_desc );
        out_tex = std::move( tex );
        return S_OK;
    }


    HRESULT Prepare( )
    {
        if ( !blur::device || !blur::context )
            return E_FAIL;

        // A new frame invalidates last frame's requests; the draw lists that pointed at them have
        // already been rendered.
        const int frame = ImGui::GetFrameCount( );
        if ( resources.frame != frame )
        {
            resources.frame = frame;
            resources.requests.clear( );
        }

        HRESULT hr = CreatePipeline( blur::device );
        if ( FAILED( hr ) )
            return hr;

        // Requests are recorded while the frame is being built, long before the render pass binds
        // the back buffer, so the size comes from the display here and is corrected in Render().
        const ImVec2 display = ImGui::GetIO( ).DisplaySize;
        if ( display.x < 1.f || display.y < 1.f )
            return E_FAIL;

        return CreateTargets( blur::device, ( UINT )display.x, ( UINT )display.y,
            DXGI_FORMAT_R8G8B8A8_UNORM );
    }


    void Render( const ImDrawList*, const ImDrawCmd* cmd )
    {
        resources.last_result = S_FALSE;

        ID3D11Device* device = blur::device;
        ID3D11DeviceContext* context = blur::context;
        if ( !device || !context || !resources.vs || !resources.ps )
            return;

        ComPtr<ID3D11RenderTargetView> saved_rtv;
        ComPtr<ID3D11DepthStencilView> saved_dsv;
        context->OMGetRenderTargets( 1, saved_rtv.GetAddressOf( ), saved_dsv.GetAddressOf( ) );

        D3D11_TEXTURE2D_DESC desc{ };
        ComPtr<ID3D11Texture2D> back;
        HRESULT hr = CurrentTarget( context, desc, back );
        if ( FAILED( hr ) )
        {
            resources.last_result = hr;
            return;
        }

        hr = CreateTargets( device, desc.Width, desc.Height, desc.Format );
        if ( FAILED( hr ) || !resources.capture || !resources.a || !resources.b )
        {
            resources.last_result = FAILED( hr ) ? hr : E_FAIL;
            return;
        }

        UINT viewport_count = 1;
        D3D11_VIEWPORT saved_viewport{ };
        context->RSGetViewports( &viewport_count, &saved_viewport );

        // Snapshot the target. MSAA targets have to be resolved; ours is not, but the resolve path
        // costs nothing to keep and stops the copy silently failing if that ever changes.
        if ( desc.SampleDesc.Count > 1 )
            context->ResolveSubresource( resources.capture.Get( ), 0, back.Get( ), 0, desc.Format );
        else
            context->CopyResource( resources.capture.Get( ), back.Get( ) );

        const float w = ( float )resources.width, h = ( float )resources.height;
        const float bw = ( float )resources.bw, bh = ( float )resources.bh;

        D3D11_VIEWPORT vp{ };
        vp.Width = bw;
        vp.Height = bh;
        vp.MaxDepth = 1.f;

        ID3D11ShaderResourceView* const null_srv[ 1 ] = { nullptr };
        ID3D11Buffer* const cb = resources.cb.Get( );
        ID3D11SamplerState* const sampler = resources.sampler.Get( );
        const float blend_factor[ 4 ] = { 0.f, 0.f, 0.f, 0.f };

        context->IASetInputLayout( nullptr );
        context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        context->VSSetShader( resources.vs.Get( ), nullptr, 0 );
        context->PSSetShader( resources.ps.Get( ), nullptr, 0 );
        context->GSSetShader( nullptr, nullptr, 0 );
        context->HSSetShader( nullptr, nullptr, 0 );
        context->DSSetShader( nullptr, nullptr, 0 );
        context->CSSetShader( nullptr, nullptr, 0 );
        context->PSSetSamplers( 0, 1, &sampler );
        context->PSSetConstantBuffers( 0, 1, &cb );
        context->OMSetBlendState( resources.blend_off.Get( ), blend_factor, 0xffffffff );
        context->OMSetDepthStencilState( resources.depth_off.Get( ), 0 );
        context->RSSetState( resources.raster.Get( ) );
        context->RSSetViewports( 1, &vp );

        const auto pass = [ & ]( ID3D11ShaderResourceView* src, ID3D11RenderTargetView* dst,
                                 float dx, float dy )
        {
            Params p{ };
            p.dir[ 0 ] = dx;
            p.dir[ 1 ] = dy;
            context->UpdateSubresource( resources.cb.Get( ), 0, nullptr, &p, 0, 0 );

            context->PSSetShaderResources( 0, 1, null_srv ); // no texture can be target and source at once
            context->OMSetRenderTargets( 1, &dst, nullptr );
            context->PSSetShaderResources( 0, 1, &src );
            context->Draw( 3, 0 );
        };

        // First horizontal pass reads the full-resolution capture into the half-resolution target,
        // so its step is in the capture's texels; everything after it works in the blur targets'.
        pass( resources.scapture.Get( ), resources.rb.Get( ), 1.f / w, 0.f );
        pass( resources.sb.Get( ), resources.ra.Get( ), 0.f, 1.f / bh );

        for ( int i = 1; i < k_iterations; ++i )
        {
            pass( resources.sa.Get( ), resources.rb.Get( ), 1.f / bw, 0.f );
            pass( resources.sb.Get( ), resources.ra.Get( ), 0.f, 1.f / bh );
        }

        context->PSSetShaderResources( 0, 1, null_srv );

        ID3D11RenderTargetView* rtv = saved_rtv.Get( );
        context->OMSetRenderTargets( 1, &rtv, saved_dsv.Get( ) );
        if ( viewport_count > 0 )
            context->RSSetViewports( 1, &saved_viewport );

        IM_UNUSED( cmd );
        resources.last_result = S_OK;
    }


    void Submit( ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha, float rounding, ImColor tint )
    {
        if ( !draw || !blur::device || !blur::context || !std::isfinite( alpha ) || alpha <= 0.f )
            return;
        if ( max.x <= min.x || max.y <= min.y )
            return;
        if ( FAILED( Prepare( ) ) || !resources.sa )
            return;

        Request request{ };
        request.alpha = std::clamp( alpha, 0.f, 1.f );
        request.rounding = rounding >= 0.f ? rounding : ImGui::GetStyle( ).WindowRounding;
        request.rect = ImVec4( min.x, min.y, max.x, max.y );
        request.tint = tint;

        resources.requests.push_back( request );
        draw->AddCallback( Render, &resources.requests.back( ) );
        draw->AddCallback( ImDrawCallback_ResetRenderState, nullptr );

        const float tw = ( float )resources.width, th = ( float )resources.height;
        const ImVec2 uv_min( min.x / tw, min.y / th );
        const ImVec2 uv_max( max.x / tw, max.y / th );

        draw->AddImageRounded(
            ( ImTextureID )resources.sa.Get( ),
            min, max, uv_min, uv_max,
            IM_COL32( 255, 255, 255, IM_F32_TO_INT8_SAT( request.alpha ) ),
            request.rounding, ImDrawFlags_RoundCornersAll );

        const ImVec4 t = tint.Value;
        if ( t.w > 0.f )
            draw->AddRectFilled( min, max, ImColor( t.x, t.y, t.z, t.w * request.alpha ),
                request.rounding, ImDrawFlags_RoundCornersAll );
    }

} // namespace


namespace blur
{
    HRESULT last_result( ) { return resources.last_result; }

    void on_device_reset( ) { resources.Reset( ); }

    void shutdown( )
    {
        resources.Reset( );
        device = nullptr;
        context = nullptr;
    }
}


void draw_blur( ImDrawList* draw, float alpha, float rounding )
{
    if ( !draw )
        return;

    // No explicit rect: blur exactly what the caller has clipped to, which is how the menu chrome
    // in imgui.cpp uses it (it pushes the window rect immediately before calling).
    Submit( draw, draw->GetClipRectMin( ), draw->GetClipRectMax( ), alpha, rounding,
        ImColor( 1.0f, 1.0f, 1.0f, 0.0f ) );
}

void draw_blur_rounded( ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha, float rounding, ImColor tint )
{
    Submit( draw, min, max, alpha, rounding, tint );
}
