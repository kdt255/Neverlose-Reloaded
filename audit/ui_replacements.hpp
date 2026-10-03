#pragma once
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace auditfix {
inline float Approach(float current, float target, float rate, float dt) noexcept {
    if (!std::isfinite(current)) current = target;
    if (!std::isfinite(dt) || dt <= 0 || !std::isfinite(rate) || rate <= 0) return current;
    return current + (target-current) * -std::expm1(-rate*dt);
}

// Namespaced custom widget: does not replace the generic upstream Checkbox API.
inline bool Toggle(const char* label, bool* value, float scale, const ImVec4& accent) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems || !value || !label) return false;
    ImGuiContext& g = *GImGui;
    const float s = std::isfinite(scale) ? std::clamp(scale, 0.5f, 4.0f) : 1.0f;
    const ImGuiID id = window->GetID(label);
    const char* end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 text = ImGui::CalcTextSize(label, end);
    const float sw = 34*s, sh = 19*s;
    const float width = (std::max)(sw, ImGui::GetContentRegionAvail().x);
    const float height = (std::max)(sh, text.y) + 2*g.Style.FramePadding.y;
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, pos + ImVec2(width, height));
    ImGui::ItemSize(bb, g.Style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) { *value = !*value; ImGui::MarkItemEdited(id); }
    const ImGuiID key = ImHashStr("##toggle_animation", 0, id);
    ImGuiStorage* storage = window->DC.StateStorage;
    const float t = Approach(storage->GetFloat(key, *value ? 1.0f : 0.0f),
                             *value ? 1.0f : 0.0f, 12, g.IO.DeltaTime);
    storage->SetFloat(key, t);
    ImGui::RenderNavHighlight(bb, id);
    auto* draw = window->DrawList;
    if (hovered)
        draw->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImVec4(1,1,1,0.04f)), 4*s);
    const ImVec2 start(pos.x+width-sw, pos.y+(height-sh)*0.5f);
    draw->AddRectFilled(start, start+ImVec2(sw,sh),
        ImGui::GetColorU32(ImLerp(ImVec4(0.020f,0.035f,0.055f,1), accent, t)), sh*0.5f);
    draw->AddCircleFilled(start+ImVec2(sh*0.5f+(sw-sh)*t,sh*0.5f), sh*0.5f-2*s,
        ImGui::GetColorU32(ImLerp(ImVec4(0.498f,0.529f,0.557f,1),ImVec4(1,1,1,1),t)));
    const ImVec4 clip(pos.x, pos.y, (std::max)(pos.x,start.x-8*s), bb.Max.y);
    draw->AddText(g.Font,g.FontSize,pos+ImVec2(0,(height-text.y)*0.5f),
        ImGui::GetColorU32(ImLerp(ImVec4(0.588f,0.588f,0.588f,1),ImVec4(1,1,1,1),t)),
        label,end,0,&clip);
    return pressed;
}

// Same superellipse exponent and sample count; expensive functions run once.
inline void SquirclePath(ImDrawList* draw, ImVec2 lo, ImVec2 hi, float radius) {
    static const auto unit = [] {
        std::array<ImVec2,68> points{};
        for (int corner=0; corner<4; ++corner)
            for (int i=0; i<=16; ++i) {
                const float angle = (corner+i/16.0f)*IM_PI*0.5f;
                const float x=std::cos(angle), y=std::sin(angle);
                points[corner*17+i] = ImVec2(std::copysign(std::pow(std::abs(x),0.4f),x),
                                            std::copysign(std::pow(std::abs(y),0.4f),y));
            }
        return points;
    }();
    const float r=(std::min)(radius*1.5f,(std::min)(hi.x-lo.x,hi.y-lo.y)*0.5f);
    const ImVec2 centers[4]{{hi.x-r,hi.y-r},{lo.x+r,hi.y-r},{lo.x+r,lo.y+r},{hi.x-r,lo.y+r}};
    draw->PathClear();
    for(int i=0;i<68;++i) draw->PathLineTo(centers[i/17]+unit[i]*r);
}
inline void SquircleFilled(ImDrawList* draw, ImVec2 lo, ImVec2 hi, ImU32 color, float radius) {
    if (!draw || !(color&IM_COL32_A_MASK) || hi.x<=lo.x || hi.y<=lo.y) return;
    if (!(radius>0)) { draw->AddRectFilled(lo,hi,color); return; }
    SquirclePath(draw,lo,hi,radius); draw->PathFillConvex(color);
}
inline void Squircle(ImDrawList* draw, ImVec2 lo, ImVec2 hi, ImU32 color, float radius, float thickness=1) {
    if (!draw || !(color&IM_COL32_A_MASK) || hi.x<=lo.x || hi.y<=lo.y || !(thickness>0)) return;
    if (!(radius>0)) { draw->AddRect(lo,hi,color,0,0,thickness); return; }
    SquirclePath(draw,lo,hi,radius); draw->PathStroke(color,ImDrawFlags_Closed,thickness);
}
}
