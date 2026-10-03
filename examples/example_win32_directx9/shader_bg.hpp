#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

// Animated HLSL backgrounds (shaders/*.h) rendered behind the menu's chrome.
//
// Each effect draws into its own offscreen target and hands back a shader resource view, so the
// menu just blits it under the panels. The panels are translucent, which is what lets the effect
// read through them - `opacity` scales that on top.
namespace shader_bg {

    struct state_t {
        bool  enabled  = false;
        int   selected = 0;
        float opacity  = 0.35f;
        float speed    = 1.0f;
        bool  blur_enabled = false;
        float blur_intensity = 8.f;
    };

    state_t&    state( );
    int         count( );
    int         builtin_count( ); // [0, builtin_count) are the vendored set, the rest are ours
    const char* name( int index );
    const char* icon( int index );

    // Renders the selected effect. Must be called once per frame from inside the menu window: the
    // effects ask ImGui whether that window is hovered to drive their mouse input.
    void update( ImVec2 size, ImVec2 pos );

    // The view to sample this frame, or null when nothing should be drawn (disabled, first frame,
    // or the size changed and the target is about to be recreated under us).
    void* view( ImVec2 size );

    // Composites the current frame's effect into `draw`. The effect is only ever rendered at the
    // menu's size, so every caller is mapped back through the menu's own UV space - a card sitting
    // over the menu continues the same image instead of restarting it. `alpha_scale` carries the
    // caller's own fade. Returns false when nothing was drawn.
    bool draw_into( ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float alpha_scale = 1.f );

    void shutdown( );
}

// Called by the menu chrome in imgui.cpp, between the blur and the panel fills.
void menu_shader_background( ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding );
