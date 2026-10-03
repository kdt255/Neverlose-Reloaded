// Dear ImGui: standalone example application for DirectX 9
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define  IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <tchar.h>
#include <string>

#include "gui.hpp"
#include "hashes.hpp"
#include "blur.hpp"
#include "bytes.hpp"
#include "notification.hpp"

extern IMGUI_IMPL_API void ImGui_ImplDX9_InvalidateDeviceObjects();
extern IMGUI_IMPL_API bool ImGui_ImplDX9_CreateDeviceObjects();

static void RebuildFonts(float scale)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX9_InvalidateDeviceObjects();
    io.Fonts->Clear();

    const char* regular_candidates[] = {
        "PingFangSC-Regular.ttf",
        "PingFang SC Regular.ttf",
        "PingFang.ttc",
        "C:\\Windows\\Fonts\\PingFangSC-Regular.ttf",
        "C:\\Windows\\Fonts\\PingFang.ttc",
        "C:\\Windows\\Fonts\\seguisb.ttf"
    };
    const char* regular_font = nullptr;
    for (int i = 0; i < IM_ARRAYSIZE(regular_candidates); ++i) {
        if (::GetFileAttributesA(regular_candidates[i]) != INVALID_FILE_ATTRIBUTES) {
            regular_font = regular_candidates[i];
            break;
        }
    }

    ImFontConfig text_config;
    text_config.OversampleH = 3;
    text_config.PixelSnapH = true;
    text_config.RasterizerMultiply = 1.0f;
    text_config.FontBuilderFlags |= (1 << 3); // ImGuiFreeTypeBuilderFlags_LightHinting
    const ImWchar* text_ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    io.Fonts->AddFontFromFileTTF("SSTMedium.TTF", 17.0f * scale, &text_config);

    if (regular_font) {
        ImFontConfig pingfang_config;
        pingfang_config.MergeMode = true;
        pingfang_config.OversampleH = 3;
        pingfang_config.PixelSnapH = true;
        pingfang_config.RasterizerMultiply = 1.15f;
        pingfang_config.FontBuilderFlags |= (1 << 3);
        io.Fonts->AddFontFromFileTTF(regular_font, 17.0f * scale, &pingfang_config, text_ranges);
    }

    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 16.0f * scale, &icons_config, icon_ranges);

    io.Fonts->AddFontFromFileTTF("SSTBold.TTF", 30.0f * scale, &text_config);
    if (regular_font) {
        ImFontConfig pingfang_config;
        pingfang_config.MergeMode = true;
        pingfang_config.OversampleH = 3;
        pingfang_config.PixelSnapH = true;
        pingfang_config.RasterizerMultiply = 1.15f;
        io.Fonts->AddFontFromFileTTF(regular_font, 30.0f * scale, &pingfang_config, text_ranges);
    }

    ImFontConfig hexsync_config;
    hexsync_config.OversampleH = 3;
    hexsync_config.PixelSnapH = true;
    ImFont* hexsync_font = io.Fonts->AddFontFromFileTTF("SpaceGrotesk-SemiBold.ttf", 40.0f * scale, &hexsync_config);
    if (!hexsync_font)
        hexsync_font = io.FontDefault;

    io.Fonts->AddFontFromFileTTF("Inter-Medium.ttf", 17.0f * scale, &text_config);
    io.Fonts->AddFontFromFileTTF("Inter-SemiBold.ttf", 17.0f * scale, &text_config);
    io.Fonts->AddFontFromFileTTF("MuseoSansCyrl-700.ttf", 17.0f * scale, &text_config);

    io.Fonts->AddFontFromFileTTF("SSTBold.TTF", 12.0f * scale, &text_config);

    io.Fonts->Build();
    ImGui_ImplDX9_CreateDeviceObjects();
}

using namespace ImGui;

#define ALPHA    ( ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoBorder )
#define NO_ALPHA ( ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoBorder )

IDirect3DTexture9* avatar{ };
IDirect3DTexture9* bg{ };

