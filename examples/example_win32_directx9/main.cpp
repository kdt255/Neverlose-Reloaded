// Dear ImGui: standalone example application for DirectX 9
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define  IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <tchar.h>
#include <string>

#include "gui.hpp"
#include "hashes.hpp"
#include "blur.hpp"
#include "bytes.hpp"
#include "shader_bg.hpp"
#include "spotify.hpp"
#include "gifbg.hpp"
#include "notification.hpp"
#include <windows.h>
#include <shellapi.h>

extern IMGUI_IMPL_API void ImGui_ImplDX11_InvalidateDeviceObjects();
extern IMGUI_IMPL_API bool ImGui_ImplDX11_CreateDeviceObjects();

#pragma comment( lib, "windowscodecs.lib" )

static void RebuildFonts(float scale)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplDX11_InvalidateDeviceObjects();
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
    // Latin + Latin-1 + Latin Extended-A (Turkish ğ ş ı İ) + Cyrillic (Russian) - the menu's
    // languages. Without these only Latin-1 was baked and the translations drew as "?".
    static const ImWchar ui_ranges[] = { 0x0020, 0x00FF, 0x0100, 0x017F, 0x0400, 0x052F, 0 };
    // The row labels read heavier than the reference menu's, so the UI face is rasterized a
    // little thinner (same metrics, lighter stems).
    ImFontConfig ui_config = text_config;
    ui_config.OversampleH = 1;           // crisp stems (the 3x filter smeared them into a heavier look)
    ui_config.RasterizerMultiply = 1.0f;
    io.Fonts->AddFontFromFileTTF("Inter-Medium.ttf", 15.0f * scale, &ui_config, ui_ranges); // cap height matches SST 17

    if (regular_font) {
        ImFontConfig pingfang_config;
        pingfang_config.MergeMode = true;
        pingfang_config.OversampleH = 3;
        pingfang_config.PixelSnapH = true;
        pingfang_config.RasterizerMultiply = 1.15f;
        pingfang_config.FontBuilderFlags |= (1 << 3);
        io.Fonts->AddFontFromFileTTF(regular_font, 15.0f * scale, &pingfang_config, text_ranges);
    }

    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 14.0f * scale, &icons_config, icon_ranges);

    io.Fonts->AddFontFromFileTTF("SSTBold.TTF", 30.0f * scale, &text_config, ui_ranges);
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

    io.Fonts->AddFontFromFileTTF("Inter-Medium.ttf", 17.0f * scale, &text_config, ui_ranges);
    io.Fonts->AddFontFromFileTTF("Inter-SemiBold.ttf", 17.0f * scale, &text_config, ui_ranges);
    io.Fonts->AddFontFromFileTTF("MuseoSansCyrl-700.ttf", 17.0f * scale, &text_config, ui_ranges);

    io.Fonts->AddFontFromFileTTF("Inter-SemiBold.ttf", 12.5f * scale, &text_config, ui_ranges); // group titles

    // Caption face (profile sub-line, chevrons, small values). Baked at the size it is actually
    // drawn at: asking the 15px body font for 13px text is a scaled bitmap, which is what made
    // "Lifetime" look mushy next to the name.
    ImFontConfig caption_config = ui_config;
    io.Fonts->AddFontFromFileTTF("Inter-Medium.ttf", 13.0f * scale, &caption_config, ui_ranges);
    ImFontConfig caption_icons = icons_config; // MergeMode already set
    io.Fonts->AddFontFromFileTTF("fa-solid-900.ttf", 13.0f * scale, &caption_icons, icon_ranges);


    // Lyrics faces, appended so every existing font index stays put. Baked at the sizes they are
    // actually drawn at - the whole point of the lyrics panel is large, clean type, and a 13px
    // face stretched to 22 is the one thing that would ruin it.
    ImFontConfig lyrics_big = text_config;
    io.Fonts->AddFontFromFileTTF("Inter-SemiBold.ttf", 22.0f * scale, &lyrics_big, ui_ranges);   // 8: active line
    ImFontConfig lyrics_small = text_config;
    io.Fonts->AddFontFromFileTTF("Inter-Medium.ttf", 17.0f * scale, &lyrics_small, ui_ranges);   // 9: the rest

    io.Fonts->Build();
    ImGui_ImplDX11_CreateDeviceObjects();
}

using namespace ImGui;

#define ALPHA    ( ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoBorder )
#define NO_ALPHA ( ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoBorder )

ID3D11ShaderResourceView* avatar{ };
ID3D11ShaderResourceView* bg{ };

// Data. The device and the context are deliberately not static: shaders/*.h reach for them by
// extern, which is the contract that whole shader set is written against.
ID3D11Device*                   g_pd3dDevice = nullptr;
ID3D11DeviceContext*            g_pd3dDeviceContext = nullptr;
ID3D11RenderTargetView*         g_mainRenderTargetView = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------------------------------------------
// Texture loading. The D3D9 build used D3DX for this; D3DX11 is not shipped with the Windows SDK
// any more, so decoding goes through WIC (which is, and reads the embedded JPEG and the PNG on
// disk alike). Straight - not premultiplied - BGRA, because that is what ImGui's blend expects.
// ---------------------------------------------------------------------------------------------

static Microsoft::WRL::ComPtr<IWICImagingFactory> g_wic;

static bool EnsureWIC()
{
    if (g_wic)
        return true;
    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); // already-initialised is not an error here
    return SUCCEEDED(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(g_wic.ReleaseAndGetAddressOf())));
}

static bool CreateTextureFromDecoder(IWICBitmapDecoder* decoder, ID3D11ShaderResourceView** out)
{
    using Microsoft::WRL::ComPtr;

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
        return false;

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(g_wic->CreateFormatConverter(converter.GetAddressOf())))
        return false;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return false;

    UINT w = 0, h = 0;
    if (FAILED(converter->GetSize(&w, &h)) || w == 0 || h == 0)
        return false;

    std::vector<unsigned char> pixels((size_t)w * h * 4);
    if (FAILED(converter->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data())))
        return false;

    // Full mip chain: these are drawn far smaller than they are stored (a 30px avatar out of a
    // much bigger source), and a mipless downscale is exactly the sparkle D3DX's mips avoided.
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, nullptr, tex.GetAddressOf())))
        return false;

    g_pd3dDeviceContext->UpdateSubresource(tex.Get(), 0, nullptr, pixels.data(), w * 4, 0);

    if (FAILED(g_pd3dDevice->CreateShaderResourceView(tex.Get(), nullptr, out)))
        return false;

    g_pd3dDeviceContext->GenerateMips(*out);
    return true;
}

bool LoadTextureFromMemory(const void* data, size_t size, ID3D11ShaderResourceView** out) // not static: spotify.cpp uploads cover art through it
{
    if (!EnsureWIC() || !g_pd3dDevice || !data || size == 0)
        return false;

    Microsoft::WRL::ComPtr<IWICStream> stream;
    if (FAILED(g_wic->CreateStream(stream.GetAddressOf())))
        return false;
    if (FAILED(stream->InitializeFromMemory((WICInProcPointer)const_cast<void*>(data), (DWORD)size)))
        return false;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(g_wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand,
            decoder.GetAddressOf())))
        return false;

    return CreateTextureFromDecoder(decoder.Get(), out);
}

