#pragma once
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "hashes.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

namespace menu_fonts {
struct Fonts {
    ImFont* body{};
    ImFont* heading{};
    ImFont* logo{};
    ImFont* medium{};
    ImFont* semibold{};
    ImFont* museo{};
    ImFont* caption{};
};

inline ImFont* LoadFont(ImFontAtlas& atlas, const std::filesystem::path& path,
                        float pixels, bool merge=false, const ImWchar* ranges=nullptr) {
    ImFontConfig config;
    config.SizePixels=pixels;
    config.MergeMode=merge;
    config.OversampleH=3;
    config.PixelSnapH=true;
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(input) {
        const auto length=input.tellg();
        if(length>0 && length<=static_cast<std::streamoff>((std::numeric_limits<int>::max)())) {
            const int count=static_cast<int>(length);
            void* bytes=IM_ALLOC(static_cast<std::size_t>(count));
            if(bytes) {
                input.seekg(0);
                if(input.read(static_cast<char*>(bytes),count)) {
                    config.FontDataOwnedByAtlas=true;
                    return atlas.AddFontFromMemoryTTF(bytes,count,pixels,&config,ranges);
                }
                IM_FREE(bytes);
            }
        }
    }
    // Missing optional merge fonts do not create a new slot.
    return merge ? nullptr : atlas.AddFontDefault(&config);
}

// Use trusted packaged font files. Invoke outside a frame, while the device is ready.
// On false, do not render with this atlas; retry/recover or shut down.
// All previous ImFont pointers become invalid; consumers must use the new Fonts object.
inline bool RebuildFonts(float scale, const std::filesystem::path& assets, Fonts& fonts) {
    if(!std::isfinite(scale) || scale<0.5f || scale>4.0f) return false;
    auto& io=ImGui::GetIO();
    if(io.Fonts->Locked) return false;
    ImGui_ImplDX9_InvalidateDeviceObjects();
    io.FontDefault=nullptr;
    fonts={};
    io.Fonts->Clear();
    // Existing project selects stb_truetype; no ineffective FreeType-only flags.
    io.Fonts->FontBuilderIO=nullptr;
    const auto cjk=assets / L"PingFangSC-Regular.ttf";
    const auto* ranges=io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    fonts.body=LoadFont(*io.Fonts,assets/L"SSTMedium.TTF",14*scale);
    LoadFont(*io.Fonts,cjk,14*scale,true,ranges);
    static const ImWchar icons[]{ICON_MIN_FA,ICON_MAX_FA,0};
    LoadFont(*io.Fonts,assets/L"fa-solid-900.ttf",13*scale,true,icons);
    fonts.heading=LoadFont(*io.Fonts,assets/L"SSTBold.TTF",30*scale);
    LoadFont(*io.Fonts,cjk,30*scale,true,ranges);
    fonts.logo=LoadFont(*io.Fonts,assets/L"SpaceGrotesk-SemiBold.ttf",40*scale);
    fonts.medium=LoadFont(*io.Fonts,assets/L"Inter-Medium.ttf",14*scale);
    fonts.semibold=LoadFont(*io.Fonts,assets/L"Inter-SemiBold.ttf",14*scale);
    fonts.museo=LoadFont(*io.Fonts,assets/L"MuseoSansCyrl-700.ttf",14*scale);
    fonts.caption=LoadFont(*io.Fonts,assets/L"SSTBold.TTF",12*scale);
    io.FontDefault=fonts.body;
    return fonts.body && fonts.heading && fonts.logo && fonts.medium && fonts.semibold &&
           fonts.museo && fonts.caption && io.Fonts->Build() && ImGui_ImplDX9_CreateDeviceObjects();
}
}
