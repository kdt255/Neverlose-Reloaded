#pragma once

// Custom.h - the effects written for this menu, on one shared runtime.
//
// The vendored ImGui-Shader headers each carry their own copy of the compile / FBO / resize /
// dispatch boilerplate. That is fine for one effect at a time and unbearable at nine, so these
// share a single `CustomEffect` and differ only by their pixel shader body: every one of them is
// a fullscreen triangle sampling nothing, reading one constant buffer, writing one opaque colour.
//
// Every effect takes its palette from the menu's accent, so the background follows whatever the
// user picked in the profile card instead of fighting it.

#include "imgui.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>

#pragma comment( lib, "d3dcompiler.lib" )

extern ID3D11Device*        g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;

namespace shader {

// Must match the cbuffer in the prologue below register for register: HLSL packs into float4
// slots, so the order and the padding here are load-bearing.
__declspec( align( 16 ) ) struct CustomProps {
    float uResolution[ 2 ];  // c0.xy
    float uMouse[ 2 ];       // c0.zw
    float uAccent[ 4 ];      // c1
    float uTime;             // c2.x
    float uAspect;           // c2.y
    float uClickAge;         // c2.z
    float uPad0;             // c2.w
    float uClick[ 2 ];       // c3.xy
    float uPad1[ 2 ];        // c3.zw
};

// Set once per frame by shader_bg before the selected effect runs - the effects are driven through
// a plain function-pointer table, so per-frame inputs that are not (dt, size, pos) arrive here.
inline float g_custom_accent[ 4 ] = { 0.30f, 0.45f, 1.00f, 1.00f };
inline float g_custom_click[ 2 ]  = { 0.5f, 0.5f };
inline float g_custom_click_age   = 1.0e6f;

inline void set_custom_env( const ImVec4& accent, const ImVec2& click_uv, float click_age ) {
    g_custom_accent[ 0 ] = accent.x;
    g_custom_accent[ 1 ] = accent.y;
    g_custom_accent[ 2 ] = accent.z;
    g_custom_accent[ 3 ] = accent.w;
    g_custom_click[ 0 ] = click_uv.x;
    g_custom_click[ 1 ] = click_uv.y;
    g_custom_click_age = click_age;
}

// Shared HLSL: the constant buffer, the fullscreen-triangle vertex shader and the hash / noise
// helpers. Prepended to every effect body before compiling.
inline const char* const CUSTOM_HLSL_PROLOGUE = R"HLSL(
cbuffer CustomProps : register(b0) {
    float2 uResolution;
    float2 uMouse;      // 0..1 across the menu, y up
    float4 uAccent;
    float  uTime;
    float  uAspect;
    float  uClickAge;   // seconds since the last click inside the menu
    float  uPad0;
    float2 uClick;      // 0..1, y down
    float2 uPad1;
};

struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VS_OUT vs_main(uint id : SV_VertexID) {
    VS_OUT o;
    o.uv  = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return o;
}

float hash11(float p) {
    p = frac(p * 0.1031f);
    p *= p + 33.33f;
    p *= p + p;
    return frac(p);
}

float hash12(float2 p) {
    float3 p3 = frac(p.xyx * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float2 hash22(float2 p) {
    float3 p3 = frac(p.xyx * float3(0.1031f, 0.1030f, 0.0973f));
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.xx + p3.yz) * p3.zy);
}

float vnoise(float2 p) {
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = hash12(i);
    float b = hash12(i + float2(1.0f, 0.0f));
    float c = hash12(i + float2(0.0f, 1.0f));
    float d = hash12(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm(float2 p) {
    float v = 0.0f, a = 0.5f;
    [unroll] for (int i = 0; i < 5; ++i) {
        v += a * vnoise(p);
        p *= 2.02f;
        a *= 0.5f;
    }
    return v;
}
)HLSL";


// ---------------------------------------------------------------------------------------------
// Effect bodies
// ---------------------------------------------------------------------------------------------

// Soft drifting colour blobs - the "mesh gradient" look. The calmest of the set and the one that
// stays readable under a full menu of text.
inline const char* const MESH_HLSL = R"HLSL(
float4 ps_main(VS_OUT i) : SV_Target {
    float2 p = float2(i.uv.x * uAspect, i.uv.y);
    float  t = uTime * 0.18f;

    float3 a  = uAccent.rgb;
    float3 c0 = a;
    float3 c1 = saturate(a.bgr * 1.15f + 0.04f);
    float3 c2 = saturate(a * float3(0.45f, 0.65f, 1.35f));
    float3 c3 = float3(0.030f, 0.040f, 0.070f);

    float2 q0 = float2(uAspect * (0.30f + 0.22f * sin(t * 1.10f)), 0.32f + 0.18f * cos(t * 0.83f));
    float2 q1 = float2(uAspect * (0.74f + 0.20f * cos(t * 0.91f)), 0.28f + 0.21f * sin(t * 1.27f));
    float2 q2 = float2(uAspect * (0.55f + 0.26f * sin(t * 0.67f)), 0.78f + 0.16f * cos(t * 1.03f));
    float2 q3 = float2(uAspect * (0.15f + 0.18f * cos(t * 1.31f)), 0.80f + 0.14f * sin(t * 0.74f));

    // inverse-square weights give the soft falloff a mesh gradient lives on
    float w0 = 1.0f / (dot(p - q0, p - q0) + 0.05f);
    float w1 = 1.0f / (dot(p - q1, p - q1) + 0.05f);
    float w2 = 1.0f / (dot(p - q2, p - q2) + 0.05f);
    float w3 = 1.0f / (dot(p - q3, p - q3) + 0.07f);

    float3 col = (c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3) / (w0 + w1 + w2 + w3);
    col *= 0.86f + 0.14f * fbm(p * 3.0f + t); // dither the banding out
    return float4(col * 0.75f, 1.0f);
}
)HLSL";

// Vertical curtains wandering across the top, striated along their length.
inline const char* const AURORA_HLSL = R"HLSL(
float4 ps_main(VS_OUT i) : SV_Target {
    float2 uv = i.uv;
    float  t  = uTime * 0.22f;
    float3 col = float3(0.014f, 0.020f, 0.042f);

    [unroll] for (int k = 0; k < 5; ++k) {
        float fk = (float)k;
        float x  = uv.x * 2.1f + fk * 0.53f;
        float centre = 0.22f + fk * 0.125f
                     + 0.10f * sin(t * (0.60f + fk * 0.13f) + fk * 1.9f)
                     + 0.16f * fbm(float2(x, t * 0.5f + fk * 4.0f));

        // Asymmetric on purpose: a curtain has a defined top edge and spills light downward, and
        // a symmetric gaussian was what made this read as haze instead.
        float dy = uv.y - centre;
        float w  = dy < 0.0f ? 0.022f : 0.075f + 0.02f * sin(t * 0.9f + fk * 2.3f);
        float band = exp(-pow(abs(dy) / w, 1.5f));
        band *= 0.30f + 0.70f * fbm(float2(uv.x * 16.0f + t * 1.4f + fk * 7.0f, uv.y * 3.0f));

        float3 tint = lerp(uAccent.rgb, saturate(uAccent.rgb.bgr * 1.30f), frac(fk * 0.37f));
        col += tint * band * (1.15f - fk * 0.12f);
    }

    col *= 0.62f + 0.38f * (1.0f - uv.y * 0.8f);
    return float4(col, 1.0f);
}
)HLSL";

