#include "shader_bg.hpp"
#include "imgui_internal.h" // ImClamp / ImSaturate / IM_F32_TO_INT8_SAT
#include "hashes.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cmath>

#include "shaders/ColorBends.h"
#include "shaders/Galaxy.h"
#include "shaders/LightRays.h"
#include "shaders/Lightning.h"
#include "shaders/LineWaves.h"
#include "shaders/LiquidChrome.h"
#include "shaders/LiquidEther.h"
#include "shaders/Particles.h"
#include "shaders/PixelSnow.h"
#include "shaders/Plasma.h"
#include "shaders/Prism.h"
#include "shaders/PrismaticBurst.h"
#include "shaders/Custom.h"

#include "gui.hpp" // the accent the custom effects take their palette from

extern ID3D11DeviceContext*    g_pd3dDeviceContext;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

namespace {

    using Microsoft::WRL::ComPtr;

    constexpr char k_blur_hlsl[] = R"HLSL(
cbuffer BlurParams : register(b0) {
    float2 direction;
    int pairCount;
    float padding;
    float4 kernel[16];
};
Texture2D source : register(t0);
SamplerState linearClamp : register(s0);
struct VS_OUT { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
VS_OUT vs_main(uint id : SV_VertexID) {
    VS_OUT result;
    result.uv = float2((id << 1) & 2, id & 2);
    result.position = float4(result.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return result;
}
float4 ps_main(VS_OUT input) : SV_Target {
    float4 color = source.Sample(linearClamp, input.uv) * kernel[0].x;
    [loop] for (int i = 1; i <= pairCount; ++i) {
        float2 offset = direction * kernel[i].y;
        color += (source.Sample(linearClamp, input.uv + offset)
                + source.Sample(linearClamp, input.uv - offset)) * kernel[i].x;
    }
    return color;
}
)HLSL";

    struct alignas( 16 ) blur_params_t {
        float direction[ 2 ]{};
        int pair_count = 0;
        float padding = 0.f;
        float kernel[ 16 ][ 4 ]{};
    };

    struct blur_target_t {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> view;
        ComPtr<ID3D11RenderTargetView> target;
    };

    // The postprocess runs while building the menu, before ImGui installs its render state.
    struct blur_context_state_t {
        ID3D11DeviceContext* context;
        ComPtr<ID3D11InputLayout> layout;
        D3D11_PRIMITIVE_TOPOLOGY topology{};
        ComPtr<ID3D11VertexShader> vs;
        ComPtr<ID3D11PixelShader> ps;
        ComPtr<ID3D11GeometryShader> gs;
        ComPtr<ID3D11HullShader> hs;
        ComPtr<ID3D11DomainShader> ds;
        ComPtr<ID3D11Buffer> constants;
        ComPtr<ID3D11SamplerState> sampler;
        ComPtr<ID3D11ShaderResourceView> source;
        ComPtr<ID3D11BlendState> blend;
        ComPtr<ID3D11DepthStencilState> depth;
        ComPtr<ID3D11RasterizerState> raster;
        ID3D11RenderTargetView* targets[ D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT ]{};
        ComPtr<ID3D11DepthStencilView> depth_view;
        float blend_factor[ 4 ]{};
        UINT sample_mask = 0, stencil_ref = 0;
        UINT viewport_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        D3D11_VIEWPORT viewports[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ]{};

        explicit blur_context_state_t( ID3D11DeviceContext* ctx ) : context( ctx ) {
            ctx->IAGetInputLayout( layout.GetAddressOf( ) );
            ctx->IAGetPrimitiveTopology( &topology );
            ctx->VSGetShader( vs.GetAddressOf( ), nullptr, nullptr );
            ctx->PSGetShader( ps.GetAddressOf( ), nullptr, nullptr );
            ctx->GSGetShader( gs.GetAddressOf( ), nullptr, nullptr );
            ctx->HSGetShader( hs.GetAddressOf( ), nullptr, nullptr );
            ctx->DSGetShader( ds.GetAddressOf( ), nullptr, nullptr );
            ctx->PSGetConstantBuffers( 0, 1, constants.GetAddressOf( ) );
            ctx->PSGetSamplers( 0, 1, sampler.GetAddressOf( ) );
            ctx->PSGetShaderResources( 0, 1, source.GetAddressOf( ) );
            ctx->OMGetBlendState( blend.GetAddressOf( ), blend_factor, &sample_mask );
            ctx->OMGetDepthStencilState( depth.GetAddressOf( ), &stencil_ref );
            ctx->RSGetState( raster.GetAddressOf( ) );
            ctx->OMGetRenderTargets( IM_ARRAYSIZE( targets ), targets, depth_view.GetAddressOf( ) );
            ctx->RSGetViewports( &viewport_count, viewports );
        }