// Data
static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX9 Example", WS_OVERLAPPEDWINDOW, 0, 0, 1920, 1080, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    RebuildFonts(1.0f);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ImGuiStyle default_style = ImGui::GetStyle();

    static float last_scale = 1.0f;

    // Main loop
    bool done = false;
    while (!done)
    {
        if (gui.m_scale != last_scale) {
            RebuildFonts(gui.m_scale);
            last_scale = gui.m_scale;
        }

        ImGui::GetStyle() = default_style;
        ImGui::GetStyle().ScaleAllSizes(gui.m_scale);
        // FontGlobalScale is NOT modified here because fonts are already rasterized at the correct size natively!

        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if ( !avatar )
            D3DXCreateTextureFromFileInMemoryEx( g_pd3dDevice, &esliboganet, sizeof esliboganet, 30, 30, D3DX_DEFAULT, 0,
                D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &avatar );

        if ( !bg )
            D3DXCreateTextureFromFileExA( g_pd3dDevice, "C:\\Users\\admin\\Downloads\\irwin.png", 1920, 1080, D3DX_DEFAULT, 0,
                D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &bg );

        blur::device = g_pd3dDevice;

        static bool bools[50]{};
        static int ints[50]{}, combo = 0;
        std::vector < const char* > items = { "Head", "Chest", "Stomach", "Legs" };
        static char buf[64];

        static float color[4] = { 1.f, 1.f, 1.f, 1.f };
        static ImVec2 gui_pos, gui_size;
        static float focus_anim = 0.f;
        bool any_popup_open = false;
        auto handle_popup_anim = [&](const char* popup_name, bool clicked, bool& out_wants_open) -> float {
            ImGuiID id = ImGui::GetID(popup_name);
            static std::unordered_map<ImGuiID, bool> wants_open;
            static std::unordered_map<ImGuiID, float> anims;
            
            if (clicked) {
                wants_open[id] = !wants_open[id];
                if (!wants_open[id]) anims[id] = 0.f; // Instant close
            }
            
            bool is_open = ImGui::IsPopupOpen(popup_name);
            if (wants_open[id] && !is_open && anims[id] > 0.8f) {
                wants_open[id] = false;
                anims[id] = 0.f; // Instant close
            }
            
            if (wants_open[id]) {
                anims[id] = ImLerp(anims[id], 1.f, ImGui::GetIO().DeltaTime * 12.f);
            } else {
                anims[id] = 0.f; // Force no exit animation
            }
            
            if (wants_open[id] && !is_open) {
                ImGui::OpenPopup(popup_name);
            }
            
            out_wants_open = wants_open[id];
            return anims[id];
        };

        auto center_popup = [&](const char* popup_name, ImVec2 default_size, float anim) {
            ImGuiContext& g = *GImGui;
            ImGuiID id = ImGui::GetID(popup_name);
            int my_idx = -1;
            for (int i = 0; i < g.OpenPopupStack.Size; i++) {
                if (g.OpenPopupStack[i].PopupId == id) {
                    my_idx = i;
                    break;
                }
            }
            if (my_idx != -1) {
                ImGuiWindow* win = g.OpenPopupStack[my_idx].Window;
                ImVec2 my_size = win ? win->Size : default_size;
                ImVec2 center = gui_pos + gui_size * 0.5f;
                ImVec2 target = center - my_size * 0.5f;
                
                target.y += (1.f - anim) * 20.f; // Slide down animation
                
                if (my_idx < g.OpenPopupStack.Size - 1) {
                    ImGuiWindow* child = g.OpenPopupStack[my_idx + 1].Window;
                    float child_w = child ? child->Size.x : 180.f;
                    target.x = center.x - child_w * 0.5f - my_size.x - 15.f;
                }
                
                static std::unordered_map<ImGuiID, ImVec2> pos_map;
                if (!win || win->Hidden) pos_map[id] = target + ImVec2(0, 15.f);
                
                pos_map[id] = ImLerp(pos_map[id], target, ImGui::GetIO().DeltaTime * 12.f);
                ImGui::SetNextWindowPos(pos_map[id], ImGuiCond_Always);
            }
        };

        PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

        ImGui::SetNextWindowSize( ImVec2( 800 * gui.m_scale, 600 * gui.m_scale ), ImGuiCond_Always );
        ImGui::Begin( "Hello, world!", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground ); {

            auto window = GetCurrentWindow( );
            auto draw   = window->DrawList;
            auto pos    = window->Pos;
            auto size   = window->Size;
            auto style  = GetStyle( );

            gui.m_anim = ImLerp( gui.m_anim, 1.f, 0.045f );

            GetBackgroundDrawList( )->AddImage( bg, ImVec2( 0, 0 ), io.DisplaySize );

            draw->AddText( io.Fonts->Fonts[1], io.Fonts->Fonts[1]->FontSize, pos + ImVec2( 170 / 2 - io.Fonts->Fonts[1]->CalcTextSizeA( io.Fonts->Fonts[1]->FontSize, FLT_MAX, 0, "NEVERLOSE" ).x / 2 + 1, 20 * gui.m_scale ), gui.accent_color.to_im_color( ), "NEVERLOSE" );
            draw->AddText( io.Fonts->Fonts[1], io.Fonts->Fonts[1]->FontSize, pos + ImVec2( 170 / 2 - io.Fonts->Fonts[1]->CalcTextSizeA( io.Fonts->Fonts[1]->FontSize, FLT_MAX, 0, "NEVERLOSE" ).x / 2, 20 ), GetColorU32( ImGuiCol_Text ), "NEVERLOSE" );

            //user
            {
                ImVec2 area_min = pos + ImVec2(0, size.y - 50);
                ImVec2 area_max = pos + ImVec2(170, size.y);
                bool is_hovered = ImGui::IsMouseHoveringRect(area_min, area_max);

                ImU32 chevron_color = is_hovered ? gui.text.to_im_color() : gui.text_disabled.to_im_color(0.8);

                // borders
                {
                    draw->AddLine(area_min, pos + ImVec2(170, size.y - 50), gui.border.to_im_color());

                    draw->AddLine(window->Pos + ImVec2(170, 60), window->Pos + ImVec2(window->Size.x, 61), gui.border.to_im_color());

                    window->DrawList->AddLine(window->Pos + ImVec2(170, 0), window->Pos + ImVec2(170, window->Size.y), gui.border.to_im_color());
                }


                draw->AddImageRounded(avatar, pos + ImVec2(15, size.y - 40), pos + ImVec2(45, size.y - 10), ImVec2(0, 0), ImVec2(1, 1), ImColor(1.f, 1.f, 1.f, 1.f), 100);

                draw->AddText(pos + ImVec2(55, size.y - 38), gui.text.to_im_color(), "kdt255");
                const char* t1 = "Till: ";
                draw->AddText(pos + ImVec2(55, size.y - 23), gui.text_disabled.to_im_color(), t1);
                draw->AddText(pos + ImVec2(55 + CalcTextSize(t1).x, size.y - 23), gui.accent_color.to_im_color(), "Lifetime");

                float chevron_width = CalcTextSize(ICON_FA_CHEVRON_RIGHT).x;
                float text_block_center_y = ((size.y - 40) + (size.y - 10)) * 0.5f - (CalcTextSize(ICON_FA_CHEVRON_RIGHT).y * 0.5f);

                draw->AddText(
                    pos + ImVec2(160 - chevron_width, text_block_center_y),
                    chevron_color,
                    ICON_FA_CHEVRON_RIGHT
                );

                static ImVec2 user_popup_pos;
                SetCursorPos(ImVec2(0, size.y - 50));
                bool clicked_user = InvisibleButton("##user_popup_btn", ImVec2(170, 50));
                bool user_btn_hovered = ImGui::IsItemHovered();
                static float user_btn_anim = 0.f;
                user_btn_anim = ImLerp(user_btn_anim, user_btn_hovered ? 1.f : 0.f, io.DeltaTime * 15.f);
                //if (user_btn_anim > 0.01f) {
                //    draw->AddRectFilled(pos + ImVec2(0, size.y - 50), pos + ImVec2(170, size.y), gui.frame_inactive.to_im_color(user_btn_anim * 0.5f));
                //}
                
                bool user_wants_open = false;
                float user_anim = handle_popup_anim("UserPopupAnim", clicked_user, user_wants_open);

                if (user_wants_open || user_anim > 0.01f) {
                    center_popup("UserPopupAnim", ImVec2(220, 250), user_anim);
                    ImGui::SetNextWindowSize(ImVec2(220, 0)); // fixed width, auto height
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, user_anim);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
                    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.f);
                    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.06f, 0.07f, 0.09f, 0.95f)); 
                    ImGui::PushStyleColor(ImGuiCol_Border, gui.border.to_im_color().Value);
                    
                    if (ImGui::BeginPopup("UserPopupAnim")) {
                        if (!user_wants_open && user_anim <= 0.01f) {
                            ImGui::CloseCurrentPopup();
                        }
                        
                        draw_blur(ImGui::GetWindowDrawList(), user_anim);
                        
                        ImFont* header_font = ImGui::GetIO().Fonts->Fonts[4];
                        ImGui::PushFont(header_font);
                        ImGui::TextColored(gui.text.to_vec4(1.f, false), "Profile");
                        ImGui::PopFont();
                        
                        ImVec2 header_p = ImGui::GetCursorScreenPos();
                        float w = ImGui::GetWindowWidth();
                        ImGui::GetWindowDrawList()->AddLine(header_p + ImVec2(-15, 4), header_p + ImVec2(w - 15, 4), gui.border.to_im_color(), 1.0f);
                        ImGui::Dummy(ImVec2(0, 10)); // gap after separator
                        
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddImageRounded(avatar, p, p + ImVec2(35, 35), ImVec2(0,0), ImVec2(1,1), ImColor(255,255,255), 100);
                        ImGui::GetWindowDrawList()->AddText(p + ImVec2(45, 0), gui.text.to_im_color(), "kdt255");
                        const char* pt1 = "Till: ";
                        ImGui::GetWindowDrawList()->AddText(p + ImVec2(45, 15), gui.text_disabled.to_im_color(), pt1);
                        ImGui::GetWindowDrawList()->AddText(p + ImVec2(45 + ImGui::CalcTextSize(pt1).x, 15), gui.accent_color.to_im_color(), "Lifetime");
                        
                        ImGui::Dummy(ImVec2(0, 45));
                        
                        auto draw_line = [&]() {
                            ImVec2 p = ImGui::GetCursorScreenPos();
                            ImGui::GetWindowDrawList()->AddLine(p + ImVec2(0, 2), p + ImVec2(190, 2), gui.border.to_im_color(0.5f));
                            ImGui::Dummy(ImVec2(0, 7));
                        };
                        
                        draw_line();
                        
                        static std::unordered_map<ImGuiID, float> item_anims;
                        auto user_item = [&](const char* icon, const char* label, const char* const items[], int items_count, int* current_item, bool is_gear, bool is_toggle, bool* toggle_val) {
                            ImVec2 p = ImGui::GetCursorScreenPos();
                            ImGuiID id = ImGui::GetID(label);
                            bool clicked = ImGui::InvisibleButton(label, ImVec2(190, 24));
                            bool h = ImGui::IsItemHovered();
                            
                            float& anim = item_anims[id];
                            anim = ImLerp(anim, h ? 1.f : 0.f, io.DeltaTime * 15.f);
                            
                            if (anim > 0.01f) {
                                ImGui::GetWindowDrawList()->AddRectFilled(p, p + ImVec2(190, 24), gui.frame_inactive.to_im_color(anim * 0.5f), 6);
                            }

                            ImU32 text_col = gui.text.to_im_color();
                            ImU32 text_dis = gui.text_disabled.to_im_color();
                            
                            float icon_y = (24.f - ImGui::CalcTextSize(icon).y) / 2.f;
                            float label_y = (24.f - ImGui::CalcTextSize(label).y) / 2.f;
                            ImGui::GetWindowDrawList()->AddText(p + ImVec2(5, icon_y), text_col, icon);
                            ImGui::GetWindowDrawList()->AddText(p + ImVec2(30, label_y), text_col, label);

                            if (items && current_item) {
                                std::string right_text = std::string(items[*current_item]) + " " + ICON_FA_CHEVRON_RIGHT;
                                float w = ImGui::CalcTextSize(right_text.c_str()).x;
                                float r_y = (24.f - ImGui::CalcTextSize(right_text.c_str()).y) / 2.f;
                                ImGui::GetWindowDrawList()->AddText(p + ImVec2(185 - w, r_y), (h) ? text_col : text_dis, right_text.c_str());
                                
                                std::string inner_popup_name = std::string("combo_") + label;
                                
                                bool inner_wants_open = false;
                                float inner_anim = handle_popup_anim(inner_popup_name.c_str(), clicked, inner_wants_open);

                                if (inner_wants_open || inner_anim > 0.01f) {
                                    ImGui::SetNextWindowSize(ImVec2(180, 0)); // fixed width, auto height
                                    center_popup(inner_popup_name.c_str(), ImVec2(180, 100), inner_anim);
                                    
                                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, inner_anim);
                                    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.f);
                                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
                                    ImGui::PushStyleColor(ImGuiCol_PopupBg, gui.group_box_bg.to_vec4(1.f, false));
                                    ImGui::PushStyleColor(ImGuiCol_Border, gui.border.to_vec4(1.f, false));

                                    if (ImGui::BeginPopup(inner_popup_name.c_str())) {
                                        if (!inner_wants_open && inner_anim <= 0.01f) {
                                            ImGui::CloseCurrentPopup();
                                        }
                                        draw_blur(ImGui::GetWindowDrawList(), inner_anim);
                                        
                                        ImFont* header_font = ImGui::GetIO().Fonts->Fonts[4];
                                        ImGui::PushFont(header_font);
                                        ImGui::TextColored(gui.text.to_vec4(1.f, false), "%s", label);
                                        ImGui::PopFont();
                                        
                                        ImVec2 header_p = ImGui::GetCursorScreenPos();
                                        float w_h = ImGui::GetWindowWidth();
                                        ImGui::GetWindowDrawList()->AddLine(header_p + ImVec2(-12, 4), header_p + ImVec2(w_h - 12, 4), gui.border.to_im_color(), 1.0f);
                                        ImGui::Dummy(ImVec2(0, 8)); // gap after separator
                                        
                                        for (int i = 0; i < items_count; i++) {
                                            if (ImGui::Selectable(items[i], *current_item == i, ImGuiSelectableFlags_DontClosePopups)) {
                                                *current_item = i;
                                            }
                                        }
                                        ImGui::EndPopup();
                                    }
                                    ImGui::PopStyleColor(2);
                                    ImGui::PopStyleVar(3);
                                }
                            } else if (is_gear) {
                                float cog_y = (24.f - ImGui::CalcTextSize(ICON_FA_COG).y) / 2.f;
                                ImGui::GetWindowDrawList()->AddText(p + ImVec2(150, cog_y), text_dis, ICON_FA_COG);
                                ImGui::GetWindowDrawList()->AddRectFilled(p + ImVec2(170, 4), p + ImVec2(190, 20), gui.accent_color.to_im_color(), 6);
                            } else if (is_toggle && toggle_val) {
                                float switch_w = 26, switch_h = 16;
                                ImVec2 sp = p + ImVec2(185 - switch_w, 4);
                                
                                ImGuiID toggle_id = ImGui::GetID((std::string(label) + "_toggle").c_str());
                                static std::unordered_map<ImGuiID, float> toggle_anims;
                                float& t_anim = toggle_anims[toggle_id];
                                t_anim = ImLerp(t_anim, *toggle_val ? 1.f : 0.f, io.DeltaTime * 15.f);
                                
                                ImVec4 c_off = ImGui::ColorConvertU32ToFloat4(ImColor(55, 60, 70));
                                ImVec4 c_on = ImGui::ColorConvertU32ToFloat4(gui.accent_color.to_im_color());
                                ImU32 bg_col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                                    c_off.x + (c_on.x - c_off.x) * t_anim,
                                    c_off.y + (c_on.y - c_off.y) * t_anim,
                                    c_off.z + (c_on.z - c_off.z) * t_anim,
                                    c_off.w + (c_on.w - c_off.w) * t_anim
                                ));
                                
                                ImGui::GetWindowDrawList()->AddRectFilled(sp, sp + ImVec2(switch_w, switch_h), bg_col, switch_h / 2);
                                
                                float circle_x = (switch_h / 2) + (switch_w - switch_h) * t_anim;
                                ImGui::GetWindowDrawList()->AddCircleFilled(sp + ImVec2(circle_x, switch_h/2), switch_h/2 - 2.5f, ImColor(255,255,255), 12);
                                
                                if (clicked) *toggle_val = !*toggle_val;
                            }
                            return clicked;
                        };
                        
                        static bool sync_val = false;
                        static int lang_idx = 0;
                        static int scale_idx = 1;
                        static int esp_idx = 0;
                        static int win_idx = 0;
                        static int unit_idx = 0;
                        static int safe_idx = 0;
                        
                        const char* langs[] = { "English", "Russian" };
                        const char* scales[] = { "75%", "100%", "125%", "150%", "175%", "200%" };
                        const char* autos[] = { "Auto", "Manual" };
                        const char* states[] = { "Disabled", "Enabled" };

                        //static int scale_idx = 1; // Default to 100%
                        
                        user_item(ICON_FA_GLOBE, "Language", langs, IM_ARRAYSIZE(langs), &lang_idx, false, false, nullptr);
                        user_item(ICON_FA_ARROWS_ALT_V, "Menu Scale", scales, IM_ARRAYSIZE(scales), &scale_idx, false, false, nullptr);
                        
                        // Map scale_idx to gui.m_scale
                        float scale_values[] = { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
                        gui.m_scale = scale_values[scale_idx];

                        user_item(ICON_FA_TEXT_HEIGHT, "ESP Scale", autos, IM_ARRAYSIZE(autos), &esp_idx, false, false, nullptr);
                        user_item(ICON_FA_CLONE, "Windows Scale", autos, IM_ARRAYSIZE(autos), &win_idx, false, false, nullptr);
                        user_item(ICON_FA_RULER_COMBINED, "Units", autos, IM_ARRAYSIZE(autos), &unit_idx, false, false, nullptr);
                        
                        draw_line();
                       
                        user_item(ICON_FA_SYNC, "Synchronization", nullptr, 0, nullptr, false, true, &sync_val);
                        
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar(3);
                }
            }

            SetCursorPos( ImVec2( 10, 70 ) );
            BeginChild( "##tabs", ImVec2( 150, size.y - 120 ) );
            static bool visuals_expanded = false;

            gui.group_title( "Aimbot" );
            if ( gui.tab( ICON_FA_CROSSHAIRS, "Rage", gui.m_tab == 0 ) && gui.m_tab != 0 ) {
                gui.m_tab = 0, gui.m_anim = 0.f;
                visuals_expanded = false;
            }

            if ( gui.tab( ICON_FA_MOUSE, "Legit", gui.m_tab == 1 ) && gui.m_tab != 1 ) {
                gui.m_tab = 1, gui.m_anim = 0.f;
                visuals_expanded = false;
            }

            Spacing( ), Spacing( ), Spacing( );

            gui.group_title("Common" );
            
            
            static float visuals_anim = 0.f;

            bool visuals_active = (gui.m_tab == 2 || gui.m_tab == 3 || gui.m_tab == 4);
            
            if ( gui.tab( ICON_FA_IMAGE, "Visuals", visuals_active ) ) {
                if ( !visuals_expanded ) {
                    visuals_expanded = true;
                    if (gui.m_tab != 3 && gui.m_tab != 4) {
                        gui.m_tab = 3; 
                        gui.m_anim = 0.f;
                    }
                }
            }

            visuals_anim = ImLerp(visuals_anim, visuals_expanded ? 1.f : 0.f, io.DeltaTime * 12.f);

            if ( visuals_anim > 0.01f ) {
                auto window = GetCurrentWindow();
                ImVec2 cursor_before = window->DC.CursorPos;
                float total_height = 2.f * (30.f + ImGui::GetStyle().ItemSpacing.y); // 2 tabs + spacing

                window->DrawList->PushClipRect(cursor_before, cursor_before + ImVec2(GetWindowWidth(), total_height * visuals_anim), true);
                
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * visuals_anim);
                
                if ( gui.tab( ICON_FA_USER, "Players", gui.m_tab == 3, 10.f ) && gui.m_tab != 3 )
                    gui.m_tab = 3, gui.m_anim = 0.f;

                if ( gui.tab( ICON_FA_GLOBE, "World", gui.m_tab == 4, 10.f ) && gui.m_tab != 4 )
                    gui.m_tab = 4, gui.m_anim = 0.f;
                    
                ImGui::PopStyleVar();
                window->DrawList->PopClipRect();
                
                window->DC.CursorPos.y = cursor_before.y + (total_height * visuals_anim);
            }

            if ( gui.tab( ICON_FA_SLIDERS_H, "Miscellaneous", gui.m_tab == 5 ) && gui.m_tab != 5 ) {
                gui.m_tab = 5, gui.m_anim = 0.f;
                visuals_expanded = false; // Auto-collapse
            }

            EndChild( );

            static std::vector<std::string> configs = { "112255", "legit cfg", "hvh master" };
            static int selected_cfg = 0;

            static bool show_preset_popup = false;
            SetCursorPos( ImVec2( 190, 15 ) );
            ImVec2 preset_btn_pos = ImGui::GetCursorScreenPos();
            
            ImGui::SetCursorScreenPos(preset_btn_pos);
            bool save_clicked = ImGui::InvisibleButton("##top_save_btn", ImVec2(35, 30));
            bool save_hovered = ImGui::IsItemHovered();
            
            ImGui::SetCursorScreenPos(preset_btn_pos + ImVec2(35, 0));
            bool drop_clicked = ImGui::InvisibleButton("##top_drop_btn", ImVec2(125, 30));
            bool drop_hovered = ImGui::IsItemHovered();
            
            if (save_clicked) {
                std::string current_cfg_name = (!configs.empty() && selected_cfg >= 0 && selected_cfg < configs.size()) ? configs[selected_cfg] : "None";
                if (current_cfg_name != "None") {
                    PushNotification("Config Saved", "Saved changes to " + current_cfg_name);
                }
            }
            
            bool preset_hovered = save_hovered || drop_hovered;
            static float preset_btn_anim = 0.f;
            preset_btn_anim = ImLerp(preset_btn_anim, preset_hovered ? 1.f : 0.f, io.DeltaTime * 15.f);
            
            draw->AddRectFilled(preset_btn_pos, preset_btn_pos + ImVec2(160, 30), gui.frame_inactive.to_im_color(0.5f + preset_btn_anim * 0.3f), 6);
            
            float save_icon_y = (30.f - ImGui::CalcTextSize(ICON_FA_SAVE).y) * 0.5f;
            draw->AddText(preset_btn_pos + ImVec2(12, save_icon_y), save_hovered ? gui.accent_color.to_im_color() : gui.text.to_im_color(), ICON_FA_SAVE);
            draw->AddLine(preset_btn_pos + ImVec2(35, 0), preset_btn_pos + ImVec2(35, 30), ImColor(255,255,255,30));
            
            std::string current_cfg_name = (!configs.empty() && selected_cfg >= 0 && selected_cfg < configs.size()) ? configs[selected_cfg] : "None";
            
            // Center text mathematically
            float cfg_text_w = ImGui::CalcTextSize(current_cfg_name.c_str()).x;
            float cfg_text_h = ImGui::CalcTextSize(current_cfg_name.c_str()).y;
            float text_x = 35.f + (110.f - cfg_text_w) / 2.f;
            float text_y = (30.f - cfg_text_h) / 2.f;
            draw->AddText(preset_btn_pos + ImVec2(text_x, text_y), gui.text.to_im_color(), current_cfg_name.c_str());
            
            ImVec2 chev_center = preset_btn_pos + ImVec2(145, 15);
            draw->AddLine(chev_center + ImVec2(-4, -2), chev_center + ImVec2(0, 2), drop_hovered ? gui.text.to_im_color() : gui.text_disabled.to_im_color(), 1.5f);
            draw->AddLine(chev_center + ImVec2(0, 2), chev_center + ImVec2(4, -2), drop_hovered ? gui.text.to_im_color() : gui.text_disabled.to_im_color(), 1.5f);
            
            bool preset_wants_open = false;
            float preset_anim = handle_popup_anim("PresetPopupAnim", drop_clicked, preset_wants_open);

            if (preset_wants_open || preset_anim > 0.01f) {
                ImGui::SetNextWindowSize(ImVec2(360, 0));
                
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, preset_anim);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
                ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.f);
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.06f, 0.07f, 0.09f, 0.95f)); 
                ImGui::PushStyleColor(ImGuiCol_Border, gui.border.to_vec4(1.f, false));
                
                if (ImGui::BeginPopup("PresetPopupAnim")) {
                    if (!preset_wants_open && preset_anim <= 0.01f) {
                        ImGui::CloseCurrentPopup();
                    }
                    draw_blur(ImGui::GetWindowDrawList(), preset_anim);
                    float popup_h = ImMin(350.f, 35.f + configs.size() * 32.f);
                    
                    ImVec2 popup_start_pos = ImGui::GetCursorScreenPos();
                    // Offset standard dropdown slightly down if desired, but native is best
                    ImGui::BeginChild("Presets.main", ImVec2(360, popup_h), false, ImGuiWindowFlags_NoScrollbar);
                    
                    ImGui::GetWindowDrawList()->AddRectFilled(popup_start_pos + ImVec2(0, 20), popup_start_pos + ImVec2(360, popup_h), gui.group_box_bg.to_im_color(), 6);
                    ImGui::GetWindowDrawList()->AddRect(popup_start_pos + ImVec2(0, 20), popup_start_pos + ImVec2(360, popup_h), gui.border.to_im_color(), 6);
                    
                    ImFont* header_font = ImGui::GetIO().Fonts->Fonts[4];
                    ImGui::GetWindowDrawList()->AddText(header_font, header_font->FontSize, popup_start_pos + ImVec2(5, 0), gui.text.to_im_color(), "Presets");
                    
                    static std::unordered_map<ImGuiID, float> preset_anims;
                    
                    auto icon_btn = [&](const char* id, const char* icon, float x_offset, bool blue = false) -> bool {
                        ImGui::SetCursorScreenPos(popup_start_pos + ImVec2(300.f - x_offset, -2.0f));
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImGuiID gid = ImGui::GetID(id);
                        bool clicked = ImGui::InvisibleButton(id, ImGui::CalcTextSize(icon));
                        bool h = ImGui::IsItemHovered();
                        float& anim = preset_anims[gid];
                        anim = ImLerp(anim, h ? 1.f : 0.f, io.DeltaTime * 15.f);
                        
                        ImU32 base_col = blue ? gui.accent_color.to_im_color() : gui.text_disabled.to_im_color();
                        ImVec4 c_base = ImGui::ColorConvertU32ToFloat4(base_col);
                        ImVec4 c_hov = ImGui::ColorConvertU32ToFloat4(gui.text.to_im_color());
                        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                            c_base.x + (c_hov.x - c_base.x) * anim,
                            c_base.y + (c_hov.y - c_base.y) * anim,
                            c_base.z + (c_hov.z - c_base.z) * anim,
                            c_base.w + (c_hov.w - c_base.w) * anim
                        ));
                        
                        ImGui::GetWindowDrawList()->AddText(p, col, icon);
                        return clicked;
                    };
                    
                    if (icon_btn("##up", ICON_FA_UPLOAD, 65, true)) {
                        PushNotification("Cloud", "Config uploaded to cloud successfully");
                    }
                    if (icon_btn("##trash", ICON_FA_TRASH, 40, false)) {
                        if (!configs.empty() && selected_cfg >= 0 && selected_cfg < configs.size()) {
                            std::string deleted_name = configs[selected_cfg];
                            configs.erase(configs.begin() + selected_cfg);
                            selected_cfg = ImMax(0, selected_cfg - 1);
                            PushNotification("Config Deleted", "Removed config: " + deleted_name);
                        }
                    }
                    if (icon_btn("##plus", ICON_FA_PLUS, 15, false)) {
                        configs.push_back("new config " + std::to_string(configs.size() + 1));
                        selected_cfg = configs.size() - 1;
                        PushNotification("Config Created", "Created new config");
                    }
                    
                    ImGui::SetCursorPos(ImVec2(12, 21));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 10));
                    ImGui::BeginChild("Presets", ImVec2(320 - 24, popup_h - 21), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysUseWindowPadding);
                    ImGui::BeginGroup();
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, gui.m_anim);
                    
                    static int renaming_cfg = -1;
                    static char rename_buf[64] = "";
                    
                    for (int i = 0; i < configs.size(); ++i) {
                        bool is_selected = (selected_cfg == i);
                        ImVec2 item_pos = ImGui::GetCursorScreenPos();
                        
                        ImGuiID row_id = ImGui::GetID((std::string("row_") + std::to_string(i)).c_str());
                        float& row_anim = preset_anims[row_id];
                        
                        if (ImGui::InvisibleButton((std::string("##cfg") + std::to_string(i)).c_str(), ImVec2(230, 30))) {
                            selected_cfg = i;
                            PushNotification("Config Loaded", "Loaded config: " + configs[i]);
                        }
                        bool h = ImGui::IsItemHovered();
                        row_anim = ImLerp(row_anim, h || is_selected ? 1.f : 0.f, io.DeltaTime * 15.f);
                        
                        if (row_anim > 0.01f) {
                            ImGui::GetWindowDrawList()->AddRectFilled(item_pos, item_pos + ImVec2(296, 30), gui.frame_inactive.to_im_color(row_anim * 0.5f), 6);
                        }
                        if (is_selected) {
                            ImGui::GetWindowDrawList()->AddRect(item_pos, item_pos + ImVec2(296, 30), gui.accent_color.to_im_color(), 6);
                        }
                            
                        if (renaming_cfg == i) {
                            ImGui::SetCursorScreenPos(item_pos + ImVec2(10, 5)); // fixed height 20 so 5 is centered in 30
                            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                            ImGui::BeginChild("##rename_child", ImVec2(210, 20), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
                            ImGui::PushItemWidth(-1);
                            ImGui::PushID(i);
                            if (ImGui::InputText("", rename_buf, sizeof(rename_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                                std::string old_name = configs[i];
                                configs[i] = rename_buf;
                                renaming_cfg = -1;
                                PushNotification("Config Renamed", "Renamed " + old_name + " to " + rename_buf);
                            } else if (ImGui::IsItemDeactivated()) {
                                renaming_cfg = -1;
                            }
                            ImGui::PopID();
                            ImGui::PopItemWidth();
                            ImGui::EndChild();
                            ImGui::PopStyleVar();
                        } else {
                            ImU32 tcol = row_anim > 0.5f ? gui.text.to_im_color() : gui.text_disabled.to_im_color();
                            float t_y = (30.f - ImGui::CalcTextSize(configs[i].c_str()).y) / 2.f;
                            ImGui::GetWindowDrawList()->AddText(item_pos + ImVec2(10, t_y), tcol, configs[i].c_str());
                        }
                        
                        if (is_selected) {
                            ImVec2 dot_pos = item_pos + ImVec2(251, 5);
                            ImGui::SetCursorScreenPos(dot_pos);
                            if (ImGui::InvisibleButton((std::string("##dots") + std::to_string(i)).c_str(), ImVec2(20, 20))) {
                                renaming_cfg = i;
                                strcpy(rename_buf, configs[i].c_str());
                                ImGui::SetKeyboardFocusHere(-1);
                            }
                            
                            ImGuiID dot_id = ImGui::GetID((std::string("dot_") + std::to_string(i)).c_str());
                            float& dot_anim = preset_anims[dot_id];
                            dot_anim = ImLerp(dot_anim, ImGui::IsItemHovered() ? 1.f : 0.f, io.DeltaTime * 15.f);
                            
                            ImVec4 dc_base = ImGui::ColorConvertU32ToFloat4(gui.text_disabled.to_im_color());
                            ImVec4 dc_hov = ImGui::ColorConvertU32ToFloat4(gui.text.to_im_color());
                            ImU32 dot_col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                                dc_base.x + (dc_hov.x - dc_base.x) * dot_anim,
                                dc_base.y + (dc_hov.y - dc_base.y) * dot_anim,
                                dc_base.z + (dc_hov.z - dc_base.z) * dot_anim,
                                dc_base.w + (dc_hov.w - dc_base.w) * dot_anim
                            ));
                            
                            ImGui::GetWindowDrawList()->AddText(dot_pos + ImVec2(0, 2), dot_col, ICON_FA_ELLIPSIS_H);
                            
                            ImVec2 save_pos = item_pos + ImVec2(276, 5);
                            ImGui::SetCursorScreenPos(save_pos);
                            if (ImGui::InvisibleButton((std::string("##savecfg") + std::to_string(i)).c_str(), ImVec2(20, 20))) {
                                PushNotification("Config Saved", "Saved changes to " + configs[i]);
                            }
                            
                            ImGuiID save_id = ImGui::GetID((std::string("save_") + std::to_string(i)).c_str());
                            float& save_anim = preset_anims[save_id];
                            save_anim = ImLerp(save_anim, ImGui::IsItemHovered() ? 1.f : 0.f, io.DeltaTime * 15.f);
                            
                            ImU32 save_col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                                dc_base.x + (dc_hov.x - dc_base.x) * save_anim,
                                dc_base.y + (dc_hov.y - dc_base.y) * save_anim,
                                dc_base.z + (dc_hov.z - dc_base.z) * save_anim,
                                dc_base.w + (dc_hov.w - dc_base.w) * save_anim
                            ));
                            ImGui::GetWindowDrawList()->AddText(save_pos + ImVec2(2, 2), save_col, ICON_FA_SAVE);
                        }
                        
                        ImGui::SetCursorScreenPos(item_pos + ImVec2(0, 30));
                    }
                    ImGui::PopStyleVar(3);
                    ImGui::EndGroup();
                    ImGui::EndChild();
                    ImGui::EndChild();
                    
                    ImGui::EndPopup();
                }
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(3);
            }
            
            
            PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );

            PopStyleVar( );

            PushStyleVar( ImGuiStyleVar_Alpha, gui.m_anim );
            PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, 8 ) );

            SetCursorPos( ImVec2( 185, 81 - ( 5 * gui.m_anim ) ) );
            BeginChild( "##childs", ImVec2( size.x - 200, size.y - 96 ) );

            switch ( gui.m_tab ) {

            case 0: {

                ImVec2 start_pos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(start_pos);

                gui.group_box("MAIN", ImVec2( GetWindowWidth( ) / 2 - GetStyle( ).ItemSpacing.x / 2, 222 ) ); {

                    Checkbox( "Enabled", &bools[0] );
                    Separator();
                    Checkbox("Silent Aim", &bools[1]);
                    Separator();
                    Checkbox("Automatic Fire", &bools[2]);
                    Separator();
                    Checkbox("Aim Through Walls", &bools[3]);
                    Separator();
                    SliderInt("Field Of View", &ints[0], 0, 180);

                } gui.end_group_box( );

                gui.group_box("SELECTION", ImVec2(GetWindowWidth() / 2 - GetStyle().ItemSpacing.x / 2, GetWindowHeight() - 232 - GetStyle().ItemSpacing.y)); {

                    for (int i = 1; i < 16; ++i) {
                        Checkbox(std::to_string(i).c_str(), &bools[i]);

                        if (i != 15)
                            Separator();

                    }

                } gui.end_group_box();

                ImGui::SetCursorPos(ImVec2(start_pos.x + GetWindowWidth() / 2 + GetStyle().ItemSpacing.x / 2, start_pos.y));
                gui.group_box("OTHER", ImVec2(GetWindowWidth() / 2 - GetStyle().ItemSpacing.x / 2, 290 - GetStyle().ItemSpacing.y)); {

                    for ( int i = 1; i < 16; ++i ) {
                        Checkbox( std::to_string( i ).c_str( ), &bools[ i ] );

                        if ( i != 15 )
                            Separator( );

                    }

                } gui.end_group_box( );


                ImGui::SetCursorPos(ImVec2(start_pos.x + GetWindowWidth() / 2 + GetStyle().ItemSpacing.x / 2, start_pos.y + 290 + GetStyle().ItemSpacing.y));
                gui.group_box("ANTI-AIM", ImVec2(GetWindowWidth() / 2 - GetStyle().ItemSpacing.x / 2, GetWindowHeight() - 300 - GetStyle().ItemSpacing.y)); {

                    auto popup_logic = [&](const char* name, bool& is_clicked) {
                        std::string popup_name = std::string(name) + "Popup";
                        bool w_open = false;
                        float p_anim = handle_popup_anim(popup_name.c_str(), is_clicked, w_open);

                        if (w_open || p_anim > 0.01f) {
                            ImGui::SetNextWindowSize(ImVec2(240, 0));
                            center_popup(popup_name.c_str(), ImVec2(240, 100), p_anim);
                            
                            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, p_anim);
                            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.f);
                            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
                            ImGui::PushStyleColor(ImGuiCol_PopupBg, gui.group_box_bg.to_vec4(1.f, false));
                            ImGui::PushStyleColor(ImGuiCol_Border, gui.border.to_vec4(1.f, false));
                            
                            if (ImGui::BeginPopup(popup_name.c_str())) {
                                if (!w_open && p_anim <= 0.01f) {
                                    ImGui::CloseCurrentPopup();
                                }
                                draw_blur(ImGui::GetWindowDrawList(), p_anim);
                            
                                // Header
                                ImFont* header_font = ImGui::GetIO().Fonts->Fonts[4];
                                ImGui::PushFont(header_font);
                                ImGui::TextColored(gui.text.to_vec4(1.f, false), "%s", name);
                            ImGui::PopFont();
                            
                            // Separator
                            ImVec2 p = ImGui::GetCursorScreenPos();
                            float w = ImGui::GetWindowWidth();
                            ImGui::GetWindowDrawList()->AddLine(p + ImVec2(-12, 4), p + ImVec2(w - 12, 4), gui.border.to_im_color(), 1.0f);
                            ImGui::Dummy(ImVec2(0, 8)); // gap after separator
                            
                            ImGui::PushItemWidth(180);
                            ImGui::Combo("Mode", &combo, items.data(), items.size());
                            ImGui::SliderInt("Value", &ints[0], 0, 100, "%d");
                            ImGui::PopItemWidth();
                            
                            ImGui::EndPopup();
                        }
                        ImGui::PopStyleColor(2);
                        ImGui::PopStyleVar(3);
                    }
                    };

                    bool p_clk = gui.sub_menu("Pitch");
                    bool y_clk = gui.sub_menu("Yaw");
                    bool f_clk = gui.sub_menu("Freestanding");
                    bool m_clk = gui.sub_menu("Mouse Override");

                    popup_logic("Pitch", p_clk);
                    popup_logic("Yaw", y_clk);
                    popup_logic("Freestanding", f_clk);
                    popup_logic("Mouse Override", m_clk);

                } gui.end_group_box( );
            }
            break;

            case 1:

                gui.group_box("Baby", ImVec2( GetWindowWidth( ) / 2 - GetStyle( ).ItemSpacing.x / 2, GetWindowHeight( ) / 2 - GetStyle( ).ItemSpacing.y / 2 ) ); {

                } gui.end_group_box( );

                gui.group_box("Ad", ImVec2( GetWindowWidth( ) / 2 - GetStyle( ).ItemSpacing.x / 2, GetWindowHeight( ) / 2 - GetStyle( ).ItemSpacing.y / 2 ) ); {

                } gui.end_group_box( );

                SameLine( ), SetCursorPosY( 0 );

                gui.group_box("Non icon name", ImVec2( GetWindowWidth( ) / 2 - GetStyle( ).ItemSpacing.x / 2, GetWindowHeight( ) ) ); {

                } gui.end_group_box( );

                break;

            case 2:
                break;

            case 5: {
                // UI SETTINGS logic has been moved to User Profile popup
                break;
            }
            }

            EndChild( );

            PopStyleVar( 2 );

            ImGuiContext& g = *GImGui;
            any_popup_open = false;
            if (g.OpenPopupStack.Size > 0) {
                const char* root_popup_name = g.OpenPopupStack[0].Window ? g.OpenPopupStack[0].Window->Name : "";
                if (strncmp(root_popup_name, "##Combo_", 8) != 0) {
                    any_popup_open = true;
                }
            }
            focus_anim = ImLerp(focus_anim, any_popup_open ? 1.f : 0.f, io.DeltaTime * 6.f);
            
            gui_pos = window->Pos;
            gui_size = window->Size;
            
            if (focus_anim > 0.01f) {
                ImGui::SetCursorPos(ImVec2(0, 0));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
                
                ImGui::BeginChild("FocusOverlay", gui_size, false, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
                
                draw_blur(ImGui::GetWindowDrawList(), focus_anim);
                ImGui::GetWindowDrawList()->AddRectFilled(gui_pos, gui_pos + gui_size, ImColor(0.f, 0.f, 0.f, 0.15f * focus_anim), 10.f);
                
                ImGui::EndChild();
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor();
            }

        } ImGui::End( );

        PopStyleVar( );

        // Rendering
        DrawNotifications();
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x*clear_color.w*255.0f), (int)(clear_color.y*clear_color.w*255.0f), (int)(clear_color.z*clear_color.w*255.0f), (int)(clear_color.w*255.0f));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);

        // Handle loss of D3D9 device
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