// A grid of dots lit by a travelling ring, with a bright patch under the pointer.
inline const char* const DOTS_HLSL = R"HLSL(
float4 ps_main(VS_OUT i) : SV_Target {
    float2 p = float2(i.uv.x * uAspect, i.uv.y);
    float  cells = 46.0f;
    float2 g  = p * cells;
    float2 id = floor(g);
    float2 f  = frac(g) - 0.5f;

    float2 c = (id + 0.5f) / cells;
    float2 m = float2(uMouse.x * uAspect, 1.0f - uMouse.y);

    float wave = 0.5f + 0.5f * sin(uTime * 1.6f - length(c - float2(uAspect * 0.5f, 0.5f)) * 9.0f);
    float near = exp(-length(c - m) * 7.0f);
    float amp  = saturate(wave * 0.55f + near * 0.90f);

    float r = 0.06f + 0.20f * amp;
    float d = smoothstep(r, r * 0.45f, length(f));

    float3 col = float3(0.026f, 0.034f, 0.058f) + uAccent.rgb * d * (0.30f + 0.85f * amp);
    return float4(col, 1.0f);
}
)HLSL";

// Hex cells whose borders light up in a wave rolling out from the centre.
inline const char* const HEX_HLSL = R"HLSL(
static const float2 kHexS = float2(1.0f, 1.7320508f);

// Nearest of the two interleaved lattices that make up a hex grid: xy is the offset inside the
// cell, zw the cell's id. The second lattice has to be sampled from its own shifted floor(), not
// from the first's cell - sharing one floor() leaves wedges of the plane outside both hexagons,
// where the distance runs past the 0.5 border value and the outline fills in as a solid tile.
float4 hexCell(float2 p) {
    float4 hC = floor(float4(p, p - float2(0.5f, 1.0f)) / kHexS.xyxy) + 0.5f;
    float4 h  = float4(p - hC.xy * kHexS, p - (hC.zw + 0.5f) * kHexS);
    return dot(h.xy, h.xy) < dot(h.zw, h.zw) ? float4(h.xy, hC.xy) : float4(h.zw, hC.zw + 0.5f);
}

// 0 at the cell's centre, exactly 0.5 on its border.
float hexDist(float2 p) {
    p = abs(p);
    return max(dot(p, kHexS * 0.5f), p.x);
}

float4 ps_main(VS_OUT i) : SV_Target {
    float2 p = float2(i.uv.x * uAspect, i.uv.y) * 13.0f;
    float4 h = hexCell(p);
    float  d = hexDist(h.xy);

    // `rim`, not `line` - `line` is an HLSL keyword (geometry shader primitive) and naming a local
    // that fails the whole compile, which shows up as an effect that silently renders nothing.
    // A cell's distance runs 0 at its centre to 0.5 on its border, so the outline has to be a very
    // thin slice off the top of that - at 0.42 it was a third of the cell's area and the grid read
    // as filled tiles rather than as a wireframe.
    float rim  = smoothstep(0.482f, 0.50f, d);
    float fill = smoothstep(0.50f, 0.45f, d);

    float pulse = 0.5f + 0.5f * sin(uTime * 1.4f - length(h.zw) * 0.55f + hash12(h.zw) * 6.28318f);
    pulse = pow(pulse, 3.0f);

    float3 col = float3(0.022f, 0.030f, 0.054f);
    col += uAccent.rgb * rim * (0.45f + 1.00f * pulse);
    col += uAccent.rgb * fill * 0.030f * pulse;
    return float4(col, 1.0f);
}
)HLSL";

// Stars streaking outward from the centre, three layers at different speeds.
inline const char* const WARP_HLSL = R"HLSL(
float4 ps_main(VS_OUT i) : SV_Target {
    float2 uv = (i.uv - 0.5f) * float2(uAspect, 1.0f);
    float  t  = uTime * 0.35f;

    float ang = atan2(uv.y, uv.x);
    float rad = length(uv);
    float3 col = float3(0.013f, 0.018f, 0.036f);

    [unroll] for (int k = 0; k < 3; ++k) {
        float fk = (float)k;
        float lanes = 90.0f + fk * 55.0f;
        float a  = (ang / 6.28318f + 0.5f) * lanes;
        float ai = floor(a);
        float af = frac(a) - 0.5f;

        float seed  = hash11(ai + fk * 77.0f);
        float z     = frac(seed + t * (0.35f + seed * 0.90f));
        float r     = z * z * 0.90f;   // accelerating away from the centre
        float star  = exp(-abs(rad - r) * (26.0f + fk * 10.0f)) * exp(-abs(af) * 40.0f);
        star *= smoothstep(0.0f, 0.25f, z) * (1.0f - smoothstep(0.70f, 1.0f, z));

        col += lerp(float3(1.0f, 1.0f, 1.0f), uAccent.rgb, 0.55f) * star * (0.90f - fk * 0.22f);
    }

    col *= 1.0f - smoothstep(0.15f, 0.95f, rad) * 0.35f;
    return float4(col, 1.0f);
}
)HLSL";