        ~blur_context_state_t( ) {
            ID3D11ShaderResourceView* null_view = nullptr;
            context->PSSetShaderResources( 0, 1, &null_view );
            context->OMSetRenderTargets( IM_ARRAYSIZE( targets ), targets, depth_view.Get( ) );
            for ( auto* target : targets )
                if ( target ) target->Release( );
            context->IASetInputLayout( layout.Get( ) );
            context->IASetPrimitiveTopology( topology );
            context->VSSetShader( vs.Get( ), nullptr, 0 );
            context->PSSetShader( ps.Get( ), nullptr, 0 );
            context->GSSetShader( gs.Get( ), nullptr, 0 );
            context->HSSetShader( hs.Get( ), nullptr, 0 );
            context->DSSetShader( ds.Get( ), nullptr, 0 );
            ID3D11Buffer* cb = constants.Get( );
            ID3D11SamplerState* ss = sampler.Get( );
            ID3D11ShaderResourceView* srv = source.Get( );
            context->PSSetConstantBuffers( 0, 1, &cb );
            context->PSSetSamplers( 0, 1, &ss );
            context->PSSetShaderResources( 0, 1, &srv );
            context->OMSetBlendState( blend.Get( ), blend_factor, sample_mask );
            context->OMSetDepthStencilState( depth.Get( ), stencil_ref );
            context->RSSetState( raster.Get( ) );
            context->RSSetViewports( viewport_count, viewports );
        }
    };

    struct shader_blur_t {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11VertexShader> vs;
        ComPtr<ID3D11PixelShader> ps;
        ComPtr<ID3D11Buffer> constants;
        ComPtr<ID3D11SamplerState> sampler;
        ComPtr<ID3D11BlendState> blend;
        ComPtr<ID3D11DepthStencilState> depth;
        ComPtr<ID3D11RasterizerState> raster;
        blur_target_t targets[ 2 ];
        ComPtr<ID3D11ShaderResourceView> previous_view;
        UINT width = 0, height = 0;
        bool ready = false;

