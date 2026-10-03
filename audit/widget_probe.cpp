#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "examples/example_win32_directx9/gui.hpp"
#include <cstdio>
#include <cmath>
#include <cassert>
#include "ui_replacements.hpp"

int main()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(400, 300));
    ImGui::Begin("probe", nullptr, ImGuiWindowFlags_NoSavedSettings);
    auto* draw = ImGui::GetWindowDrawList();
    bool value = false;
    const int before_visible = draw->VtxBuffer.Size;
    ImGui::Checkbox("visible", &value);
    std::printf("visible_checkbox_vertices=%d\n", draw->VtxBuffer.Size - before_visible);
    ImGui::SetCursorScreenPos(ImVec2(20, 10000));
    const int before_clipped = draw->VtxBuffer.Size;
    ImGui::Checkbox("clipped", &value);
    std::printf("clipped_checkbox_vertices=%d\n", draw->VtxBuffer.Size - before_clipped);
    ImGui::SetCursorScreenPos(ImVec2(20, 80));
    const int before_hidden = draw->VtxBuffer.Size;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
    ImGui::Checkbox("alpha_zero", &value);
    int nonzero = 0;
    for (int i = before_hidden; i < draw->VtxBuffer.Size; ++i)
        nonzero += (draw->VtxBuffer[i].col & IM_COL32_A_MASK) != 0;
    ImGui::PopStyleVar();
    std::printf("alpha_zero_nontransparent_vertices=%d\n", nonzero);
    const int before_auto = draw->VtxBuffer.Size;
    draw->AddCircleFilled(ImVec2(20, 20), 7.5f, IM_COL32_WHITE);
    std::printf("auto_circle_vertices=%d\n", draw->VtxBuffer.Size - before_auto);
    const int before_fixed = draw->VtxBuffer.Size;
    draw->AddCircleFilled(ImVec2(20, 20), 7.5f, IM_COL32_WHITE, 256);
    std::printf("fixed_circle_vertices=%d\n", draw->VtxBuffer.Size - before_fixed);
    std::printf("lerp_250ms=%f\n", ImLerp(0.0f, 1.0f, 0.25f * 15.0f));
    std::printf("exp_250ms=%f\n", 1.0f - std::exp(-0.25f * 15.0f));
    ImGui::SetCursorScreenPos(ImVec2(20,10000));
    int before = draw->VtxBuffer.Size;
    auditfix::Toggle("fixed_clipped",&value,1,ImVec4(0.604f,0.596f,0.788f,1));
    assert(draw->VtxBuffer.Size == before);
    std::printf("fixed_clipped_vertices=%d\n",draw->VtxBuffer.Size-before);
    ImGui::SetCursorScreenPos(ImVec2(20,130));
    before = draw->VtxBuffer.Size;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,0);
    auditfix::Toggle("fixed_alpha_zero",&value,1,ImVec4(0.604f,0.596f,0.788f,1));
    for(int i=before;i<draw->VtxBuffer.Size;++i) assert((draw->VtxBuffer[i].col&IM_COL32_A_MASK)==0);
    ImGui::PopStyleVar();
    std::printf("fixed_alpha_zero_vertices=%d\n",draw->VtxBuffer.Size-before);
    const float once=auditfix::Approach(0,1,15,1);
    float many=0;
    for(int i=0;i<60;++i) many=auditfix::Approach(many,1,15,1.0f/60);
    assert(std::abs(once-many)<0.00001f);
    auditfix::SquircleFilled(draw,ImVec2(200,100),ImVec2(300,180),IM_COL32_WHITE,20);
    auditfix::Squircle(draw,ImVec2(200,100),ImVec2(300,180),IM_COL32_WHITE,20);
    ImGui::End();
    ImGui::Render();
    ImGui::DestroyContext();
}