// Drifting organic cells, lit along the borders where the two nearest points are equidistant.
inline const char* const VORONOI_HLSL = R"HLSL(
float4 ps_main(VS_OUT i) : SV_Target {
    float2 p  = float2(i.uv.x * uAspect, i.uv.y) * 6.5f;
    float  t  = uTime * 0.40f;
    float2 ip = floor(p), fp = frac(p);

    float  d1 = 8.0f, d2 = 8.0f;
    float2 best = float2(0.0f, 0.0f);

    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            float2 o = float2((float)x, (float)y);
            float2 h = hash22(ip + o);
            float2 q = o + 0.5f + 0.42f * sin(t + 6.28318f * h);
            float  d = length(q - fp);
            if (d < d1) { d2 = d1; d1 = d; best = ip + o; }
            else if (d < d2) { d2 = d; }
        }
    }

    float  edge = smoothstep(0.0f, 0.22f, d2 - d1);
    float  cell = hash12(best);
    float3 fill = float3(0.020f, 0.028f, 0.050f) + uAccent.rgb * (0.06f + 0.20f * cell);
    return float4(lerp(uAccent.rgb * 0.85f, fill, edge), 1.0f);
}
)HLSL";

// Droplets running down the glass, leaving trails - reads as the blur behind being refracted.
inline const char* const RAIN_HLSL = R"HLSL(
float4 ps_main(VS_OUT i) : SV_Target {
    float2 uv = i.uv;
    float  t  = uTime;
    float3 base = float3(0.028f, 0.036f, 0.062f);
    float  drops = 0.0f;

    [unroll] for (int k = 0; k < 3; ++k) {
        float fk   = (float)k;
        float cols = 14.0f + fk * 9.0f;
        float gx   = uv.x * uAspect * cols;
        float ci   = floor(gx);
        float cf   = frac(gx) - 0.5f;

        float seed  = hash11(ci + fk * 31.7f);
        float y     = frac(uv.y + t * (0.18f + seed * 0.35f) + seed);
        float head  = exp(-abs(y) * 38.0f) + exp(-abs(y - 1.0f) * 38.0f);
        float trail = exp(-y * 5.0f) * 0.45f;
        float w     = exp(-abs(cf) * (22.0f - fk * 4.0f));

        drops += (head + trail) * w * (0.90f - fk * 0.20f);
    }

    float3 col = base;
    col += uAccent.rgb * drops * 0.55f;
    col += pow(drops, 3.0f) * 0.18f;                                  // specular on the heads
    col += base * fbm(uv * float2(uAspect, 1.0f) * 4.0f + t * 0.05f); // fogging on the pane
    return float4(col, 1.0f);
}
)HLSL";

// Rings thrown out from wherever the menu was last clicked, over a slow accent wash that also
// glows under the pointer - so it has something to say between clicks.
inline const char* const RIPPLE_HLSL = R"HLSL(
float4 ps_main(VS_OUT i) : SV_Target {
    float2 p = float2(i.uv.x * uAspect, i.uv.y);
    float  t = uTime;

    float3 col = lerp(float3(0.020f, 0.028f, 0.050f), uAccent.rgb * 0.16f,
                      0.45f + 0.35f * fbm(p * 2.2f + t * 0.08f));

    if (uClickAge < 2.4f) {
        float2 c    = float2(uClick.x * uAspect, uClick.y);
        float  r    = uClickAge * 0.85f;
        float  fade = saturate(1.0f - uClickAge / 2.4f);
        float  d    = length(p - c);

        [unroll] for (int k = 0; k < 3; ++k) {
            float rr   = r - (float)k * 0.075f;
            float ring = exp(-pow((d - rr) * 26.0f, 2.0f));
            col += uAccent.rgb * ring * fade * (0.85f - (float)k * 0.22f);
        }
    }

    float2 m = float2(uMouse.x * uAspect, 1.0f - uMouse.y);
    col += uAccent.rgb * exp(-length(p - m) * 5.5f) * 0.18f;
    return float4(col, 1.0f);
}
)HLSL";

// Liquid glass: metaballs merged with a smooth union, shaded from the gradient of their own
// distance field - refraction offset, chromatic split and a specular along the rim.
inline const char* const GLASS_HLSL = R"HLSL(
// Diagonal accent bands under the noise: the lens needs something with structure behind it, or
// the refraction has nothing to bend and the whole thing reads as a flat wash.
float3 backdrop(float2 q, float t) {
    float bands = 0.5f + 0.5f * sin((q.x * 2.4f + q.y * 1.3f) * 6.0f - t * 0.8f);
    float f     = fbm(q * 2.4f + float2(t * 0.10f, -t * 0.07f));
    float3 lit  = uAccent.rgb * (0.30f + 0.55f * bands);
    return lerp(float3(0.016f, 0.022f, 0.044f), lit, saturate(f * 1.35f));
}

