#pragma once

#include <d3d11.h>

#pragma comment ( lib, "d3d11.lib" )
#pragma comment ( lib, "dxgi.lib" )
#pragma comment ( lib, "d3dcompiler.lib" )

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

namespace blur {

    // Set once, after the device is created (main.cpp). The module owns nothing else global.
    inline ID3D11Device*        device  = nullptr;
    inline ID3D11DeviceContext* context = nullptr;

    void    on_device_reset( ); // drop every size-dependent resource (swap chain resize / lost device)
    void    shutdown( );
    HRESULT last_result( );
}

// Blurs whatever is already in the back buffer behind `draw`'s current clip rect and composites it
// back rounded. Both take effect where they are recorded in the draw list, so anything drawn after
// them sits on top of the blur, and anything before it is what gets blurred.
extern void draw_blur( ImDrawList* drawList, float alpha = 1.0f, float rounding = -1.0f );
extern void draw_blur_rounded( ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha = 1.0f, float rounding = -1.0f, ImColor tint = ImColor( 1.0f, 1.0f, 1.0f, 0.0f ) );