        bool prepare( UINT w, UINT h ) {
            if ( device.Get( ) != g_pd3dDevice ) {
                *this = shader_blur_t{};
                device = g_pd3dDevice;
            }
            if ( !device )
                return false;

            if ( !vs || !ps || !constants || !sampler || !blend || !depth || !raster ) {
                ComPtr<ID3DBlob> code;
                if ( FAILED( D3DCompile( k_blur_hlsl, sizeof( k_blur_hlsl ) - 1, nullptr,
                    nullptr, nullptr, "vs_main", "vs_4_0", 0, 0, code.GetAddressOf( ), nullptr ) ) ) return false;
                if ( FAILED( device->CreateVertexShader( code->GetBufferPointer( ), code->GetBufferSize( ),
                    nullptr, vs.ReleaseAndGetAddressOf( ) ) ) ) return false;
                code.Reset( );
                if ( FAILED( D3DCompile( k_blur_hlsl, sizeof( k_blur_hlsl ) - 1, nullptr,
                    nullptr, nullptr, "ps_main", "ps_4_0", 0, 0, code.GetAddressOf( ), nullptr ) ) ) return false;
                if ( FAILED( device->CreatePixelShader( code->GetBufferPointer( ), code->GetBufferSize( ),
                    nullptr, ps.ReleaseAndGetAddressOf( ) ) ) ) return false;

                D3D11_BUFFER_DESC cb{};
                cb.ByteWidth = sizeof( blur_params_t );
                cb.Usage = D3D11_USAGE_DEFAULT;
                cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                if ( FAILED( device->CreateBuffer( &cb, nullptr, constants.ReleaseAndGetAddressOf( ) ) ) ) return false;
                D3D11_SAMPLER_DESC ss{};
                ss.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                ss.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
                ss.MaxLOD = D3D11_FLOAT32_MAX;
                if ( FAILED( device->CreateSamplerState( &ss, sampler.ReleaseAndGetAddressOf( ) ) ) ) return false;
                D3D11_BLEND_DESC bs{};
                bs.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
                if ( FAILED( device->CreateBlendState( &bs, blend.ReleaseAndGetAddressOf( ) ) ) ) return false;
                D3D11_DEPTH_STENCIL_DESC ds{};
                if ( FAILED( device->CreateDepthStencilState( &ds, depth.ReleaseAndGetAddressOf( ) ) ) ) return false;
                D3D11_RASTERIZER_DESC rs{};
                rs.FillMode = D3D11_FILL_SOLID;
                rs.CullMode = D3D11_CULL_NONE;
                rs.DepthClipEnable = TRUE;
                if ( FAILED( device->CreateRasterizerState( &rs, raster.ReleaseAndGetAddressOf( ) ) ) ) return false;
            }

            if ( width == w && height == h && targets[ 1 ].view )
                return true;

            blur_target_t next[ 2 ];
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = w;
            desc.Height = h;
            desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            for ( auto& target : next ) {
                if ( FAILED( device->CreateTexture2D( &desc, nullptr, target.texture.GetAddressOf( ) ) ) ||
                     FAILED( device->CreateShaderResourceView( target.texture.Get( ), nullptr, target.view.GetAddressOf( ) ) ) ||
                     FAILED( device->CreateRenderTargetView( target.texture.Get( ), nullptr, target.target.GetAddressOf( ) ) ) )
                    return false;
            }

            // Begin() may already have recorded last frame's view before this update resizes it.
            previous_view = targets[ 1 ].view;
            targets[ 0 ] = std::move( next[ 0 ] );
            targets[ 1 ] = std::move( next[ 1 ] );
            width = w;
            height = h;
            return true;
        }

        void render( ID3D11ShaderResourceView* source, ImVec2 size, float intensity ) {
            ready = false;
            if ( !source || !g_pd3dDeviceContext || !std::isfinite( intensity ) || intensity <= 0.f )
                return;
            if ( !prepare( ( UINT )size.x, ( UINT )size.y ) )
                return;

            intensity = ImClamp( intensity, 0.f, 30.f );
            const int radius = ( int )std::ceil( intensity );
            const float sigma = ImMax( intensity / 3.f, 0.1f );
            float weights[ 31 ]{};
            float total = 1.f;
            weights[ 0 ] = 1.f;
            for ( int i = 1; i <= radius; ++i ) {
                weights[ i ] = std::exp( -( float )( i * i ) / ( 2.f * sigma * sigma ) );
                total += 2.f * weights[ i ];
            }

            blur_params_t params;
            params.kernel[ 0 ][ 0 ] = 1.f / total;
            // Linear filtering combines each neighbouring pair into one texture fetch.
            for ( int i = 1; i <= radius; i += 2 ) {
                const float a = weights[ i ];
                const float b = i + 1 <= radius ? weights[ i + 1 ] : 0.f;
                const float weight = a + b;
                if ( weight <= 0.f ) continue;
                const int pair = ++params.pair_count;
                params.kernel[ pair ][ 0 ] = weight / total;
                params.kernel[ pair ][ 1 ] = ( i * a + ( i + 1 ) * b ) / weight;
            }

            ID3D11DeviceContext* ctx = g_pd3dDeviceContext;
            blur_context_state_t saved( ctx );
            ID3D11Buffer* cb = constants.Get( );
            ID3D11SamplerState* ss = sampler.Get( );
            const float blend_factor[ 4 ]{};
            D3D11_VIEWPORT viewport{};
            viewport.Width = ( float )width;
            viewport.Height = ( float )height;
            viewport.MaxDepth = 1.f;
            ctx->IASetInputLayout( nullptr );
            ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
            ctx->VSSetShader( vs.Get( ), nullptr, 0 );
            ctx->PSSetShader( ps.Get( ), nullptr, 0 );
            ctx->GSSetShader( nullptr, nullptr, 0 );
            ctx->HSSetShader( nullptr, nullptr, 0 );
            ctx->DSSetShader( nullptr, nullptr, 0 );
            ctx->PSSetConstantBuffers( 0, 1, &cb );
            ctx->PSSetSamplers( 0, 1, &ss );
            ctx->OMSetBlendState( blend.Get( ), blend_factor, 0xffffffff );
            ctx->OMSetDepthStencilState( depth.Get( ), 0 );
            ctx->RSSetState( raster.Get( ) );
            ctx->RSSetViewports( 1, &viewport );

            for ( int axis = 0; axis < 2; ++axis ) {
                params.direction[ 0 ] = axis == 0 ? 1.f / width : 0.f;
                params.direction[ 1 ] = axis == 1 ? 1.f / height : 0.f;
                ctx->UpdateSubresource( cb, 0, nullptr, &params, 0, 0 );
                ID3D11ShaderResourceView* null_view = nullptr;
                ID3D11RenderTargetView* target = targets[ axis ].target.Get( );
                ID3D11ShaderResourceView* input = axis == 0 ? source : targets[ 0 ].view.Get( );
                ctx->PSSetShaderResources( 0, 1, &null_view );
                ctx->OMSetRenderTargets( 1, &target, nullptr );
                ctx->PSSetShaderResources( 0, 1, &input );
                ctx->Draw( 3, 0 );
            }
            ready = true;
        }
    } s_blur;