static bool LoadTextureFromFile(const wchar_t* path, ID3D11ShaderResourceView** out)
{
    if (!EnsureWIC() || !g_pd3dDevice || !path)
        return false;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(g_wic->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf())))
        return false;

    return CreateTextureFromDecoder(decoder.Get(), out);
}

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 0, 0, 1920, 1080, nullptr, nullptr, wc.hInstance, nullptr);

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
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    spotify::start(); // polls the Windows media session on its own thread from here on
    gifbg::start();   // decodes the player's background GIF on another

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
        gui.apply_style_colors(); // the reset above restores the dark style colors - re-apply the theme
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
            CleanupRenderTarget();
            blur::on_device_reset(); // its ping-pong targets are sized to the back buffer
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        static bool textures_tried = false;
        if ( !textures_tried ) {
            textures_tried = true; // one attempt: a missing file must not re-decode every frame
            LoadTextureFromMemory( esliboganet, sizeof esliboganet, &avatar );
            LoadTextureFromFile( L"C:\\Users\\admin\\Downloads\\this-menu-was-made-by-kdt255.png", &bg );
        }

        blur::device = g_pd3dDevice;
        blur::context = g_pd3dDeviceContext;

        static bool bools[50]{};
        static int ints[50]{}, combo = 0;
        std::vector < const char* > items = { "Head", "Chest", "Stomach", "Legs" };
        static char buf[64];

        static float color[4] = { 1.f, 1.f, 1.f, 1.f };
        static ImVec2 gui_pos, gui_size;
        static float focus_anim = 0.f;
        // Popup plumbing (click position, open/close animation, placement) lives in gui now -
        // see gui.popup_begin()/row_popup()/popup_position().

        PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );

        // Menu drag: ImGui's own window move snaps the window to the cursor every frame, so the
        // menu is flagged NoMove and dragged here instead - the cursor sets a target and the
        // window eases towards it, which gives the move its trailing, animated feel.
        static bool   menu_pos_ready = false;
        static bool   menu_dragging = false;
        static ImVec2 menu_pos, menu_target, menu_grab;
        if ( menu_pos_ready ) {
            if ( menu_dragging ) {
                if ( io.MouseDown[ 0 ] )
                    menu_target = io.MousePos - menu_grab;
                else
                    menu_dragging = false;
            }
            menu_pos = ImLerp( menu_pos, menu_target, 1.f - expf( -20.f * ImMin( io.DeltaTime, 0.05f ) ) );
            if ( ImLengthSqr( menu_target - menu_pos ) < 0.25f )
                menu_pos = menu_target; // settled - stop sub-pixel creeping
            ImGui::SetNextWindowPos( menu_pos, ImGuiCond_Always );
        }

        ImGui::SetNextWindowSize( ImVec2( 800 * gui.m_scale, 600 * gui.m_scale ), ImGuiCond_Always );
        // NoBringToFrontOnFocus: a click inside the menu used to focus it at EndFrame, which moved
        // it in front of an open popup *before that frame was rendered* - the popup spent its
        // closing frame behind the menu, then reappeared on top as it faded. The menu never has
        // to cover anything (popups and tooltips always belong above it), so it never reorders.
        ImGui::Begin( "Hello, world!", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus );
        {

            auto window = GetCurrentWindow( );
            auto draw   = window->DrawList;
            auto pos    = window->Pos;
            auto size   = window->Size;
            auto style  = GetStyle( );

            gui.m_anim = ImLerp( gui.m_anim, 1.f, 0.045f );

            GetBackgroundDrawList( )->AddImage( bg, ImVec2( 0, 0 ), io.DisplaySize );

            gui.name_plate( pos, 170.f ); // 170: the side panel's own width (imgui.cpp draws it unscaled)

            // Renders the background effect for this frame. It has to run with the menu window
            // current (the effects ask ImGui whether it is hovered) and before ImGui::Render(),
            // which is what lets the chrome record the view up in Begin() and still get this
            // frame's pixels out of it.
            shader_bg::update( size, pos );

            //user
            {
                // borders
                {
                    draw->AddLine(window->Pos + ImVec2(185, 60), window->Pos + ImVec2(window->Size.x - 15, 61), gui.border.to_im_color());

                    draw->AddLine(window->Pos + ImVec2(10, 60), window->Pos + ImVec2(window->Size.x - 640, 60), gui.border.to_im_color());

                    //window->DrawList->AddLine(window->Pos + ImVec2(170, 15), window->Pos + ImVec2(170, window->Size.y), gui.border.to_im_color());
                }
                gui.user_profile(pos + ImVec2(10, size.y - 48), pos + ImVec2(160, size.y - 8), avatar);
            }

            static bool ambience_open = false; // the Ambience screen, drawn after the tabs below

            SetCursorPos( ImVec2( 10, 70 ) );
            BeginChild( "##tabs", ImVec2( 150, size.y - 120 ) );
            static bool visuals_expanded = false;
            // The Ambience screen covers the tab it belongs to, so the sidebar underneath must not
            // take clicks while it is up (DisabledAlpha 1: blocked, but not greyed out).
            PushStyleVar( ImGuiStyleVar_DisabledAlpha, 1.f );
            BeginDisabled( ambience_open );

            gui.group_title( "AIMBOT" );
            if ( gui.tab( ICON_FA_CROSSHAIRS, "Rage", gui.m_tab == 0 ) && gui.m_tab != 0 ) {
                gui.m_tab = 0, gui.m_anim = 0.f;
                visuals_expanded = false;
            }

            if ( gui.tab( ICON_FA_MOUSE, "Legit", gui.m_tab == 1 ) && gui.m_tab != 1 ) {
                gui.m_tab = 1, gui.m_anim = 0.f;
                visuals_expanded = false;
            }

            Spacing( ), Spacing( ), Spacing( );

            gui.group_title("COMMON" );
            
            
            static float visuals_anim = 0.f;

            bool visuals_active = (gui.m_tab == 2 || gui.m_tab == 3 || gui.m_tab == 4 || gui.m_tab == 6);
            
            if ( gui.tab( ICON_FA_IMAGE, "Visuals", visuals_active ) ) {
                if ( !visuals_expanded ) {
                    visuals_expanded = true;
                    if (gui.m_tab != 3 && gui.m_tab != 4 && gui.m_tab != 6) {
                        gui.m_tab = 3; 
                        gui.m_anim = 0.f;
                    }
                }
            }

            visuals_anim = ImLerp(visuals_anim, visuals_expanded ? 1.f : 0.f, io.DeltaTime * 12.f);

            if ( visuals_anim > 0.01f ) {
                auto window = GetCurrentWindow();
                ImVec2 cursor_before = window->DC.CursorPos;
                float total_height = 3.f * (30.f + ImGui::GetStyle().ItemSpacing.y); // 3 tabs + spacing

                window->DrawList->PushClipRect(cursor_before, cursor_before + ImVec2(GetWindowWidth(), total_height * visuals_anim), true);
                
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * visuals_anim);
                
                if ( gui.tab( ICON_FA_USER, "Players", gui.m_tab == 3, 10.f ) && gui.m_tab != 3 )
                    gui.m_tab = 3, gui.m_anim = 0.f;

                if ( gui.tab( ICON_FA_GLOBE, "World", gui.m_tab == 4, 10.f ) && gui.m_tab != 4 )
                    gui.m_tab = 4, gui.m_anim = 0.f;

                if ( gui.tab( ICON_FA_MAGIC, "Shader", gui.m_tab == 6, 10.f ) && gui.m_tab != 6 )
                    gui.m_tab = 6, gui.m_anim = 0.f;
                    
                ImGui::PopStyleVar();
                window->DrawList->PopClipRect();
                
                window->DC.CursorPos.y = cursor_before.y + (total_height * visuals_anim);
            }

            if ( gui.tab( ICON_FA_SLIDERS_H, "Miscellaneous", gui.m_tab == 5 ) && gui.m_tab != 5 ) {
                gui.m_tab = 5, gui.m_anim = 0.f;
                visuals_expanded = false;
            }

            EndDisabled( );
            PopStyleVar( );
            EndChild( );

            static std::vector<config_entry> configs = {
                { "This",                       false, 6, 6 },
                { "Menu",                       false, 5, 5 },
                { "Is",                    false, 4, 4 },
                { "Created",                    false, 3, 3 },
                { "By",                    false, 2, 2 },
                { "kdt255",                    false, 1, 1 },
            };
            static int selected_cfg = 0;

            SetCursorPos( ImVec2( 190, 15 ) );
            ImVec2 preset_btn_pos = ImGui::GetCursorScreenPos();

            const bool has_cfg = !configs.empty() && selected_cfg >= 0 && selected_cfg < (int)configs.size();
            std::string current_cfg_name = has_cfg ? configs[selected_cfg].name : "None";

            // The dropdown is sized to the preset name (within limits) instead of clipping it.
            const ImVec2 cfg_text_sz = ImGui::CalcTextSize(current_cfg_name.c_str());
            const float  chevron_w   = ImGui::CalcTextSize(ICON_FA_CHEVRON_DOWN).x;
            const float  drop_w      = ImClamp(cfg_text_sz.x + chevron_w + 40.f, 125.f, 300.f);
            const float  btn_w       = 35.f + drop_w;

            ImGui::SetCursorScreenPos(preset_btn_pos);
            bool save_clicked = ImGui::InvisibleButton("##top_save_btn", ImVec2(35, 30));
            bool save_hovered = ImGui::IsItemHovered();

            ImGui::SetCursorScreenPos(preset_btn_pos + ImVec2(35, 0));
            bool drop_clicked = ImGui::InvisibleButton("##top_drop_btn", ImVec2(drop_w, 30));
            bool drop_hovered = ImGui::IsItemHovered();

            if (save_clicked && has_cfg) {
                PushNotification(gui.tr("Config Saved"), gui.trf("Saved changes to %s", current_cfg_name.c_str()));
            }

            bool preset_hovered = save_hovered || drop_hovered;
            static float preset_btn_anim = 0.f;
            preset_btn_anim = ImLerp(preset_btn_anim, preset_hovered ? 1.f : 0.f, io.DeltaTime * 15.f);

            draw->AddRectFilled(preset_btn_pos, preset_btn_pos + ImVec2(btn_w, 30), gui.frame_inactive.to_im_color(0.5f + preset_btn_anim * 0.3f), 6);

            float save_icon_y = (30.f - ImGui::CalcTextSize(ICON_FA_SAVE).y) * 0.5f;
            draw->AddText(preset_btn_pos + ImVec2(12, save_icon_y), save_hovered ? gui.accent_color.to_im_color() : gui.text.to_im_color(), ICON_FA_SAVE);
            draw->AddLine(preset_btn_pos + ImVec2(35, 0), preset_btn_pos + ImVec2(35, 30), gui.divider.to_im_color(1.f, false));

            // Centered in the space left of the chevron, clipped only if the name hit the cap
            const float label_area = drop_w - chevron_w - 24.f;
            float text_x = 35.f + 12.f + (label_area - ImMin(cfg_text_sz.x, label_area)) * 0.5f;
            float text_y = (30.f - cfg_text_sz.y) * 0.5f;
            draw->PushClipRect(preset_btn_pos + ImVec2(35, 0), preset_btn_pos + ImVec2(35 + 12.f + label_area, 30), true);
            draw->AddText(preset_btn_pos + ImVec2(text_x, text_y), gui.text.to_im_color(), current_cfg_name.c_str());
            draw->PopClipRect();

            draw->AddText(preset_btn_pos + ImVec2(btn_w - chevron_w - 12.f, text_y + 2), gui.text_disabled.to_im_color(), ICON_FA_CHEVRON_DOWN);
            ImGui::SameLine();
            Button("Demo Button");

            gui.config_popup("PresetPopupAnim", drop_clicked, configs, selected_cfg, preset_btn_pos + ImVec2(35, 36));

            const int search_tab = gui.search_bar(pos + ImVec2(size.x - 15.f * gui.m_scale, 15.f));
            if (search_tab >= 0 && search_tab != gui.m_tab) {
                gui.m_tab = search_tab;
                gui.m_anim = 0.f;
                visuals_expanded = search_tab == 3 || search_tab == 4;
            }
            
            
            PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0, 0 ) );

            PopStyleVar( );

            PushStyleVar( ImGuiStyleVar_Alpha, gui.m_anim );
            PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8, 8 ) );

            SetCursorPos( ImVec2( 185, 81 - ( 5 * gui.m_anim ) ) );
            BeginChild( "##childs", ImVec2( size.x - 200, size.y - 91 ), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );

            const auto draw_tab = [&](int tab) {
            static const char* tab_names[] = { "Rage", "Legit", "Visuals", "Players", "World", "Miscellaneous", "Shader" };
            gui.search_scope_begin(tab, tab >= 0 && tab < IM_ARRAYSIZE(tab_names) ? tab_names[tab] : "");
            switch ( tab ) {

            case 0: {

                gui.begin_grid(2);

                gui.grid_col(0);
                gui.grid_box("MAIN", k_auto_height); {

                    gui.row_checkbox( "Enabled", &bools[0] );
                    gui.row_checkbox("Silent Aim", &bools[1]);
                    gui.row_checkbox("Automatic Fire", &bools[2]);
                    gui.row_checkbox("Aim Through Walls", &bools[3]);
                    gui.row_begin(); SliderInt("Field Of View", &ints[0], 0, 180);
                    gui.info_tooltip("Field Of View", "Adjust the field of view in degrees.");

                } gui.end_group_box( );

                gui.grid_box("SELECTION", k_auto_height); { // sized to content - symmetric top/bottom padding

                    for (int i = 1; i < 7; ++i)
                        gui.row_checkbox(std::to_string(i).c_str(), &bools[i]);

                } gui.end_group_box();

                gui.grid_col(1);
                gui.grid_box("OTHER", k_auto_height); {

                    for ( int i = 1; i < 7; ++i )
                        gui.row_checkbox( std::to_string( i ).c_str( ), &bools[ i ] );

                } gui.end_group_box( );

                gui.grid_box("ANTI-AIM", k_auto_height); { // sized to content - symmetric top/bottom padding


                    gui.row_checkbox("Enabled", &bools[15]);

                    // These four rows share one body - only the content is written here,
                    // gui.row_popup() draws the row and owns the popup itself.
                    auto sub_menu_content = [&] {
                        gui.popup_section_begin("sec1", true); {
                            ImGui::Checkbox("Checkbox", &bools[20]);
                            gui.info_tooltip("Checkbox", "Click to enable or disable this option.");
                            ImGui::Combo("Combo", &combo, items.data(), items.size());
                            gui.info_tooltip("Combo", "Select one item from the list.");
                            static bool multi_selected[4] = { true, true, true, false };
                            ImGui::MultiCombo("Multi", multi_selected, items.data(), static_cast<int>(items.size()));
                            gui.info_tooltip("Multi", "Select one or more items. Click a selected item again to remove it.");
                        } gui.popup_section_end();

                        gui.popup_section_begin("sec2", true); {
                            ImGui::Checkbox(ICON_FA_RUNNING "Checkbox", &bools[21]);
                            gui.info_tooltip("Checkbox", "Click to enable or disable this option.");
                            ImGui::Checkbox(ICON_FA_AWARD "Checkbox", &bools[22]);
                            gui.info_tooltip("Nigger!", "Hello, this is info box!");
                            ImGui::Checkbox("Checkbox", &bools[23]);
                            gui.info_tooltip("Checkbox", "Click to enable or disable this option.");
                            ImGui::SliderInt("Slider", &ints[0], 0, 100, "%d");
                            gui.info_tooltip("Slider", "Drag the bar to adjust the value from 0 to 100.");
                        } gui.popup_section_end();
                    };

                    if ( gui.row_popup( "Pitch" ) )          { sub_menu_content(); gui.popup_end(); }
                    if ( gui.row_popup( "Yaw" ) )            { sub_menu_content(); gui.popup_end(); }
                    if ( gui.row_popup( "Freestanding" ) )   { sub_menu_content(); gui.popup_end(); }
                    if ( gui.row_popup( "Mouse Override" ) ) { sub_menu_content(); gui.popup_end(); }

                } gui.end_group_box( );

                gui.end_grid();
            }
            break;

            case 1: {

                // color picker examples
                static color_value kill_col   { ImVec4( 0.96f, 0.22f, 0.30f, 1.f ) };
                static color_value extra_col  { ImVec4( 1.00f, 0.22f, 0.30f, 1.f ), ImVec4( 1.00f, 0.58f, 0.66f, 1.f ), true };
                static color_value glow_col   { ImVec4( 0.32f, 0.49f, 0.99f, 1.f ) };
                static color_value thunder_col{ ImVec4( 0.66f, 0.80f, 1.00f, 1.f ) };
                static color_value rain_col   { ImVec4( 0.86f, 0.91f, 1.00f, 1.f ) };
                static color_value snow_col   { ImVec4( 0.95f, 0.95f, 0.97f, 1.f ) };
                static color_value layout_cols[ 5 ] = {
                    { ImVec4( 0.08f, 0.08f, 0.08f, 1.f ) }, { ImVec4( 0.00f, 0.00f, 0.00f, 1.f ) },
                    { ImVec4( 0.16f, 0.17f, 0.21f, 1.f ) }, { ImVec4( 0.32f, 0.49f, 0.99f, 1.f ) },
                    { ImVec4( 0.55f, 0.56f, 0.60f, 1.f ) } };
                static color_slot layout_slots[ 5 ] = {
                    { "Layout", &layout_cols[ 0 ] }, { "Background", &layout_cols[ 1 ] }, { "Border", &layout_cols[ 2 ] },
                    { "Accent", &layout_cols[ 3 ] }, { "Text", &layout_cols[ 4 ] } };
                static int spread_force = 20, spread_speed = 45;

                gui.begin_grid(2);

                gui.grid_col(0);
                gui.grid_box("Baby", -0.5f); { // half of column 0

                    gui.row_checkbox( "Kill Effect", &bools[ 30 ], &kill_col );   // swatch left of the toggle
                    gui.row_color( "Additional Colors", &extra_col );             // own row, gradient preset

                    gui.row_begin( );                                             // picker with extra elements
                    if ( gui.color_picker_begin( "##glow", &glow_col ) ) {
                        gui.popup_section_begin( "glow_fx", false ); {
                            ImGui::SliderInt( "Spread Force", &spread_force, 0, 100, "%d" );
                            ImGui::SliderInt( "Speed", &spread_speed, 0, 100, "%d" );
                        } gui.popup_section_end( );
                        gui.color_picker_end( );
                    }
                    ImGui::Checkbox( "Glow", &bools[ 31 ] );

                    // picker with option rows + separators, like the reference
                    static color_value name_col{ ImVec4( 1.f, 1.f, 1.f, 210.f / 255.f ) };
                    static int name_font = 0, name_case = 0;
                    static bool name_avatar = false;
                    const char* font_opts[] = { "Regular", "Bold", "Italic" };
                    const char* case_opts[] = { "Default", "Uppercase", "Lowercase" };
                    gui.row_begin( );
                    if ( gui.color_picker_begin( "##name", &name_col ) ) {
                        gui.popup_option( ICON_FA_FONT, "Font", font_opts, IM_ARRAYSIZE( font_opts ), &name_font );
                        gui.popup_option( ICON_FA_FONT_CASE, "Case", case_opts, IM_ARRAYSIZE( case_opts ), &name_case );
                        gui.popup_separator( );
                        gui.popup_toggle( ICON_FA_USER_CIRCLE, "Draw Avatar", &name_avatar );
                        gui.color_picker_end( );
                    }
                    ImGui::Checkbox( "Name", &bools[ 37 ] );

                } gui.end_group_box( );

                gui.grid_box("Ad"); { // fills the rest of column 0

                    // "..." next to a toggle: its own settings card, opened from the dots
                    static bool traj = false, traj_death = true;
                    static int traj_style = 0, traj_alpha = 80;
                    const char* traj_styles[] = { "Line", "Dots", "Arrows" };
                    gui.row_begin( );
                    if ( gui.row_options_begin( "##traj" ) ) {
                        gui.popup_option( ICON_FA_PROJECT_DIAGRAM, "Style", traj_styles, IM_ARRAYSIZE( traj_styles ), &traj_style );
                        gui.popup_toggle( ICON_FA_SKULL, "Show On Death", &traj_death );
                        gui.popup_separator( );
                        gui.popup_section_begin( "traj_fx", false ); {
                            ImGui::SliderInt( "Opacity", &traj_alpha, 0, 100, "%d" );
                        } gui.popup_section_end( );
                        gui.row_options_end( );
                    }
                    ImGui::Checkbox( "Grenade Trajectory", &traj );

                    if ( gui.row_popup( "Weather" ) ) {
                        gui.popup_section_begin( "wx_a", true ); {
                            ImGui::Checkbox( ICON_FA_WIND " Wind", &bools[ 32 ] );
                            ImGui::Checkbox( ICON_FA_TINT " Model Wetness", &bools[ 33 ] );
                        } gui.popup_section_end( );
                        gui.popup_section_begin( "wx_b", true ); {
                            gui.color_picker( "##thunder", &thunder_col );
                            ImGui::Checkbox( ICON_FA_BOLT " Thunder", &bools[ 34 ] );
                            gui.color_picker( "##rain", &rain_col );
                            ImGui::Checkbox( ICON_FA_CLOUD_SHOWERS " Rain", &bools[ 35 ] );
                            gui.color_picker( "##snow", &snow_col );
                            ImGui::Checkbox( ICON_FA_SNOWFLAKE " Snow", &bools[ 36 ] );
                        } gui.popup_section_end( );
                        gui.popup_end( );
                    }

                } gui.end_group_box( );

                gui.grid_col(1);
                gui.grid_box("Non icon name"); { // fills all of column 1

                    // multi color picker: one swatch, a chip per color on top of the card
                    gui.row_color( "Layout", layout_slots, IM_ARRAYSIZE( layout_slots ) );

                } gui.end_group_box( );

                gui.end_grid();

                break;
            }

            case 3: { // Visuals -> Players

                static int  side = 0; // 0 enemies, 1 friends, 2 esp items
                static bool enemy_on = false, offscreen = false, sounds = false, kill_fx = false, glow_on = false;
                static color_value off_col  { ImVec4( 1.00f, 1.00f, 1.00f, 1.f ) };
                static color_value snd_col  { ImVec4( 1.00f, 0.29f, 0.47f, 1.f ) };
                static color_value kill_col2{ ImVec4( 0.96f, 0.22f, 0.30f, 1.f ) };
                static color_value glow_col2{ ImVec4( 0.72f, 0.73f, 0.98f, 1.f ) };
                static color_value model_col{ ImVec4( 0.45f, 0.45f, 0.95f, 1.f ) };
                static color_value walls_col{ ImVec4( 0.36f, 0.90f, 0.90f, 1.f ) };
                static color_value shot_col { ImVec4( 0.80f, 0.86f, 0.86f, 1.f ) };
                static color_value hist_col { ImVec4( 0.85f, 0.85f, 0.87f, 1.f ) };
                static color_value rag_col  { ImVec4( 0.69f, 0.66f, 0.85f, 1.f ) };
                static int model_mode = 0, walls_mode = 0, shot_mode = 0, hist_mode = 0, rag_mode = 0;
                const char* model_modes[] = { "Disabled", "Flat", "Glow", "Chams" };

                // header: the active side opens up to show its name, the last button is "expand"
                const char* seg_icons[]  = { ICON_FA_USER, ICON_FA_USER_MINUS, ICON_FA_LIST_ALT };
                const char* seg_labels[] = { "Enemies", "Friends", "ESP Items" };
                const ImVec2 content_min = ImGui::GetCursorScreenPos( );
                const float  content_w   = ImGui::GetContentRegionAvail( ).x;
                // ImGui dismisses a popup on the press that lands outside it, before any of this
                // frame's widgets run - so the header and the handle draw from the state the card
                // had at the end of the last frame instead, or they would flicker on that press.
                static bool esp_open = false;
                // The ESP Items button opens a card; it never becomes "the side you are editing",
                // so the header keeps showing Enemies or Friends while the card is up - it only
                // stays lit for as long as the card does.
                int seg_active = side;
                const int seg_clicked = gui.icon_tabs( "##players_side", seg_icons, seg_labels,
                                                       IM_ARRAYSIZE( seg_icons ), &seg_active,
                                                       content_min + ImVec2( content_w, 0.f ), ICON_FA_EXPAND_ALT,
                                                       esp_open ? 2 : -1 );
                if ( seg_clicked == 0 || seg_clicked == 1 )
                    side = seg_clicked;

                gui.begin_grid( 2 );

                gui.grid_col( 0 );
                gui.grid_box( "ENEMY", k_auto_height ); {

                    gui.row_checkbox( "Enabled", &enemy_on );
                    gui.row_checkbox( "Offscreen Arrow", &offscreen, &off_col );
                    gui.row_checkbox( "Sounds", &sounds, &snd_col );

                } gui.end_group_box( );

                gui.grid_box( "ENEMY MODEL", k_auto_height ); {

                    gui.row_combo( "Player",       &model_col, &model_mode, model_modes, IM_ARRAYSIZE( model_modes ) );
                    gui.row_combo( "Behind Walls", &walls_col, &walls_mode, model_modes, IM_ARRAYSIZE( model_modes ) );
                    gui.row_combo( "On Shot",      &shot_col,  &shot_mode,  model_modes, IM_ARRAYSIZE( model_modes ) );
                    gui.row_combo( "History",      &hist_col,  &hist_mode,  model_modes, IM_ARRAYSIZE( model_modes ) );
                    gui.row_combo( "Ragdolls",     &rag_col,   &rag_mode,   model_modes, IM_ARRAYSIZE( model_modes ) );
                    gui.row_checkbox( "Kill Effect", &kill_fx, &kill_col2 );
                    gui.row_checkbox( "Glow", &glow_on, &glow_col2 );

                } gui.end_group_box( );

                // The card fills the right-hand column, so grab that column's geometry before the
                // grid is torn down.
                gui.grid_col( 1 );
                const float  esp_w   = gui.grid_width( );
                const ImVec2 esp_min = ImGui::GetCursorScreenPos( );
                gui.end_grid( );

                { // ESP Items: a card that rises out of the handle below it, editing the active side
                    static bool main_items[ 2 ][ 6 ]   = { { true, true, false, true, false, false },
                                                           { true, false, false, false, false, false } };
                    static bool flag_items[ 2 ][ 15 ]  = {};
                    static bool weapon_items[ 2 ][ 6 ] = { { true, false, false, false, false, false }, {} };
                    static bool aim_items[ 2 ][ 2 ]    = {};
                    const char* main_labels[]   = { "Bounding Box", "Name", "Distance", "Health Bar", "Ammo Bar", "Skeleton" };
                    const char* flag_labels[]   = { "Unarmored", "Defuser", "Blind", "Scoped", "Reload", "Immunity",
                                                    "Slowed", "Vulnerable", "Bomb", "Hostage", "Defuse", "Pin Pulled",
                                                    "Money", "Order Priority", "Delay" };
                    const char* weapon_labels[] = { "Text", "Icon", "Readiness Bar", "Bomb", "Defuser", "Taser" };
                    const char* aim_labels[]    = { "Hit Chance", "Hitboxes" };

                    // A handle at the bottom edge of the tab: the card grows upward out of it, and
                    // the header's list button opens the same card.
                    const float content_h = ImGui::GetWindowHeight( ) - ( content_min.y - ImGui::GetWindowPos( ).y );
                    const ImVec2 handle_size( 46.f * gui.m_scale, 18.f * gui.m_scale );
                    const ImVec2 handle_pos( IM_ROUND( esp_min.x + ( esp_w - handle_size.x ) * 0.5f ),
                                             IM_ROUND( content_min.y + content_h - handle_size.y ) );
                    ImGui::SetCursorScreenPos( handle_pos );
                    const ImGuiID handle_id = ImGui::GetID( "##esp_handle" );
                    const bool handle_clicked = ImGui::InvisibleButton( "##esp_handle", handle_size );
                    gui.chevron_handle( handle_id, handle_pos, handle_size, esp_open );

                    bool esp_wants = false;
                    const float esp_anim = gui.popup_animation( "##esp_items", seg_clicked == 2 || handle_clicked, esp_wants );
                    // Pinned by its top edge, just clear of the header bar: the list is taller than
                    // the column, so anchoring the bottom instead would push it over that bar. It
                    // runs past the menu's bottom edge, over the handle it came out of.
                    if ( gui.begin_popup_card_at( "##esp_items", esp_wants, esp_anim,
                                                  ImVec2( esp_min.x, content_min.y + 34.f * gui.m_scale ),
                                                  ImVec2( 0.f, 0.f ), esp_w / gui.m_scale, 12.f ) ) {

                        ImGui::PushID( side ); // Enemies and Friends keep their own set
                        ImFont* bold = ImGui::GetIO( ).Fonts->Fonts[ 4 ];
                        ImGui::PushFont( bold );
                        ImGui::TextUnformatted( gui.tr( "ESP Items" ) );
                        ImGui::PopFont( );
                        { // rule under the card's own title, then air before the first section
                            ImGui::Dummy( ImVec2( 0.f, 5.f * gui.m_scale ) );
                            const ImVec2 sp = ImGui::GetCursorScreenPos( );
                            const float  lw = ImGui::GetContentRegionAvail( ).x;
                            ImGui::GetWindowDrawList( )->AddLine( sp, sp + ImVec2( lw, 0.f ),
                                gui.popup_border.to_im_color( 0.55f ), 1.f * gui.m_scale );
                            ImGui::Dummy( ImVec2( lw, 11.f * gui.m_scale ) );
                        }

                        auto esp_section = [ & ]( const char* id, const char* title, const char* const* labels,
                                                  bool* values, int count ) {
                            gui.popup_section_begin( id, true, 10.f ); {
                                ImGui::PushStyleColor( ImGuiCol_Text, gui.text.to_vec4( 1.f, false ) );
                                ImGui::TextUnformatted( gui.tr( title ) );
                                ImGui::PopStyleColor( );
                                ImGui::Dummy( ImVec2( 0.f, 12.f * gui.m_scale ) );
                                gui.chips( id, labels, values, count );
                            } gui.popup_section_end( );
                        };

                        esp_section( "##esp_main",   "Main",   main_labels,   main_items[ side ],   IM_ARRAYSIZE( main_labels ) );
                        esp_section( "##esp_flags",  "Flags",  flag_labels,   flag_items[ side ],   IM_ARRAYSIZE( flag_labels ) );
                        esp_section( "##esp_weapon", "Weapon", weapon_labels, weapon_items[ side ], IM_ARRAYSIZE( weapon_labels ) );
                        esp_section( "##esp_aim",    "Aimbot", aim_labels,    aim_items[ side ],    IM_ARRAYSIZE( aim_labels ) );

                        ImGui::PopID( );
                        gui.end_popup_card( );
                    }

                    esp_open = ImGui::IsPopupOpen( ImGui::GetID( "##esp_items" ), ImGuiPopupFlags_None );
                }

                break;
            }

            case 4: { // Visuals -> World

                static int  unlock_spec = 0, visual_recoil = 0, radar = 0, scope_overlay = 0;
                static bool inacc = false, prox_warn = false, traj_on = false, traj_death = true;
                static int  traj_style = 0, traj_alpha = 80;
                static color_value inacc_col{ ImVec4( 0.62f, 0.64f, 0.70f, 1.f ) };
                const char* select_items[]  = { "Select", "Always", "In Air", "On Key" };
                const char* overlay_items[] = { "Default", "Classic", "Minimal" };
                const char* traj_styles[]   = { "Line", "Dots", "Arrows" };

                // one shared body for the sub-menu rows of this tab
                auto world_popup = [ & ] {
                    static bool p_enabled = true, p_outline = false;
                    static int  p_style = 0;
                    static int  p_size = 45;
                    const char* p_styles[] = { "Default", "Filled", "Outlined" };
                    gui.popup_section_begin( "wp", true ); {
                        ImGui::Checkbox( "Enabled", &p_enabled );
                        ImGui::Combo( "Style", &p_style, p_styles, IM_ARRAYSIZE( p_styles ) );
                        ImGui::Checkbox( "Outline", &p_outline );
                    } gui.popup_section_end( );
                    gui.popup_section_begin( "wp2", false ); {
                        ImGui::SliderInt( "Size", &p_size, 0, 100, "%d" );
                    } gui.popup_section_end( );
                };

                // Twelve rows per column: the reference's rhythm, with the box padding trimmed
                // instead of the rows (5px spacing read as cramped).
                gui.row_spacing = 7.f;
                gui.box_pad_y = 6.f;
                gui.begin_grid( 2 );

                gui.grid_col( 0 );
                gui.grid_box( "VIEW", k_auto_height ); {

                    if ( gui.row_popup( "View Options" ) )        { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Scope Options" ) )       { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Viewmodel Options" ) )   { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Perspective Options" ) ) { world_popup( ); gui.popup_end( ); }
                    gui.row_begin( ); ImGui::Combo( "Unlock Spectating", &unlock_spec, select_items, IM_ARRAYSIZE( select_items ) );
                    gui.row_begin( ); ImGui::Combo( "Visual Recoil", &visual_recoil, select_items, IM_ARRAYSIZE( select_items ) );

                } gui.end_group_box( );

                gui.grid_box( "WORLD ESP", k_auto_height ); {

                    if ( gui.row_popup( "Bomb" ) )     { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Weapons" ) )  { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Hostages" ) ) { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Grenades" ) ) { world_popup( ); gui.popup_end( ); }

                    gui.row_begin( );
                    if ( gui.row_options_begin( "##traj_world" ) ) {
                        gui.popup_option( ICON_FA_PROJECT_DIAGRAM, "Style", traj_styles, IM_ARRAYSIZE( traj_styles ), &traj_style );
                        gui.popup_toggle( ICON_FA_SKULL, "Show On Death", &traj_death );
                        gui.popup_separator( );
                        gui.popup_section_begin( "traj_w", false ); {
                            ImGui::SliderInt( "Opacity", &traj_alpha, 0, 100, "%d" );
                        } gui.popup_section_end( );
                        gui.row_options_end( );
                    }
                    ImGui::Checkbox( "Grenade Trajectory", &traj_on );
                    gui.row_checkbox( "Grenade Proximity Warnings", &prox_warn );

                } gui.end_group_box( );

                gui.grid_col( 1 );
                gui.grid_box( "HUD", k_auto_height ); {

                    gui.row_begin( ); ImGui::Combo( "Radar", &radar, select_items, IM_ARRAYSIZE( select_items ) );
                    gui.row_begin( ); ImGui::Combo( "Scope Overlay", &scope_overlay, overlay_items, IM_ARRAYSIZE( overlay_items ) );
                    gui.row_checkbox( "Inaccuracy Overlay", &inacc, &inacc_col );
                    if ( gui.row_popup( "Death Notices" ) ) { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Scoreboard" ) )    { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Crosshairs" ) )    { world_popup( ); gui.popup_end( ); }

                } gui.end_group_box( );

                gui.grid_box( "MISCELLANEOUS", k_auto_height ); {

                    if ( gui.row_popup( "Windows" ) )  { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Removals" ) ) { world_popup( ); gui.popup_end( ); }
                    ambience_open |= gui.row_sub_menu( "Ambience", "Per-map lighting and weather." );
                    if ( gui.row_popup( "Hit Marker" ) )     { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Bullet Tracers" ) ) { world_popup( ); gui.popup_end( ); }
                    if ( gui.row_popup( "Bullet Impacts" ) ) { world_popup( ); gui.popup_end( ); }

                } gui.end_group_box( );

                gui.end_grid( );
                gui.row_spacing = 8.f;
                gui.box_pad_y = 8.f;

                break;
            }

            case 5: {
                // UI SETTINGS logic has been moved to User Profile popup
                break;
            }

            case 6: { // Visuals -> Shader
                shader_bg::state_t& sh = shader_bg::state( );

                const float weights[] = { 0.42f, 0.58f };
                gui.begin_grid( 2, weights );

                gui.grid_col( 0 );
                gui.grid_box( "BACKGROUND", k_auto_height ); {
                    gui.row_begin( );
                    const float settings_label_w = ImMax( ImGui::CalcTextSize( gui.tr( "Opacity" ) ).x,
                        ImGui::CalcTextSize( gui.tr( "Speed" ) ).x ) / gui.m_scale;
                    if ( gui.row_options_begin( "##shader_settings", ImMax( 216.f, settings_label_w + 128.f ) ) ) {
                        int opacity = ( int )IM_ROUND( sh.opacity * 100.f );
                        if ( ImGui::SliderInt( "Opacity", &opacity, 0, 100, "%d%%" ) )
                            sh.opacity = opacity / 100.f;

                        int speed = ( int )IM_ROUND( sh.speed * 100.f );
                        if ( ImGui::SliderInt( "Speed", &speed, 0, 300, "%d%%" ) )
                            sh.speed = speed / 100.f;
                        gui.row_options_end( );
                    }
                    ImGui::Checkbox( "Enabled", &sh.enabled );
                    gui.info_tooltip( "Enabled", "Draw the selected effect behind the menu's panels." );

                    gui.row_begin( );
                    const float blur_label_w = ImGui::CalcTextSize( gui.tr( "Blur Intensity" ) ).x / gui.m_scale;
                    if ( gui.row_options_begin( "##shader_blur_settings", ImMax( 240.f, blur_label_w + 128.f ) ) ) {
                        int intensity = ( int )IM_ROUND( sh.blur_intensity );
                        if ( ImGui::SliderInt( "Blur Intensity", &intensity, 0, 30, "%d px" ) )
                            sh.blur_intensity = ( float )intensity;
                        gui.row_options_end( );
                    }
                    ImGui::Checkbox( "Blur On Shader", &sh.blur_enabled );
                    gui.info_tooltip( "Blur On Shader", "Blur the selected effect behind the menu." );
                } gui.end_group_box( );

                gui.grid_col( 1 );
                const float saved_spacing = gui.row_spacing;
                const float saved_padding = gui.box_pad_y;
                gui.row_spacing = 3.f;
                gui.box_pad_y = 6.f;
                gui.grid_box( "EFFECT", 0.f, true ); {
                    for ( int i = 0; i < shader_bg::count( ); ++i ) {
                        char label[ 64 ];
                        ImFormatString( label, IM_ARRAYSIZE( label ), "%s  %s",
                            shader_bg::icon( i ), shader_bg::name( i ) );
                        if ( gui.framed_row( label, sh.selected == i ) )
                            sh.selected = i;
                    }
                } gui.end_group_box( );
                gui.row_spacing = saved_spacing;
                gui.box_pad_y = saved_padding;

                gui.end_grid( );
                break;
            }
            }
            gui.search_scope_end();
            };
            draw_tab(gui.m_tab);

            EndChild( );

            // ---- Ambience: its own panel over the menu (Visuals -> World -> Ambience) ----
            {
                static int  map_sel = 0;
                static bool fx[ 11 ] = { false, false, false, false, false, false, false, false, false, false, false };
                static color_value fx_cols[ 11 ] = {
                    { ImVec4( 0.66f, 0.62f, 0.86f, 1.f ) }, { ImVec4( 1.00f, 1.00f, 1.00f, 1.f ) },
                    { ImVec4( 0.85f, 0.85f, 0.88f, 1.f ) }, { ImVec4( 0.88f, 0.88f, 0.92f, 1.f ) },
                    { ImVec4( 1.00f, 1.00f, 1.00f, 1.f ) }, { ImVec4( 0.80f, 0.84f, 0.92f, 1.f ) },
                    { ImVec4( 0.90f, 0.90f, 0.95f, 1.f ) }, { ImVec4( 1.00f, 1.00f, 1.00f, 1.f ) },
                    { ImVec4( 0.86f, 0.88f, 0.95f, 1.f ) }, { ImVec4( 0.84f, 0.86f, 0.94f, 1.f ) },
                    { ImVec4( 0.82f, 0.86f, 0.96f, 1.f ) } };
                static int  fx_mode[ 11 ] = {};
                static bool wind = false, wetness = false, thunder = false, rain = false, snow = false;
                static color_value thunder_c{ ImVec4( 0.44f, 0.69f, 1.00f, 1.f ) };
                static color_value rain_c   { ImVec4( 0.86f, 0.91f, 1.00f, 1.f ) };
                static color_value snow_c   { ImVec4( 0.95f, 0.95f, 0.97f, 1.f ) };
                const char* fx_labels[] = { "Nightmode", "Fullbright", "Exposure", "Sunlight", "Sky", "Fog",
                                            "Bloom", "Vignette", "Local Contrast", "Color Correction", "Depth of Field" };
                const bool  fx_dots[]   = { false, false, true, false, false, false, true, false, true, true, true };
                const char* fx_modes[]  = { "Default", "Soft", "Strong" };

                if ( gui.overlay_begin( "Ambience", &ambience_open ) ) {

                    const float weights[] = { 0.454f, 0.546f };
                    gui.begin_grid( 2, weights );

                    gui.grid_col( 0 );
                    { // no box round the map list - the entries are framed already
                        gui.column_title( "MAP SELECTION" );
                        bool dots_clicked = false, add_clicked = false;
                        const float col_w = gui.grid_width( );
                        if ( gui.framed_row( "Global", map_sel == 0, ICON_FA_ELLIPSIS_H, nullptr, &dots_clicked, col_w ) )
                            map_sel = 0;
                        if ( gui.framed_row( "Main Menu", map_sel == 1, ICON_FA_FILE_IMPORT, "Add", &add_clicked, col_w ) )
                            map_sel = 1;
                        if ( add_clicked )
                            PushNotification( gui.tr( "Ambience" ), gui.tr( "Added the current map" ) );
                    }

                    gui.grid_col( 1 );
                    gui.row_spacing = 6.f;
                    gui.box_pad_y = 6.f;
                    gui.grid_box( "EFFECTS", k_auto_height ); {

                        for ( int i = 0; i < IM_ARRAYSIZE( fx_labels ); ++i ) {
                            ImGui::PushID( i );
                            gui.row_begin( );
                            if ( fx_dots[ i ] ) { // "..." for the effects with their own settings
                                if ( gui.row_options_begin( "##fx_opts" ) ) {
                                    gui.popup_option( ICON_FA_ADJUST, "Mode", fx_modes, IM_ARRAYSIZE( fx_modes ), &fx_mode[ i ] );
                                    gui.popup_separator( );
                                    gui.popup_section_begin( "fx_amount", false ); {
                                        static int amount[ 11 ] = { 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50 };
                                        ImGui::SliderInt( "Amount", &amount[ i ], 0, 100, "%d" );
                                    } gui.popup_section_end( );
                                    gui.row_options_end( );
                                }
                            } else {
                                gui.color_picker( "##fx_col", &fx_cols[ i ] );
                            }
                            ImGui::Checkbox( fx_labels[ i ], &fx[ i ] );
                            ImGui::PopID( );
                        }

                        if ( gui.row_popup( "Weather" ) ) {
                            gui.popup_section_begin( "wx1", true ); {
                                if ( gui.row_options_begin( "##wind_opts" ) ) {
                                    gui.popup_toggle( ICON_FA_TREE, "Affect Foliage", &wetness );
                                    gui.row_options_end( );
                                }
                                ImGui::Checkbox( ICON_FA_WIND " Wind", &wind );
                                if ( gui.row_options_begin( "##wet_opts" ) ) {
                                    gui.popup_toggle( ICON_FA_USER, "Players Only", &wetness );
                                    gui.row_options_end( );
                                }
                                ImGui::Checkbox( ICON_FA_TINT " Model Wetness", &wetness );
                            } gui.popup_section_end( );
                            gui.popup_section_begin( "wx2", true ); {
                                gui.color_picker( "##thunder", &thunder_c );
                                ImGui::Checkbox( ICON_FA_BOLT " Thunder", &thunder );
                                gui.color_picker( "##rain", &rain_c );
                                ImGui::Checkbox( ICON_FA_CLOUD_SHOWERS " Rain", &rain );
                                gui.color_picker( "##snow", &snow_c );
                                ImGui::Checkbox( ICON_FA_SNOWFLAKE " Snow", &snow );
                            } gui.popup_section_end( );
                            gui.popup_end( );
                        }
                        if ( gui.row_popup( "Removals" ) ) {
                            gui.popup_section_begin( "rm", true ); {
                                static bool rm[ 3 ] = { false, false, false };
                                ImGui::Checkbox( "Smoke", &rm[ 0 ] );
                                ImGui::Checkbox( "Flashbang", &rm[ 1 ] );
                                ImGui::Checkbox( "Scope Blur", &rm[ 2 ] );
                            } gui.popup_section_end( );
                            gui.popup_end( );
                        }

                    } gui.end_group_box( );
                    gui.row_spacing = 8.f;
                    gui.box_pad_y = 8.f;

                    gui.end_grid( );
                    gui.overlay_end( );
                }
            }

            // Register real rows in unvisited tabs. Hidden children skip input and rendering;
            // row registration happens before SkipItems in the shared widget implementations.
            if (gui.search_index_pass()) {
                const ImVec2 saved_cursor = window->DC.CursorPos;
                const ImVec2 saved_cursor_max = window->DC.CursorMaxPos;
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.f);
                ImGui::BeginDisabled();
                for (int tab = 0; tab < 2; ++tab) {
                    if (tab == gui.m_tab)
                        continue;
                    ImGui::PushID(tab);
                    ImGui::SetCursorScreenPos(window->Pos - ImVec2(10000.f, 10000.f));
                    ImGui::BeginChild("##search_index", ImVec2(size.x - 200.f, size.y - 91.f), false,
                        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
                    draw_tab(tab);
                    ImGui::EndChild();
                    ImGui::PopID();
                }
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
                window->DC.CursorPos = saved_cursor;
                window->DC.CursorMaxPos = saved_cursor_max;
            }

            PopStyleVar( 2 );

            // Menu background darkening on popup open has been disabled as requested
            focus_anim = 0.f;
            gui_pos = window->Pos;
            gui_size = window->Size;

            if ( !menu_pos_ready ) { // first frame: start from wherever imgui.ini put the menu
                menu_pos = menu_target = window->Pos;
                menu_pos_ready = true;
            }
            // Grab only on empty space (no widget under the cursor). NoPopupHierarchy: popups
            // opened from inside the menu count as its "children" by default, so grabbing a
            // popup's empty space used to drag the whole menu - the popup drags itself instead.
            if ( !menu_dragging && ImGui::IsMouseClicked( 0 ) && !ImGui::IsAnyItemHovered( ) &&
                 ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy ) ) {
                menu_dragging = true;
                menu_grab = io.MousePos - window->Pos;
            }

        } ImGui::End( );

        PopStyleVar( );

        // Its own top-level window, submitted after the menu so it floats above it.
        gui.spotify_widget_draw( );

        // Rendering
        DrawNotifications();
        ImGui::EndFrame();
        ImGui::Render();

        const float clear_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                                            clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // vsync
    }

    spotify::stop();
    gifbg::stop();
    blur::shutdown();
    shader_bg::shutdown();
    if (avatar) { avatar->Release(); avatar = nullptr; }
    if (bg) { bg->Release(); bg = nullptr; }
    g_wic.Reset();
    ImGui_ImplDX11_Shutdown();
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
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;  // taken from the window
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    // The blur copies straight out of the back buffer, so it has to be usable as a texture too.
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.OutputWindow = hWnd;

    const UINT flags = 0;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, (UINT)IM_ARRAYSIZE(levels), D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
        &level, &g_pd3dDeviceContext);
    if (hr == DXGI_ERROR_UNSUPPORTED) // no hardware device: fall back to WARP
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
            levels, (UINT)IM_ARRAYSIZE(levels), D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
            &level, &g_pd3dDeviceContext);
    if (FAILED(hr))
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back))) && back)
    {
        g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRenderTargetView);
        back->Release();
    }
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
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