float blobField(float2 q, float t) {
    float d = 1.0e9f;
    [unroll] for (int k = 0; k < 7; ++k) {
        float  fk = (float)k;
        float2 c  = float2(uAspect * (0.5f + 0.36f * sin(t * (0.39f + fk * 0.055f) + fk * 1.7f)),
                                      0.5f + 0.31f * cos(t * (0.33f + fk * 0.071f) + fk * 2.4f));
        float  r  = 0.085f + 0.055f * sin(t * 0.55f + fk * 2.1f);
        float  di = length(q - c) - r;

        float h = saturate(0.5f + 0.5f * (d - di) / 0.16f); // smooth union
        d = lerp(d, di, h) - 0.16f * h * (1.0f - h);
    }
    return d;
}

float4 ps_main(VS_OUT i) : SV_Target {
    float2 q = float2(i.uv.x * uAspect, i.uv.y);
    float  t = uTime * 0.5f;

    float d = blobField(q, t);
    float e = 1.5f / uResolution.y;
    float2 n = float2(blobField(q + float2(e, 0.0f), t) - blobField(q - float2(e, 0.0f), t),
                      blobField(q + float2(0.0f, e), t) - blobField(q - float2(0.0f, e), t)) / (2.0f * e);

    float  inside = smoothstep(0.012f, -0.012f, d);
    float  rim    = smoothstep(0.035f, 0.0f, abs(d));
    float2 off    = -n * 0.10f * inside;

    // the chromatic split is the whole reason this reads as glass rather than as a blob
    float3 col;
    col.r = backdrop(q + off * 1.16f, t).r;
    col.g = backdrop(q + off,         t).g;
    col.b = backdrop(q + off * 0.84f, t).b;

    float spec = pow(saturate(dot(normalize(n + 1.0e-5f), normalize(float2(-0.55f, 0.83f)))), 16.0f) * rim;
    col += spec * 0.85f;
    col += uAccent.rgb * rim * 0.45f;
    col  = lerp(col, col * 1.18f + uAccent.rgb * 0.06f, inside);
    return float4(col, 1.0f);
}
)HLSL";


// ---------------------------------------------------------------------------------------------
// Runtime
// ---------------------------------------------------------------------------------------------

class CustomEffect {
public:
    struct FBO {
        ID3D11Texture2D*          tex = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11RenderTargetView*   rtv = nullptr;
        void Release( ) {
            if ( srv ) { srv->Release( ); srv = nullptr; }
            if ( rtv ) { rtv->Release( ); rtv = nullptr; }
            if ( tex ) { tex->Release( ); tex = nullptr; }
        }
    };

    FBO outputFBO;

    explicit CustomEffect( const char* body ) : body( body ) { }

    // Matching the vendored effects' signature exactly, so both kinds share one dispatch table.
    void Update( float dt_sec, ImVec2 winSize, ImVec2 winPos ) {
        if ( !initialized )
            Init( );
        if ( !vs || !ps || winSize.x < 1.f || winSize.y < 1.f )
            return;

        Resize( ( int )winSize.x, ( int )winSize.y );
        if ( !outputFBO.rtv )
            return;

        timeAcc += dt_sec;

        const ImVec2 mp = ImGui::GetMousePos( );
        if ( ImGui::IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem ) ) {
            mouse[ 0 ] = ( mp.x - winPos.x ) / winSize.x;
            mouse[ 1 ] = 1.f - ( mp.y - winPos.y ) / winSize.y;
        }

        CustomProps props{ };
        props.uResolution[ 0 ] = ( float )width;
        props.uResolution[ 1 ] = ( float )height;
        props.uMouse[ 0 ] = mouse[ 0 ];
        props.uMouse[ 1 ] = mouse[ 1 ];
        props.uAccent[ 0 ] = g_custom_accent[ 0 ];
        props.uAccent[ 1 ] = g_custom_accent[ 1 ];
        props.uAccent[ 2 ] = g_custom_accent[ 2 ];
        props.uAccent[ 3 ] = g_custom_accent[ 3 ];
        props.uTime = timeAcc;
        props.uAspect = ( float )width / ( float )height;
        props.uClickAge = g_custom_click_age;
        props.uClick[ 0 ] = g_custom_click[ 0 ];
        props.uClick[ 1 ] = g_custom_click[ 1 ];