    // Instances are created lazily by their own Update(), so the ones never selected cost nothing
    // beyond the (empty) object.
    shader::ColorBends     s_bends;
    shader::Galaxy         s_galaxy;
    shader::LightRays      s_rays;
    shader::Lightning      s_lightning;
    shader::LineWaves      s_waves;
    shader::LiquidChrome   s_chrome;
    shader::LiquidEther    s_ether;
    shader::Particles      s_particles;
    shader::PixelSnow      s_snow;
    shader::Plasma         s_plasma;
    shader::Prism          s_prism;
    shader::PrismaticBurst s_burst;

    // Written for this menu - one runtime, nine pixel shaders (shaders/Custom.h).
    shader::CustomEffect s_mesh  ( shader::MESH_HLSL );
    shader::CustomEffect s_aurora( shader::AURORA_HLSL );
    shader::CustomEffect s_dots  ( shader::DOTS_HLSL );
    shader::CustomEffect s_hex   ( shader::HEX_HLSL );
    shader::CustomEffect s_warp  ( shader::WARP_HLSL );
    shader::CustomEffect s_cells ( shader::VORONOI_HLSL );
    shader::CustomEffect s_rain  ( shader::RAIN_HLSL );
    shader::CustomEffect s_ripple( shader::RIPPLE_HLSL );
    shader::CustomEffect s_glass ( shader::GLASS_HLSL );

    struct entry_t {
        const char* name;
        const char* icon;
        void  ( *update )( float dt, ImVec2 size, ImVec2 pos );
        void* ( *view )( );
    };

    // Captureless lambdas so the table stays a plain array of function pointers - every effect has
    // the same Update()/outputFBO.srv shape, only the type differs.
#define SHADER_BG_ENTRY( var, label, glyph )                                              \
    { label, glyph,                                                                       \
      []( float dt, ImVec2 size, ImVec2 pos ) { var.Update( dt, size, pos ); },           \
      []( ) -> void* { return ( void* )var.outputFBO.srv; } }

    const entry_t k_entries[] = {
        SHADER_BG_ENTRY( s_plasma,    "Plasma",          ICON_FA_FIRE ),
        SHADER_BG_ENTRY( s_galaxy,    "Galaxy",          ICON_FA_STAR ),
        SHADER_BG_ENTRY( s_ether,     "Liquid Ether",    ICON_FA_WATER ),
        SHADER_BG_ENTRY( s_chrome,    "Liquid Chrome",   ICON_FA_TINT ),
        SHADER_BG_ENTRY( s_bends,     "Color Bends",     ICON_FA_PALETTE ),
        SHADER_BG_ENTRY( s_rays,      "Light Rays",      ICON_FA_SUN ),
        SHADER_BG_ENTRY( s_lightning, "Lightning",       ICON_FA_BOLT ),
        SHADER_BG_ENTRY( s_waves,     "Line Waves",      ICON_FA_WAVE_SQUARE ),
        SHADER_BG_ENTRY( s_particles, "Particles",       ICON_FA_ATOM ),
        SHADER_BG_ENTRY( s_snow,      "Pixel Snow",      ICON_FA_SNOWFLAKE ),
        SHADER_BG_ENTRY( s_prism,     "Prism",           ICON_FA_GEM ),
        SHADER_BG_ENTRY( s_burst,     "Prismatic Burst", ICON_FA_CERTIFICATE ),

        // --- everything from here down is ours; k_builtin_count splits the two lists in the UI ---
        SHADER_BG_ENTRY( s_glass,     "Liquid Glass",    ICON_FA_ADJUST ),
        SHADER_BG_ENTRY( s_mesh,      "Mesh Gradient",   ICON_FA_FILL_DRIP ),
        SHADER_BG_ENTRY( s_aurora,    "Aurora",          ICON_FA_MOUNTAIN ),
        SHADER_BG_ENTRY( s_dots,      "Dot Matrix",      ICON_FA_TH ),
        SHADER_BG_ENTRY( s_hex,       "Hex Grid",        ICON_FA_TH_LARGE ),
        SHADER_BG_ENTRY( s_warp,      "Starfield Warp",  ICON_FA_ROCKET ),
        SHADER_BG_ENTRY( s_cells,     "Voronoi",         ICON_FA_VECTOR_SQUARE ),
        SHADER_BG_ENTRY( s_rain,      "Rain on Glass",   ICON_FA_CLOUD_RAIN ),
        SHADER_BG_ENTRY( s_ripple,    "Click Ripple",    ICON_FA_BULLSEYE ),
    };

    constexpr int k_builtin_count = 12; // the vendored ImGui-Shader set, first in the table

#undef SHADER_BG_ENTRY

    shader_bg::state_t s_state;

    // The rect the current view was rendered for, and the effect it belongs to. A change in size or
    // effect means the view the chrome is about to record would be released by this frame's
    // update(), so that frame draws nothing instead of a dangling texture.
    ImVec2 s_view_size( 0.f, 0.f );
    ImVec2 s_view_pos( 0.f, 0.f );
    int    s_view_index = -1;

    // What Click Ripple answers to. Kept here rather than in the effect so it survives switching
    // away and back, and so every future effect can read the same click.
    ImVec2 s_click( 0.5f, 0.5f );
    float  s_click_age = 1.0e6f;
}

namespace shader_bg {

    state_t& state( ) { return s_state; }

    int count( ) { return IM_ARRAYSIZE( k_entries ); }

    int builtin_count( ) { return k_builtin_count; }

    const char* name( int index ) {
        return ( index >= 0 && index < count( ) ) ? k_entries[ index ].name : "";
    }

    const char* icon( int index ) {
        return ( index >= 0 && index < count( ) ) ? k_entries[ index ].icon : "";
    }

    void update( ImVec2 size, ImVec2 pos ) {
        s_blur.previous_view.Reset( );
        s_blur.ready = false;
        if ( !s_state.enabled ) {
            s_view_index = -1;
            return;
        }

        s_state.selected = ImClamp( s_state.selected, 0, count( ) - 1 );
        if ( size.x < 1.f || size.y < 1.f )
            return;

        const float dt = ImGui::GetIO( ).DeltaTime * ImMax( s_state.speed, 0.f );

        // Per-frame inputs the (dt, size, pos) dispatch signature has no room for. The palette
        // comes from the menu's own accent, so the background follows whatever colour the user
        // picked in the profile card instead of fighting it.
        const ImVec2 click_mouse = ImGui::GetMousePos( );
        const bool inside_menu = click_mouse.x >= pos.x && click_mouse.x < pos.x + size.x &&
                                 click_mouse.y >= pos.y && click_mouse.y < pos.y + size.y;
        if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && inside_menu ) {
            s_click = ImVec2( ( click_mouse.x - pos.x ) / size.x, ( click_mouse.y - pos.y ) / size.y );
            s_click_age = 0.f;
        }
        else {
            s_click_age += ImGui::GetIO( ).DeltaTime;
        }
        shader::set_custom_env( gui.accent_color.to_vec4( 1.f, false ), s_click, s_click_age );