        ID3D11DeviceContext* ctx = g_pd3dDeviceContext;
        ctx->UpdateSubresource( cbProps, 0, nullptr, &props, 0, 0 );

        D3D11_VIEWPORT vp{ };
        vp.Width = ( float )width;
        vp.Height = ( float )height;
        vp.MaxDepth = 1.f;

        // The pipeline is whatever ImGui left bound last frame, so blending and depth are set
        // explicitly - an effect drawn through an alpha blend would composite onto the previous
        // frame instead of replacing it.
        const float blend_factor[ 4 ] = { 0.f, 0.f, 0.f, 0.f };
        ctx->OMSetRenderTargets( 1, &outputFBO.rtv, nullptr );
        ctx->RSSetViewports( 1, &vp );
        ctx->OMSetBlendState( blendOff, blend_factor, 0xffffffff );
        ctx->OMSetDepthStencilState( depthOff, 0 );
        ctx->IASetInputLayout( nullptr );
        ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        ctx->VSSetShader( vs, nullptr, 0 );
        ctx->PSSetShader( ps, nullptr, 0 );
        ctx->GSSetShader( nullptr, nullptr, 0 );
        ctx->VSSetConstantBuffers( 0, 1, &cbProps );
        ctx->PSSetConstantBuffers( 0, 1, &cbProps );
        ctx->Draw( 3, 0 );
    }

private:
    void Init( ) {
        initialized = true; // one attempt: a shader that will not compile must not retry every frame
        if ( !g_pd3dDevice )
            return;

        const std::string src = std::string( CUSTOM_HLSL_PROLOGUE ) + body;
        if ( !Compile( src, "vs_main", "vs_5_0" ) || !Compile( src, "ps_main", "ps_5_0" ) )
            return;

        D3D11_BUFFER_DESC bd{ };
        bd.ByteWidth = sizeof( CustomProps );
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        g_pd3dDevice->CreateBuffer( &bd, nullptr, &cbProps );

        D3D11_BLEND_DESC bl{ };
        bl.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        g_pd3dDevice->CreateBlendState( &bl, &blendOff );

        D3D11_DEPTH_STENCIL_DESC ds{ };
        g_pd3dDevice->CreateDepthStencilState( &ds, &depthOff );
    }

    bool Compile( const std::string& src, const char* entry, const char* target ) {
        ID3DBlob* code = nullptr;
        ID3DBlob* err = nullptr;
        const HRESULT hr = D3DCompile( src.c_str( ), src.size( ), nullptr, nullptr, nullptr,
            entry, target, 0, 0, &code, &err );
        if ( err ) {
            OutputDebugStringA( ( const char* )err->GetBufferPointer( ) );
            err->Release( );
        }
        if ( FAILED( hr ) || !code )
            return false;

        bool ok = false;
        if ( target[ 0 ] == 'v' )
            ok = SUCCEEDED( g_pd3dDevice->CreateVertexShader( code->GetBufferPointer( ), code->GetBufferSize( ), nullptr, &vs ) );
        else
            ok = SUCCEEDED( g_pd3dDevice->CreatePixelShader( code->GetBufferPointer( ), code->GetBufferSize( ), nullptr, &ps ) );

        code->Release( );
        return ok;
    }

    void Resize( int w, int h ) {
        if ( width == w && height == h && outputFBO.rtv )
            return;
        width = w;
        height = h;

        outputFBO.Release( );

        D3D11_TEXTURE2D_DESC desc{ };
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        if ( FAILED( g_pd3dDevice->CreateTexture2D( &desc, nullptr, &outputFBO.tex ) ) )
            return;
        g_pd3dDevice->CreateShaderResourceView( outputFBO.tex, nullptr, &outputFBO.srv );
        g_pd3dDevice->CreateRenderTargetView( outputFBO.tex, nullptr, &outputFBO.rtv );
    }

    const char* body = nullptr;

    ID3D11VertexShader*      vs = nullptr;
    ID3D11PixelShader*       ps = nullptr;
    ID3D11Buffer*            cbProps = nullptr;
    ID3D11BlendState*        blendOff = nullptr;
    ID3D11DepthStencilState* depthOff = nullptr;

    int   width = 0, height = 0;
    float timeAcc = 0.f;
    float mouse[ 2 ] = { 0.5f, 0.5f };
    bool  initialized = false;
};

} // namespace shader