        // The mouse-driven effects gate their input on ImGui::IsWindowHovered() with no
        // ChildWindows flag, which reads false the moment the pointer sits on one of the menu's
        // children - that is every group box, the sidebar and the content pane, so very nearly all
        // of it, and the effect only responded in the bare gaps between boxes. Point the hover at
        // the window this is being run from while they sample it, so the whole menu surface (and
        // anything of ours drawn over it) drives them.
        ImGuiContext& g = *ImGui::GetCurrentContext( );
        ImGuiWindow* const saved_hovered = g.HoveredWindow;
        const ImVec2 mouse = ImGui::GetMousePos( );
        if ( g.CurrentWindow &&
             mouse.x >= pos.x && mouse.x < pos.x + size.x &&
             mouse.y >= pos.y && mouse.y < pos.y + size.y )
        {
            g.HoveredWindow = g.CurrentWindow;
        }

        k_entries[ s_state.selected ].update( dt, size, pos );

        g.HoveredWindow = saved_hovered;

        if ( s_state.blur_enabled )
            s_blur.render( ( ID3D11ShaderResourceView* )k_entries[ s_state.selected ].view( ),
                size, s_state.blur_intensity );

        // Every effect leaves its own target bound; the frame is still being built, so put the back
        // buffer back before anything else touches the context.
        if ( g_pd3dDeviceContext )
            g_pd3dDeviceContext->OMSetRenderTargets( 1, &g_mainRenderTargetView, nullptr );

        s_view_size = size;
        s_view_pos = pos;
        s_view_index = s_state.selected;
    }

    void* view( ImVec2 size ) {
        if ( !s_state.enabled || s_view_index < 0 )
            return nullptr;
        if ( s_view_index != s_state.selected )
            return nullptr;
        if ( ( int )size.x != ( int )s_view_size.x || ( int )size.y != ( int )s_view_size.y )
            return nullptr;
        if ( s_state.blur_enabled && s_blur.ready && s_state.blur_intensity > 0.f )
            return s_blur.targets[ 1 ].view.Get( );
        return k_entries[ s_view_index ].view( );
    }

    bool draw_into( ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float alpha_scale ) {
        if ( !draw || max.x <= min.x || max.y <= min.y )
            return false;

        void* srv = view( s_view_size );
        if ( !srv )
            return false;

        const float alpha = ImSaturate( s_state.opacity ) * ImSaturate( alpha_scale );
        if ( alpha <= 0.002f )
            return false;

        ImVec2 uv_min( ( min.x - s_view_pos.x ) / s_view_size.x, ( min.y - s_view_pos.y ) / s_view_size.y );
        ImVec2 uv_max( ( max.x - s_view_pos.x ) / s_view_size.x, ( max.y - s_view_pos.y ) / s_view_size.y );

        // ImGui's sampler wraps, so a card hanging off the menu would tile the effect. Slide the
        // window back inside instead of clamping its edges: the same span of the image, undistorted,
        // and still exactly continuous with the menu for anything that sits over it.
        const auto contain = [ ]( float& lo, float& hi ) {
            if ( hi - lo >= 1.f ) { lo = 0.f; hi = 1.f; return; }
            if ( lo < 0.f )      { hi -= lo; lo = 0.f; }
            else if ( hi > 1.f ) { lo -= hi - 1.f; hi = 1.f; }
        };
        contain( uv_min.x, uv_max.x );
        contain( uv_min.y, uv_max.y );

        draw->AddImageRounded( ( ImTextureID )srv, min, max, uv_min, uv_max,
            IM_COL32( 255, 255, 255, IM_F32_TO_INT8_SAT( alpha ) ), rounding, ImDrawFlags_RoundCornersAll );
        return true;
    }

    void shutdown( ) {
        s_blur = shader_blur_t{};
        s_view_index = -1;
        s_view_size = ImVec2( 0.f, 0.f );
        s_view_pos = ImVec2( 0.f, 0.f );
    }
}

void menu_shader_background( ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding ) {
    shader_bg::draw_into( draw, min, max, rounding, 1.f );
}
