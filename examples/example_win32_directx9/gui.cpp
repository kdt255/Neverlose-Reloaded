#include "gui.hpp"
#include "hashes.hpp"
#include "notification.hpp"
#include "shader_bg.hpp"
#include "spotify.hpp"
#include "gifbg.hpp"
#include <algorithm>
#include <cstring>
#include <math.h>

using namespace ImGui;

void AddSquircleFilled(ImDrawList* draw, ImVec2 p_min, ImVec2 p_max, ImColor col, float radius, ImDrawFlags corner_flags) {
    if (radius <= 0.0f || corner_flags == 0) { draw->AddRectFilled(p_min, p_max, col); return; }
    float L = radius * 1.5f;
    float max_L = fminf((p_max.x - p_min.x) * 0.5f, (p_max.y - p_min.y) * 0.5f);
    if (L > max_L) L = max_L;

    draw->PathClear();
    const int segments = 16;
    const ImDrawFlags corner_mask[4] = { ImDrawFlags_RoundCornersBottomRight, ImDrawFlags_RoundCornersBottomLeft, ImDrawFlags_RoundCornersTopLeft, ImDrawFlags_RoundCornersTopRight };
    const ImVec2 rect_corner[4] = { p_max, ImVec2(p_min.x, p_max.y), p_min, ImVec2(p_max.x, p_min.y) };
    for (int corner = 0; corner < 4; corner++) {
        if (!(corner_flags & corner_mask[corner])) {
            draw->PathLineTo(rect_corner[corner]); // sharp corner, no curve
            continue;
        }

        ImVec2 center;
        float start_angle;
        if (corner == 0) { center = ImVec2(p_max.x - L, p_max.y - L); start_angle = 0.0f; } // BR
        else if (corner == 1) { center = ImVec2(p_min.x + L, p_max.y - L); start_angle = IM_PI * 0.5f; } // BL
        else if (corner == 2) { center = ImVec2(p_min.x + L, p_min.y + L); start_angle = IM_PI; } // TL
        else { center = ImVec2(p_max.x - L, p_min.y + L); start_angle = IM_PI * 1.5f; } // TR

        for (int i = 0; i <= segments; i++) {
            float a = start_angle + (float)i / (float)segments * (IM_PI * 0.5f);
            float ca = cosf(a), sa = sinf(a);
            float cx = powf(fabsf(ca), 0.4f) * (ca > 0 ? 1 : -1);
            float cy = powf(fabsf(sa), 0.4f) * (sa > 0 ? 1 : -1);
            draw->PathLineTo(center + ImVec2(cx * L, cy * L));
        }
    }
    draw->PathFillConvex(col);
}

void AddSquircle(ImDrawList* draw, ImVec2 p_min, ImVec2 p_max, ImColor col, float radius, float thickness) {
    if (radius <= 0.0f) { draw->AddRect(p_min, p_max, col, 0.0f, 0, thickness); return; }
    float L = radius * 1.5f;
    float max_L = fminf((p_max.x - p_min.x) * 0.5f, (p_max.y - p_min.y) * 0.5f);
    if (L > max_L) L = max_L;

    draw->PathClear();
    const int segments = 16;
    for (int corner = 0; corner < 4; corner++) {
        ImVec2 center;
        float start_angle;
        if (corner == 0) { center = ImVec2(p_max.x - L, p_max.y - L); start_angle = 0.0f; } // BR
        else if (corner == 1) { center = ImVec2(p_min.x + L, p_max.y - L); start_angle = IM_PI * 0.5f; } // BL
        else if (corner == 2) { center = ImVec2(p_min.x + L, p_min.y + L); start_angle = IM_PI; } // TL
        else { center = ImVec2(p_max.x - L, p_min.y + L); start_angle = IM_PI * 1.5f; } // TR
        
        for (int i = 0; i <= segments; i++) {
            float a = start_angle + (float)i / (float)segments * (IM_PI * 0.5f);
            float ca = cosf(a), sa = sinf(a);
            float cx = powf(fabsf(ca), 0.4f) * (ca > 0 ? 1 : -1);
            float cy = powf(fabsf(sa), 0.4f) * (sa > 0 ? 1 : -1);
            draw->PathLineTo(center + ImVec2(cx * L, cy * L));
        }
    }
    draw->PathStroke(col, true, thickness);
}

void c_gui::render_circle_for_horizontal_bar( ImVec2 pos, ImColor color, float alpha ) {

    auto draw = GetWindowDrawList( );
    draw->AddCircleFilled( pos, 6, ImColor( color.Value.x, color.Value.y, color.Value.z, alpha * GetStyle( ).Alpha ) );
}

bool c_gui::tab( const char* icon, const char* label, bool selected, float indent ) {

    auto window = GetCurrentWindow( );
    auto id = window->GetID( label );
    label = tr( label );

    auto icon_size = CalcTextSize( icon );
    auto label_size = CalcTextSize( label, 0, 1 );

    auto pos = window->DC.CursorPos;
    auto draw = window->DrawList;

    ImRect bb( pos, pos + ImVec2( GetWindowWidth( ), 30 ) );
    ItemAdd( bb, id );
    ItemSize( bb, GetStyle( ).FramePadding.y );

    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );

    struct tab_state { float active; float hover; };
    static std::unordered_map < ImGuiID, tab_state > values;
    auto value = values.find( id );
    if ( value == values.end( ) ) {

        values.insert( { id, { 0.f, 0.f } } );
        value = values.find( id );
    }

    value->second.active = ImLerp( value->second.active, ( selected ? 1.f : 0.f ), 0.08f );
    value->second.hover = ImLerp( value->second.hover, ( hovered ? 1.f : 0.f ), 0.08f );

    // Equal margin either side (the pill used to sit 4px from the left and 10px from the right).
    const float pill_margin = 6.f * gui.m_scale;
    ImRect bg_bb(bb.Min + ImVec2(pill_margin + indent, 0), bb.Max - ImVec2(pill_margin, 0));

    float bg_alpha = ImClamp(value->second.active + (value->second.hover * 0.4f), 0.f, 1.f);
    if (bg_alpha > 0.01f) {
        draw->AddRectFilled( bg_bb.Min, bg_bb.Max, gui.tab_active.to_im_color( bg_alpha ), 6 );
    }

    ImVec4 icon_color = gui.text_disabled.to_im_color().Value;
    if (value->second.hover > 0.f)
        icon_color = ImLerp(icon_color, gui.text.to_im_color().Value, value->second.hover);
    if (value->second.active > 0.f)
        icon_color = ImLerp(icon_color, gui.accent_color.to_im_color().Value, value->second.active);

    ImVec4 text_color = ImLerp(gui.text_disabled.to_im_color().Value, gui.text.to_im_color().Value, ImClamp(value->second.active + value->second.hover, 0.f, 1.f));

    // Icon and label sit inside the pill, so they keep their padding whatever the pill's margin is.
    // The glyph gets a fixed-width column and is centred on its own ink inside it: icons are not
    // all the same width (the mouse is narrower than the crosshair), and hanging the label off the
    // glyph's advance is what left "Legit" a couple of pixels out of line with the other tabs.
    const float icon_slot = 16.f * gui.m_scale;
    const float icon_left = bg_bb.Min.x + 9.f * gui.m_scale;
    unsigned int icon_cp = 0;
    ImTextCharFromUtf8( &icon_cp, icon, NULL );
    const ImFontGlyph* icon_glyph = GetFont( )->FindGlyph( ( ImWchar )icon_cp );
    const float icon_scale = GetFontSize( ) / GetFont( )->FontSize;
    const ImVec2 icon_center = icon_glyph
        ? ImVec2( ( icon_glyph->X0 + icon_glyph->X1 ) * 0.5f * icon_scale,
                  ( icon_glyph->Y0 + icon_glyph->Y1 ) * 0.5f * icon_scale )
        : icon_size * 0.5f;
    const ImVec2 icon_pos( IM_ROUND( icon_left + icon_slot * 0.5f - icon_center.x ),
                           IM_ROUND( bb.GetCenter( ).y - icon_center.y ) );
    const float label_x = icon_left + icon_slot + 7.f * gui.m_scale;

    draw->AddText( icon_pos, GetColorU32(icon_color), icon );
    RenderScrollingText( draw, id, ImRect( label_x, bb.Min.y, bg_bb.Max.x - 8.f * gui.m_scale, bb.Max.y ),
                         label, GetColorU32(text_color), hovered );

    return pressed;
}

bool c_gui::subtab( const char* label, bool selected, int size, ImDrawFlags flags ) {

    auto window = GetCurrentWindow( );
    auto id = window->GetID( label );

    auto label_size = CalcTextSize( label, 0, 1 );

    auto pos = window->DC.CursorPos;
    auto draw = window->DrawList;

    ImRect bb( pos, pos + ImVec2( GetWindowWidth( ) / size, GetWindowHeight( ) ) );
    ItemAdd( bb, id );
    ItemSize( bb, GetStyle( ).FramePadding.y );

    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );

    static std::unordered_map < ImGuiID, float > values;
    auto value = values.find( id );
    if ( value == values.end( ) ) {

        values.insert( { id, 0.f } );
        value = values.find( id );
    }

    value->second = ImLerp( value->second, ( selected ? 1.f : 0.f ), 0.05f );

    draw->AddRectFilled( bb.Min, bb.Max, gui.frame_active.to_im_color( 0.8f * value->second ), 4, flags );

    draw->AddText( bb.GetCenter( ) - label_size / 2, selected ? gui.text.to_im_color( ) : gui.text_disabled.to_im_color( ), label );

    return pressed;
}

void c_gui::group_box( const char* name, ImVec2 size_arg, bool scrollable ) {
    if (m_search_tab >= 0) m_search_box = name;

    auto window = GetCurrentWindow( );
    auto pos = window->DC.CursorPos;

    bool is_popup = (window->Flags & ImGuiWindowFlags_Popup) != 0;

    m_cur_box_id = window->GetID( name );
    bool want_auto_size = size_arg.y <= 0.f;
    m_cur_box_auto = want_auto_size || m_force_measure;
    m_force_measure = false;
    m_row_started = false;

    if ( want_auto_size ) {
        auto it = m_auto_box_heights.find( m_cur_box_id );
        size_arg.y = ( it != m_auto_box_heights.end( ) ) ? it->second : 80.f * gui.m_scale;
    }

    BeginChild( std::string( name ).append( ".main" ).c_str( ), size_arg, false,
        ImGuiWindowFlags_NoScrollbar | ( scrollable ? ImGuiWindowFlags_NoScrollWithMouse : 0 ) );

    const float header_h = box_header_h( );

    AddSquircleFilled( GetWindowDrawList( ), pos + ImVec2( 0, header_h ), pos + size_arg, gui.group_box_bg.to_im_color( ), 20 * gui.m_scale );
    AddSquircle( GetWindowDrawList( ), pos + ImVec2( 0, header_h ), pos + size_arg, gui.border.to_im_color( ), 20 * gui.m_scale, 1.0f );

    // Title centred on its own ink inside the header strip, so the gap above and below it match.
    ImFont* header_font = ImGui::GetIO().Fonts->Fonts[is_popup ? 4 : 6];
    ImU32 header_col = is_popup ? gui.text.to_im_color() : GetColorU32( ImGuiCol_Text, 0.5f );
    const char* header_text = tr( name );
    const float header_text_y = IM_ROUND( header_h * 0.5f - TextLineCenterY( header_font, header_font->FontSize ) );
    GetWindowDrawList( )->AddText( header_font, header_font->FontSize, pos + ImVec2( 10 * gui.m_scale, header_text_y - 3 ), header_col, header_text );

    SetCursorPos( ImVec2( 0, header_h ) );
    PushStyleVar( ImGuiStyleVar_WindowPadding, { 12 * gui.m_scale, gui.box_pad_y * gui.m_scale } ); // Indent contents
    const ImGuiWindowFlags content_flags = ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar;
    BeginChild( name, { size_arg.x, size_arg.y - header_h }, 0, content_flags );
    ImGuiWindow* content = GetCurrentWindow();
    content->StateStorage.SetInt(ImHashStr("##ScrollFadeStart"), scrollable ? content->DrawList->VtxBuffer.Size : -1);

    BeginGroup( );

    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 8 * gui.m_scale, gui.row_spacing * gui.m_scale } );
    // Inherited, never overwritten: the tab already pushes gui.m_anim around all of its content,
    // and forcing it back here pinned every box at full opacity inside a container that was
    // fading (the Ambience overlay), which is what made its fade read as a glitch.
    PushStyleVar( ImGuiStyleVar_Alpha, GetStyle( ).Alpha );
}

void c_gui::end_group_box( ) {
    ImGuiWindow* content = GetCurrentWindow();
    const int fade_start = content->StateStorage.GetInt(ImHashStr("##ScrollFadeStart"), -1);
    if (fade_start >= 0 && content->ScrollMax.y > 0.5f) {
        const ImRect clip = content->InnerClipRect;
        const float fade_height = ImMin(40.f * m_scale, clip.GetHeight() * 0.25f);
        if (fade_height > 0.f) {
            const float top_distance = ImMax(0.f, content->Scroll.y);
            const float bottom_distance = ImMax(0.f, content->ScrollMax.y - content->Scroll.y);
            const float top_strength = top_distance <= 0.5f ? 0.f : ImSaturate(top_distance / fade_height);
            const float bottom_strength = bottom_distance <= 0.5f ? 0.f : ImSaturate(bottom_distance / fade_height);
            const auto smooth = [](float value) {
                const float t = ImSaturate(value);
                return t * t * (3.f - 2.f * t);
            };
            // Fade only list content: the panel, border and header keep their original opacity.
            for (int i = fade_start; i < content->DrawList->VtxBuffer.Size; ++i) {
                ImDrawVert& vertex = content->DrawList->VtxBuffer[i];
                const float top = 1.f - top_strength * (1.f - smooth((vertex.pos.y - clip.Min.y) / fade_height));
                const float bottom = 1.f - bottom_strength * (1.f - smooth((clip.Max.y - vertex.pos.y) / fade_height));
                const ImU32 alpha = (vertex.col & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
                const ImU32 faded = (ImU32)(alpha * top * bottom + 0.5f);
                vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (faded << IM_COL32_A_SHIFT);
            }
        }
    }
    if ( m_cur_box_auto ) {
        // GetCursorPosY() after the last widget already carries one extra ItemSpacing.y (ImGui
        // advances the cursor past it eagerly, not lazily before a next item that never comes)
        // - back that out, then mirror the top WindowPadding (8 * scale) for a symmetric bottom.
        float content_h = ImGui::GetCursorPosY( ) - ImGui::GetStyle( ).ItemSpacing.y + gui.box_pad_y * gui.m_scale;
        m_auto_box_heights[ m_cur_box_id ] = content_h + box_header_h( );
    }
    PopStyleVar( 3 );
    EndGroup( );
    EndChild( );
    EndChild( );
}

void c_gui::begin_grid(int columns, const float* weights) {
    m_grid_pos = ImGui::GetCursorPos();
    m_grid_size = ImGui::GetContentRegionAvail();

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float usable = m_grid_size.x - spacing * (columns - 1);

    float total = 0.f;
    for (int i = 0; i < columns; ++i)
        total += weights ? ImMax(0.01f, weights[i]) : 1.f;

    m_grid_col_w.assign(columns, 0.f);
    m_grid_cursor.assign(columns, ImVec2());
    float x = m_grid_pos.x;
    for (int i = 0; i < columns; ++i) {
        m_grid_col_w[i] = usable * (weights ? ImMax(0.01f, weights[i]) : 1.f) / total;
        m_grid_cursor[i] = ImVec2(x, m_grid_pos.y);
        x += m_grid_col_w[i] + spacing;
    }

    m_grid_active = 0;
}

void c_gui::grid_col(int index) {
    m_grid_active = index;
    ImGui::SetCursorPos(m_grid_cursor[index]);
}

float c_gui::grid_remaining() const {
    const float bottom = m_grid_pos.y + m_grid_size.y;
    return bottom - m_grid_cursor[m_grid_active].y;
}

void c_gui::grid_box(const char* name, float height, bool scrollable) {
    const int i = m_grid_active;
    const float spacing_y = ImGui::GetStyle().ItemSpacing.y;

    float h;
    bool auto_h = (height == k_auto_height);
    if (auto_h) {
        // peek at the remembered natural content height so the grid cursor advances correctly,
        // but never let it push past the column's boundary (clamp = defensive overflow guard)
        ImGuiID box_id = ImGui::GetID(name);
        auto it = m_auto_box_heights.find(box_id);
        h = (it != m_auto_box_heights.end()) ? it->second : 80.f * m_scale;
        h = ImMin(h, grid_remaining() - 2.f * m_scale);
        m_force_measure = true; // still re-measure true content height this frame for next frame's peek
    } else {
        // safety margin so a "fill remaining" box's rounded bottom corner never
        // touches the exact parent boundary (which would otherwise get hard-clipped flat)
        const float bottom_safety = 2.f * m_scale;
        const float remaining = grid_remaining() - bottom_safety;
        if (height > 0.f)      h = height * m_scale;
        else if (height < 0.f) h = remaining * -height;
        else                   h = remaining;
    }
    h = ImMax(h, 10.f * m_scale);

    ImGui::SetCursorPos(m_grid_cursor[i]);
    group_box(name, ImVec2(m_grid_col_w[i], h), scrollable);

    m_grid_cursor[i].y += h + spacing_y;
}

float c_gui::grid_width() const {
    return ( m_grid_active >= 0 && m_grid_active < (int)m_grid_col_w.size( ) ) ? m_grid_col_w[ m_grid_active ] : 0.f;
}

void c_gui::column_title(const char* name) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( window->SkipItems )
        return;
    // Mirrors group_box()'s header strip exactly (same font, same 50% ink, same 10px indent and
    // the same -3 optical lift), so a boxless column and a box beside it share one baseline.
    const ImVec2 pos = window->DC.CursorPos;
    const float  header_h = box_header_h( );
    ImFont* font = ImGui::GetIO( ).Fonts->Fonts[ 6 ];
    const float text_y = IM_ROUND( header_h * 0.5f - TextLineCenterY( font, font->FontSize ) );
    window->DrawList->AddText( font, font->FontSize, pos + ImVec2( 10 * m_scale, text_y - 3 ),
        ImGui::GetColorU32( ImGuiCol_Text, 0.5f ), tr( name ) );
    // Claim the strip without ItemSpacing on top of it - the next row has to land on header_h.
    ImGui::SetCursorScreenPos( ImVec2( pos.x, pos.y + header_h ) );
}

float c_gui::measured_box_height(const char* name) const {
    const auto it = m_auto_box_heights.find(ImGui::GetID(name));
    return it != m_auto_box_heights.end() ? it->second / m_scale : 0.f;
}

void c_gui::grid_skip(float height) {
    m_grid_cursor[m_grid_active].y += height;
}

void c_gui::end_grid() {
    m_grid_col_w.clear();
    m_grid_cursor.clear();
}

void c_gui::popup_group_box( const char* name, ImVec2 size_arg ) {
    auto window = GetCurrentWindow( );
    auto pos = window->DC.CursorPos;

    BeginChild( std::string( name ).append( ".popup_main" ).c_str( ), size_arg, false, ImGuiWindowFlags_NoScrollbar );

    AddSquircleFilled( GetWindowDrawList( ), pos, pos + size_arg, gui.group_box_bg.to_im_color( ), 6 * gui.m_scale );
    AddSquircle( GetWindowDrawList( ), pos, pos + size_arg, gui.border.to_im_color( ), 6 * gui.m_scale, 1.0f );

    ImFont* header_font = ImGui::GetIO().Fonts->Fonts[4];
    GetWindowDrawList( )->AddText( header_font, header_font->FontSize, pos + ImVec2( 15 * gui.m_scale, 12 * gui.m_scale ), gui.text.to_im_color(), name );
    
    GetWindowDrawList( )->AddLine( pos + ImVec2( 10 * gui.m_scale, 38 * gui.m_scale ), pos + ImVec2( size_arg.x - (10 * gui.m_scale), 38 * gui.m_scale ), gui.border.to_im_color(), 1.0f );

    SetCursorPos( ImVec2( 0, 45 * gui.m_scale ) );
    PushStyleVar( ImGuiStyleVar_WindowPadding, { 12 * gui.m_scale, 4 * gui.m_scale } );
    BeginChild( name, { size_arg.x, size_arg.y - (45 * gui.m_scale) }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysUseWindowPadding );

    BeginGroup( );

    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 8 * gui.m_scale, 8 * gui.m_scale } );
    PushStyleVar( ImGuiStyleVar_Alpha, gui.m_anim );
}

bool c_gui::sub_menu(const char* label) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) { search_note(label); return false; }

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 pos = window->DC.CursorPos;
    // match Checkbox's row height exactly so every row in a group_box is the same thickness
    float row_h = ImMax(19.f * gui.m_scale, ImGui::CalcTextSize(label).y) + style.FramePadding.y * 2.f;
    ImVec2 size(w, row_h);
    ImRect total_bb(pos, pos + size);
    const float flash = search_note(label);
    
    ImGui::ItemAdd(total_bb, id);
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);

    ImDrawList* draw = window->DrawList;

    bool is_popup_open = ImGui::IsPopupOpen((std::string(label) + "Popup").c_str());

    static std::unordered_map<ImGuiID, float> open_anims;
    float& o_anim = open_anims[id];
    o_anim = ImLerp(o_anim, (hovered || is_popup_open) ? 1.f : 0.f, g.IO.DeltaTime * 15.f);

    ImVec4 t_col = ImLerp(gui.text_disabled.to_vec4(), gui.text.to_vec4(), o_anim);
    t_col = ImLerp(t_col, gui.accent_color.to_vec4(1.f, false), flash); // jumped here from the search
    const ImU32 text_color = ImGui::GetColorU32(t_col);

    const char* icon = ICON_FA_CHEVRON_RIGHT;
    const ImVec2 icon_size = ImGui::CalcTextSize(icon);
    const float icon_x = total_bb.Max.x - 10.f * gui.m_scale - icon_size.x;

    // label scrolls on hover when it is longer than the room left of the chevron
    const char* shown = tr(label); // the popup id above stays on the English label
    RenderScrollingText(draw, id, ImRect(total_bb.Min.x, total_bb.Min.y, icon_x - 8.f * gui.m_scale, total_bb.Max.y),
                        shown, text_color, hovered);
    draw->AddText(ImVec2(icon_x, IM_ROUND(total_bb.GetCenter().y - icon_size.y / 2.f)), text_color, icon);
    
    //float sep_y = total_bb.Max.y + style.ItemSpacing.y * 0.5f;
    //draw->AddLine(ImVec2(total_bb.Min.x + 5, sep_y), ImVec2(total_bb.Max.x - 5, sep_y), gui.border.to_im_color());

    return pressed;
}

void c_gui::row_begin() {
    if ( m_row_started ) Separator( );
    m_row_started = true;
}

bool c_gui::row_checkbox(const char* label, bool* v, const char* description) {
    row_begin();
    const bool changed = Checkbox(label, v);
    info_tooltip(label, description);
    return changed;
}

bool c_gui::row_sub_menu(const char* label, const char* description) {
    row_begin();
    const bool pressed = sub_menu(label);
    info_tooltip(label, description);
    return pressed;
}

void c_gui::popup_shadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float alpha) {
    if (!draw || alpha <= 0.f || max.x <= min.x || max.y <= min.y)
        return;
    constexpr int rings = 12;
    constexpr int corner_segments = 16;
    constexpr int points = 4 * (corner_segments + 1);
    const float spread = 20.f * m_scale;
    const float radius = ImClamp(rounding, 0.f, ImMin(max.x - min.x, max.y - min.y) * 0.5f);
    const ImVec2 clip_pad(spread + 1.f, spread * 1.25f + 1.f);
    draw->PushClipRect(min - clip_pad, max + clip_pad, false);
    draw->PrimReserve(rings * points * 6, (rings + 1) * points);
    const unsigned int base = draw->_VtxCurrentIdx;
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    for (int ring = 0; ring <= rings; ++ring) {
        const float t = (float)ring / rings;
        const float expand = spread * t;
        const ImVec2 offset(0.f, 3.f * m_scale * t);
        const ImVec2 outer_min = min - ImVec2(expand, expand) + offset;
        const ImVec2 outer_max = max + ImVec2(expand, expand) + offset;
        const float r = radius + expand;
        const ImVec2 centers[] = {
            ImVec2(outer_max.x - r, outer_min.y + r),
            ImVec2(outer_max.x - r, outer_max.y - r),
            ImVec2(outer_min.x + r, outer_max.y - r),
            ImVec2(outer_min.x + r, outer_min.y + r)
        };
        const float falloff = (1.f - t) * (1.f - t) * (1.f - t);
        const ImU32 color = IM_COL32(0, 0, 0, (int)(255.f * 0.32f * ImSaturate(alpha) * falloff));
        for (int corner = 0; corner < 4; ++corner) {
            for (int step = 0; step <= corner_segments; ++step) {
                const float angle = (-0.5f + corner * 0.5f + 0.5f * step / corner_segments) * IM_PI;
                draw->PrimWriteVtx(centers[corner] + ImVec2(cosf(angle), sinf(angle)) * r, uv, color);
            }
        }
    }
    // Only connect adjacent outlines; the rounded popup interior remains untouched.
    for (int ring = 0; ring < rings; ++ring) {
        for (int point = 0; point < points; ++point) {
            const unsigned int a = base + ring * points + point;
            const unsigned int b = base + ring * points + (point + 1) % points;
            const unsigned int c = b + points;
            const unsigned int d = a + points;
            draw->PrimWriteIdx((ImDrawIdx)a); draw->PrimWriteIdx((ImDrawIdx)b); draw->PrimWriteIdx((ImDrawIdx)c);
            draw->PrimWriteIdx((ImDrawIdx)a); draw->PrimWriteIdx((ImDrawIdx)c); draw->PrimWriteIdx((ImDrawIdx)d);
        }
    }
    draw->PopClipRect();
}

extern void draw_blur_rounded(ImDrawList*, ImVec2, ImVec2, float, float, ImColor);

void c_gui::popup_surface(ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float alpha, bool border) {
    if (!draw || alpha <= 0.f || max.x <= min.x || max.y <= min.y)
        return;
    const float radius = ImClamp(rounding, 0.f, ImMax(0.f, ImMin(max.x - min.x, max.y - min.y) * 0.5f - 1.f));
    popup_shadow(draw, min, max, radius, alpha);
    const ImVec2 aa_padding(2.f * m_scale, 2.f * m_scale);
    draw->PushClipRect(min - aa_padding, max + aa_padding, false);
    draw_blur_rounded(draw, min, max, alpha, radius, ImColor(popup_bg.r, popup_bg.g, popup_bg.b, popup_bg.a));
    // Same animated background the menu carries, over this card's own fill and under its contents,
    // continuing the menu's image rather than starting its own - every card, tooltip and option
    // list comes through here, so they all pick it up.
    shader_bg::draw_into(draw, min, max, radius, alpha);
    if (border)
        draw->AddRect(min, max, popup_border.to_im_color(0.75f * alpha, false), radius, 0, 1.f * m_scale);
    draw->PopClipRect();
}

void c_gui::info_tooltip(const char* title, const char* description) {
    if (!title || !description || !*description)
        return;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* owner = ImGui::GetCurrentWindow();
    const ImGuiID id = g.LastItemData.ID ? g.LastItemData.ID : owner->GetID(title);
    const ImRect item_rect = g.LastItemData.Rect;
    const ImGuiID timer_key = ImHashStr("##InfoHoverTime", 0, id);
    const ImGuiID frame_key = ImHashStr("##InfoHoverFrame", 0, id);
    const ImGuiID alpha_key = ImHashStr("##InfoAlpha", 0, id);
    ImGuiStorage* storage = owner->DC.StateStorage;
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !ImGui::IsMouseDown(0);
    const bool consecutive_frame = storage->GetInt(frame_key, -1) == g.FrameCount - 1;
    float timer = consecutive_frame ? storage->GetFloat(timer_key) : 0.f;
    float visibility = consecutive_frame ? storage->GetFloat(alpha_key) : 0.f;
    timer = hovered ? timer + g.IO.DeltaTime : 0.f;
    const bool show = timer > 0.5f;
    visibility = show ? ImMin(1.f, visibility + g.IO.DeltaTime / 0.12f)
                      : ImMax(0.f, visibility - g.IO.DeltaTime / 0.14f);
    storage->SetFloat(timer_key, timer);
    storage->SetFloat(alpha_key, visibility);
    storage->SetInt(frame_key, g.FrameCount);
    if (visibility <= 0.f)
        return;

    unsigned int codepoint = 0;
    const int icon_bytes = ImTextCharFromUtf8(&codepoint, title, NULL);
    if (icon_bytes > 0 && codepoint >= ICON_MIN_FA && codepoint <= ICON_MAX_FA) {
        title += icon_bytes;
        while (*title == ' ') ++title;
    }
    title = tr(title);
    description = tr(description);
    const ImGuiNextWindowData next_window_backup = g.NextWindowData;
    g.NextWindowData.ClearFlags();
    const float alpha = g.Style.Alpha * visibility;
    const float rounding = 10.f * m_scale;
    ImGui::SetNextWindowPos(ImVec2(item_rect.Max.x + 30.f * m_scale, item_rect.Min.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f, 12.f) * m_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 8.f) * m_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border, popup_border.to_vec4(0.55f, false));
    if (ImGui::BeginTooltipEx(ImGuiTooltipFlags_None, ImGuiWindowFlags_NoBackground)) {
        ImGuiWindow* tooltip = ImGui::GetCurrentWindow();
        popup_surface(tooltip->DrawList, tooltip->Pos, tooltip->Pos + tooltip->Size, rounding, alpha);
        ImGui::PushFont(g.IO.Fonts->Fonts.Size > 4 ? g.IO.Fonts->Fonts[4] : g.Font);
        ImGui::PushStyleColor(ImGuiCol_Text, text.to_vec4(1.f, false));
        ImGui::TextUnformatted(title, ImGui::FindRenderedTextEnd(title));
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 320.f * m_scale);
        ImGui::PushStyleColor(ImGuiCol_Text, text_soft.to_vec4(1.f, false));
        ImGui::TextUnformatted(description);
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(5);
    g.NextWindowData = next_window_backup;
}

void c_gui::shadow_outline(
    ImDrawList* draw,
    ImVec2 pos,
    ImVec2 size,
    float rounding)
{
    ImVec2 min = pos;
    ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);

    const int shadowLayers = 256;

    for (int i = shadowLayers; i > 0; --i)
    {
        float alpha = (1.0f - (float)i / shadowLayers) * 35.0f;

        draw->AddRect(
            ImVec2(min.x - i, min.y - i),
            ImVec2(max.x + i, max.y + i),
            IM_COL32(0, 0, 0, (int)alpha),
            rounding + i,
            0,
            1.0f
        );
    }

    // Dark outer outline
    draw->AddRect(
        ImVec2(min.x - 1, min.y - 1),
        ImVec2(max.x + 1, max.y + 1),
        IM_COL32(0, 0, 0, 220),
        rounding,
        0,
        1.0f
    );

    // Main outline
    draw->AddRect(
        min,
        max,
        IM_COL32(70, 70, 75, 255),
        rounding,
        0,
        1.0f
    );
}

extern void draw_blur(ImDrawList* draw, float alpha = 1.0f, float rounding = -1.0f);
extern void draw_blur_rounded(ImDrawList* draw, ImVec2 min, ImVec2 max, float alpha = 1.0f, float rounding = -1.0f, ImColor tint = ImColor(1.0f, 1.0f, 1.0f, 0.0f));

void c_gui::sync_popup_context() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (m_popup_context != context || context->FrameCount < m_popup_context_frame) {
        m_popup_animations.clear();
        m_popup_scopes.clear();
        m_popup_click_pos.clear();
        m_popup_slide_pos.clear();
        m_popup_context = context;
    }
    m_popup_context_frame = context->FrameCount;
}

bool c_gui::is_popup_window(const ImGuiWindow* window) const {
    return (window->Flags & ImGuiWindowFlags_Popup) != 0 ||
        window->StateStorage.GetBool(ImHashStr("##PopupFadeWindow"));
}

float c_gui::popup_fade_alpha(ImGuiID id, bool open) {
    sync_popup_context();
    ImGuiContext& g = *GImGui;
    popup_animation_state& state = m_popup_animations[id];
    if (state.last_update != g.FrameCount) {
        // Only drop the fade when its owner has genuinely stopped drawing (tab switch, popup
        // torn down). A single skipped frame - a relayout, a clipped item - used to zero the
        // alpha here, which read as the popup blinking out mid-animation.
        if (state.last_update < g.FrameCount - 3)
            state.alpha = 0.f;
        // A single long frame (debug build, font atlas rebuild, first blur capture) used to
        // eat the whole fade in one step, which reads as the popup vanishing rather than
        // fading. Cap the step so a hitch only slows the animation down.
        const float dt = ImMin(g.IO.DeltaTime, 0.05f);
        if (open)
            state.alpha = ImLerp(state.alpha, 1.f, 1.f - expf(-15.f * dt));
        else
            state.alpha = ImMax(0.f, state.alpha - dt / 0.16f);
        if (!open && state.alpha < 0.01f)
            state.alpha = 0.f;
        state.last_update = g.FrameCount;
    }
    state.was_open = open;
    return state.alpha;
}

bool c_gui::popup_fade_visible(ImGuiID id) const {
    const auto it = m_popup_animations.find(id);
    return m_popup_context == ImGui::GetCurrentContext() && it != m_popup_animations.end() &&
        it->second.alpha > 0.f && it->second.last_render >= GImGui->FrameCount - 1;
}

float c_gui::popup_animation(const char* name, bool clicked, bool& wants_open) {
    sync_popup_context();
    ImGuiContext& g = *GImGui;
    const ImGuiID id = ImGui::GetID(name);
    popup_animation_state& state = m_popup_animations[id];
    bool open = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);
    const bool just_closed = state.was_open && state.last_update >= g.FrameCount - 1;
    if (clicked && !(g.CurrentItemFlags & ImGuiItemFlags_Disabled)) {
        if (open) {
            ImGui::ClosePopupToLevel(g.BeginPopupStack.Size, true);
            open = false;
        } else if (!just_closed && !popup_fade_visible(id)) {
            // Clicking the owner while the popup is up closes it on mouse-down (ImGui dismisses
            // popups the click lands outside of), but `clicked` only arrives on release - which
            // reopened it right back, so the close animation blinked out and returned. `was_open`
            // only covers a single frame while a click spans several, so the fade itself is the
            // gate: ignore presses until it has finished, then open normally again.
            ImGui::OpenPopupEx(id, ImGuiPopupFlags_None);
            open = true;
            state.drag = state.drag_target = ImVec2(0.f, 0.f); // a fresh open starts at its anchor
            state.dragging = false;
        }
    }
    wants_open = open;
    return popup_fade_alpha(id, open);
}

bool c_gui::begin_animated_popup(ImGuiID id, const char* window_name, ImGuiWindowFlags flags, float alpha) {
    sync_popup_context();
    ImGuiContext& g = *GImGui;
    const bool open = ImGui::IsPopupOpen(id, ImGuiPopupFlags_None);
    popup_animation_state& state = m_popup_animations[id];
    if ((!open && (alpha <= 0.f || state.last_render < g.FrameCount - 1)) ||
        (open && alpha <= 0.f)) {
        g.NextWindowData.ClearFlags();
        return false;
    }
    if (open) {
        // Same bug as the menu had, one level down: clicking a parent card while its child popup
        // (sort menu, combo list, color picker...) is open focuses the parent in EndFrame(), and
        // focusing brings it to the front of the draw order *before that frame renders* - so the
        // child spent its closing frame behind the parent card and blinked. Cards never reorder
        // on focus; each one is raised once, on the frame it opens, below.
        flags |= ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoBringToFrontOnFocus;
    } else {
        // Closed popups only retain their visual lifetime; they never rejoin the popup stack.
        flags &= ~(ImGuiWindowFlags_Popup | ImGuiWindowFlags_AlwaysAutoResize);
        flags |= ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::SetNextWindowPos(state.pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(state.size, ImGuiCond_Always);
        g.NextWindowData.Flags &= ~ImGuiNextWindowDataFlags_HasSizeConstraint;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.f);
    ImGui::BeginDisabled(!open);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, g.Style.PopupRounding);
    const bool visible = ImGui::Begin(window_name, nullptr, flags);
    ImGui::PopStyleVar();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    window->StateStorage.SetBool(ImHashStr("##AnimatedPopupSurface"), true);
    window->StateStorage.SetBool(ImHashStr("##PopupFadeWindow"), !open);
    if (open && !state.rendered_open)
        ImGui::BringWindowToDisplayFront(window); // the newest card goes on top - children open after parents
    state.rendered_open = open;
    state.last_render = g.FrameCount;
    if (open) {
        state.pos = window->Pos;
        state.size = window->Size;
    }
    m_popup_scopes.push_back({ id, open });
    if (!visible) {
        end_animated_popup();
        return false;
    }
    return true;
}

void c_gui::end_animated_popup() {
    IM_ASSERT(!m_popup_scopes.empty());
    const popup_scope scope = m_popup_scopes.back();
    m_popup_scopes.pop_back();
    if (scope.open)
        ImGui::EndPopup();
    else
        ImGui::End();
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
}

bool c_gui::begin_popup_card(const char* popup_name, bool& wants_open, float anim, ImVec2 click_pos, float width, bool allow_outside, bool exact_pos) {
    if (!wants_open && anim <= 0.f) return false;

    ImGuiContext& g = *GImGui;
    ImGuiID id = ImGui::GetID(popup_name);
    
    int my_idx = -1;
    for (int i = 0; i < g.OpenPopupStack.Size; i++) {
        if (g.OpenPopupStack[i].PopupId == id) {
            my_idx = i;
            break;
        }
    }
    
    float popup_w = width * m_scale;
    ImVec2 default_size(popup_w, 200.f * m_scale);
    
    if (my_idx != -1) {
        ImGuiWindow* win = g.OpenPopupStack[my_idx].Window;
        ImVec2 my_size = win ? win->Size : default_size;
        ImVec2 target = click_pos;
        // Offsets that depend on the card's own height go through the pivot instead of my_size:
        // on the frame after the hidden auto-fit pass my_size is still the measuring stub, which
        // placed the card ~135px off for one visible frame. ImGui resolves the pivot after the
        // real size is known, so there is nothing to catch up on.
        ImVec2 pivot(0.f, 0.f);

        if (ImGuiWindow* main_win = ImGui::FindWindowByName("Hello, world!")) {
            const float top = main_win->Pos.y, bottom = main_win->Pos.y + main_win->Size.y;
            if (exact_pos) {
                // click_pos is an anchor the caller picked (e.g. just under its button), not the
                // cursor - place the card there verbatim, only kept inside the menu.
                target.x = ImClamp(target.x, main_win->Pos.x + 10.f, ImMax(main_win->Pos.x + 10.f, main_win->Pos.x + main_win->Size.x - my_size.x - 10.f));
                target.y = ImClamp(target.y, top + 10.f, ImMax(top + 10.f, bottom - my_size.y - 10.f));
            } else if (allow_outside) {
                target.x += 45.f * m_scale;
                pivot.y = 0.5f; // centred on the row it belongs to
                target.y = ImClamp(target.y, top + 16.f + my_size.y * 0.5f, ImMax(top + 16.f + my_size.y * 0.5f, bottom - 16.f - my_size.y * 0.5f));
            } else {
                target.x = ImClamp(target.x, main_win->Pos.x + 10.f, main_win->Pos.x + main_win->Size.x - my_size.x - 10.f);
                pivot.y = 0.1f;
                target.y = ImClamp(target.y, top + 16.f + my_size.y * 0.1f, ImMax(top + 16.f + my_size.y * 0.1f, bottom - 16.f - my_size.y * 0.9f));
            }
        }

        ImGui::SetNextWindowPos(target + m_popup_animations[id].drag, ImGuiCond_Always, pivot);
    }

    return begin_popup_card_body(popup_name, id, anim, width);
}

bool c_gui::begin_popup_card_at(const char* popup_name, bool& wants_open, float anim, ImVec2 anchor, ImVec2 pivot, float width, float padding) {
    if (!wants_open && anim <= 0.f) return false;

    const ImGuiID id = ImGui::GetID(popup_name);
    if (ImGui::IsPopupOpen(id, ImGuiPopupFlags_None)) {
        // Keep the card on screen. The height isn't known until the card has been laid out once,
        // so the clamp uses the size remembered from the last time it was open (ImGui still
        // resolves the pivot against the real size, so the anchor itself is always exact).
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const auto it = m_popup_animations.find(id);
        const ImVec2 known(width * m_scale, (it != m_popup_animations.end()) ? it->second.size.y : 0.f);
        const float margin = 10.f * m_scale;
        ImVec2 top_left = anchor - known * pivot;
        top_left.x = ImClamp(top_left.x, margin, ImMax(margin, display.x - known.x - margin));
        top_left.y = ImClamp(top_left.y, margin, ImMax(margin, display.y - known.y - margin));
        ImGui::SetNextWindowPos(top_left + known * pivot + m_popup_animations[id].drag, ImGuiCond_Always, pivot);
    }

    return begin_popup_card_body(popup_name, id, anim, width, padding);
}

bool c_gui::begin_popup_card_body(const char* popup_name, ImGuiID id, float anim, float width, float padding) {
    ImGuiContext& g = *GImGui;
    const float popup_w = width * m_scale;
    ImGui::SetNextWindowSize(ImVec2(popup_w, 0)); // Fixed width, auto height fit
    const float surface_alpha = g.Style.Alpha * anim;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, surface_alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding * m_scale, padding * m_scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.f * m_scale);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
    
    char window_name[32];
    ImFormatString(window_name, IM_ARRAYSIZE(window_name), "##Popup_%08x", id);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;
    const bool open = begin_animated_popup(id, window_name, flags, anim);
    if (open) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        popup_surface(window->DrawList, window->Pos, window->Pos + window->Size,
            15.f * m_scale, surface_alpha, std::strcmp(popup_name, "UserPopupAnim") != 0);
    }

    if (!open) {
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
    }
    
    return open;
}

void c_gui::end_popup_card() {
    // Drag the card by its empty space (anything that isn't a widget). Evaluated here, after
    // the card's content, so this frame's hovered item is known. NoPopupHierarchy: a child
    // popup opened from this card is not "this card" for the purpose of grabbing it.
    if (!m_popup_scopes.empty() && m_popup_scopes.back().open) {
        ImGuiIO& io = ImGui::GetIO();
        popup_animation_state& state = m_popup_animations[m_popup_scopes.back().id];
        if (!state.dragging && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy))
            state.dragging = true;
        if (state.dragging) {
            if (ImGui::IsMouseDown(0))
                state.drag_target += io.MouseDelta;
            else
                state.dragging = false;
        }
        state.drag = ImLerp(state.drag, state.drag_target, 1.f - expf(-20.f * ImMin(io.DeltaTime, 0.05f)));
        if (ImLengthSqr(state.drag_target - state.drag) < 0.25f)
            state.drag = state.drag_target;
    }
    end_animated_popup();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

ImVec2 c_gui::popup_click_pos( const char* name, bool clicked, ImVec2 anchor ) {
    // The card opens from where the click landed, not from wherever the cursor drifted to
    // on a later frame - so the point is captured once and replayed every frame after.
    // An explicit anchor (a button's corner, say) is stored in its place.
    const ImGuiID id = ImGui::GetID( name );
    if ( clicked )
        m_popup_click_pos[ id ] = ( anchor.x > -FLT_MAX ) ? anchor : ImGui::GetIO( ).MousePos;
    const auto it = m_popup_click_pos.find( id );
    return it != m_popup_click_pos.end( ) ? it->second : ImVec2( -1.f, -1.f );
}

void c_gui::popup_position( const char* name, ImVec2 click_pos, ImVec2 default_size, float anim, bool beside ) {
    ImGuiContext& g = *GImGui;
    const ImGuiID id = ImGui::GetID( name );

    int my_idx = -1;
    for ( int i = 0; i < g.OpenPopupStack.Size; i++ )
        if ( g.OpenPopupStack[ i ].PopupId == id ) { my_idx = i; break; }
    if ( my_idx == -1 )
        return;

    ImGuiWindow* win = g.OpenPopupStack[ my_idx ].Window;
    const ImVec2 my_size = win ? win->Size : default_size;
    ImGuiWindow* main_win = ImGui::FindWindowByName( "Hello, world!" );

    ImVec2 target = click_pos;
    if ( click_pos.x == -1.f && click_pos.y == -1.f ) // never clicked: fall back to the menu's center
        target = main_win ? ( main_win->Pos + main_win->Size * 0.5f - my_size * 0.5f ) : ImVec2( 0.f, 0.f );
    else if ( main_win ) {
        if ( beside ) {
            target.x += 45.f * m_scale;
            target.y = ImClamp( target.y - 20.f, main_win->Pos.y + 10.f, main_win->Pos.y + main_win->Size.y - my_size.y - 10.f );
        } else {
            target.x = ImClamp( target.x, main_win->Pos.x + 10.f, main_win->Pos.x + main_win->Size.x - my_size.x - 10.f );
            target.y = ImClamp( target.y, main_win->Pos.y + 10.f, main_win->Pos.y + main_win->Size.y - my_size.y - 10.f );
        }
    }

    target.y += ( 1.f - anim ) * 20.f; // slide down while fading in

    ImVec2& current = m_popup_slide_pos[ id ];
    if ( !win || win->Hidden )
        current = target + ImVec2( 0.f, 15.f );
    current = ImLerp( current, target, g.IO.DeltaTime * 12.f );
    ImGui::SetNextWindowPos( current, ImGuiCond_Always );
}

bool c_gui::popup_begin( const char* name, bool clicked, float width, bool titled, bool beside ) {
    const std::string popup_name = std::string( name ) + "Popup"; // sub_menu() looks for this exact id
    const ImVec2 click_pos = popup_click_pos( popup_name.c_str( ), clicked );

    bool wants_open = false;
    const float anim = popup_animation( popup_name.c_str( ), clicked, wants_open );

    if ( !begin_popup_card( popup_name.c_str( ), wants_open, anim, click_pos, width, beside ) )
        return false;

    if ( titled ) {
        ImFontAtlas* fonts = ImGui::GetIO( ).Fonts;
        ImGui::PushFont( fonts->Fonts.Size > 4 ? fonts->Fonts[ 4 ] : ImGui::GetFont( ) );
        ImGui::TextColored( text.to_vec4( 1.f, false ), "%s", tr( name ) );
        ImGui::PopFont( );
        ImGui::Dummy( ImVec2( 0.f, 6.f * m_scale ) );
    }
    return true;
}

void c_gui::popup_end( ) {
    end_popup_card( );
}

bool c_gui::row_popup( const char* label, const char* description, float width ) {
    const bool clicked = row_sub_menu( label, description );
    return popup_begin( label, clicked, width, true, true );
}


// ---------------------------------------------------------------------------------------
//  Config browser
// ---------------------------------------------------------------------------------------

static std::unordered_map<ImGuiID, float>& cfg_anims( ) {
    static std::unordered_map<ImGuiID, float> anims;
    return anims;
}

// Read this frame's hover amount without touching it - lets a row paint its background
// before the items that decide whether it is hovered have even been submitted.
static float cfg_anim_get( ImGuiID id ) {
    const auto it = cfg_anims( ).find( id );
    return it != cfg_anims( ).end( ) ? it->second : 0.f;
}

static float cfg_anim( ImGuiID id, bool active, float speed = 15.f ) {
    float& a = cfg_anims( )[ id ];
    a = ImLerp( a, active ? 1.f : 0.f, GImGui->IO.DeltaTime * speed );
    return a;
}

// Baseline that puts a string's ink - not its em box - on the centre line of a `h` tall row.
static float cfg_text_y( float top, float h, const char* text, ImFont* font = nullptr, float size = 0.f ) {
    if ( !font ) font = ImGui::GetFont( );
    if ( size <= 0.f ) size = ImGui::GetFontSize( );
    IM_UNUSED( text );
    return IM_ROUND( top + h * 0.5f - TextLineCenterY( font, size ) );
}

static ImU32 cfg_mix( const ImVec4& a, const ImVec4& b, float t ) {
    return ImGui::GetColorU32( ImVec4( a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t,
                                       a.z + ( b.z - a.z ) * t, a.w + ( b.w - a.w ) * t ) );
}

static std::string cfg_lower( std::string s ) {
    for ( char& c : s )
        c = (char)( ( c >= 'A' && c <= 'Z' ) ? c - 'A' + 'a' : c );
    return s;
}

// Icon-only button: screen-space rect, glyph centred, brightening from `idle` to `hot`.
// `bg` paints an optional plate behind it (pass 0 for a bare glyph).
static bool cfg_icon_button( const char* id, const char* icon, ImVec2 pos, ImVec2 size,
                             const ImVec4& idle, const ImVec4& hot, ImU32 bg = 0, float rounding = 0.f ) {
    ImGui::SetCursorScreenPos( pos );
    const ImGuiID item_id = ImGui::GetID( id );
    const bool clicked = ImGui::InvisibleButton( id, size );
    const float t = cfg_anim( item_id, ImGui::IsItemHovered( ) );

    ImDrawList* draw = ImGui::GetWindowDrawList( );
    if ( ( bg >> IM_COL32_A_SHIFT ) != 0 )
        draw->AddRectFilled( pos, pos + size, bg, rounding );
    const ImVec2 tsz = ImGui::CalcTextSize( icon );
    draw->AddText( ImVec2( IM_ROUND( pos.x + ( size.x - tsz.x ) * 0.5f ),
                           IM_ROUND( pos.y + ( size.y - tsz.y ) * 0.5f ) ), cfg_mix( idle, hot, t ), icon );
    return clicked;
}

// One line of a small popup menu: icon + label, full width, brightening on hover.
static bool cfg_menu_row( const char* id, const char* icon, const char* label, float w, float h,
                          const ImVec4& idle, const ImVec4& hot ) {
    const ImVec2 pos = ImGui::GetCursorScreenPos( );
    const ImGuiID item_id = ImGui::GetID( id );
    const bool clicked = ImGui::InvisibleButton( id, ImVec2( w, h ) );
    const float t = cfg_anim( item_id, ImGui::IsItemHovered( ) );
    const ImU32 col = cfg_mix( idle, hot, t );
    const float pad = 10.f * gui.m_scale;
    label = gui.tr( label );

    ImDrawList* draw = ImGui::GetWindowDrawList( );
    if ( icon ) {
        const ImVec2 isz = ImGui::CalcTextSize( icon );
        draw->AddText( ImVec2( pos.x + pad, IM_ROUND( pos.y + ( h - isz.y ) * 0.5f ) ), col, icon );
    }
    draw->AddText( ImVec2( pos.x + pad + ( icon ? 26.f * gui.m_scale : 0.f ),
                           cfg_text_y( pos.y, h, label ) ), col, label );
    ImGui::SetCursorScreenPos( ImVec2( pos.x, pos.y + h ) );
    return clicked;
}

// Card width (unscaled, as begin_popup_card wants it) that fits the longest option in the
// current language, with room for the check and the slide it pushes the label through.
static float cfg_choice_width(const char* const* labels, int count, float minimum = 170.f) {
    float widest = 0.f;
    for (int i = 0; i < count; ++i) {
        const char* shown = gui.tr(labels[i]);
        widest = ImMax(widest, ImGui::CalcTextSize(shown, ImGui::FindRenderedTextEnd(shown)).x);
    }
    return ImMax(minimum, widest / gui.m_scale + 64.f); // 10 pad + 22 slide + 12 pad + 2x10 card padding
}

static bool cfg_choice_row(const char* id, const char* label, float w, float h, int* selected, int option) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImGuiID item_id = ImGui::GetID(id);
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h));
    if (clicked) *selected = option;
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID key = ImHashStr("##ChoiceSlide", 0, item_id);
    const float target = *selected == option ? 1.f : 0.f;
    float t = storage->GetFloat(key, target);
    t = ImLerp(t, target, 1.f - expf(-18.f * ImGui::GetIO().DeltaTime));
    storage->SetFloat(key, t);
    const float hover = cfg_anim(item_id, ImGui::IsItemHovered());
    const float s = gui.m_scale;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (hover > 0.001f)
        draw->AddRectFilled(pos, pos + ImVec2(w, h), gui.frame_inactive.to_im_color(hover * 0.6f), 6.f * s);
    const float check_size = 9.f * s;
    ImGui::RenderCheckMark(draw, pos + ImVec2(10.f * s, (h - check_size) * 0.5f),
        gui.text.to_im_color(t), check_size);
    const char* shown = gui.tr(label);
    draw->AddText(ImVec2(pos.x + (10.f + 22.f * t) * s, cfg_text_y(pos.y, h, shown)),
        cfg_mix(gui.text_disabled.to_vec4(1.f, false), gui.text.to_vec4(1.f, false), ImMax(t, hover)), shown);
    ImGui::SetCursorScreenPos(pos + ImVec2(0.f, h));
    return clicked;
}

bool c_gui::row_options_begin( const char* id, float width ) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( window->SkipItems )
        return false;

    const float s = m_scale;
    // Same slot the colour swatch uses - just left of the toggle the row is about to draw.
    const ImRect slot = next_checkbox_swatch_rect( );
    const ImRect bb( ImVec2( slot.Min.x - 3.f * s, slot.Min.y - 2.f * s ),
                     ImVec2( slot.Max.x + 3.f * s, slot.Max.y + 2.f * s ) );

    ImGui::PushID( id );
    const std::string popup_name = std::string( id ) + "Popup";
    const ImGuiID item_id = ImGui::GetID( "##dots" );
    // ItemAdd without ItemSize, and before the checkbox: the dots take their own click instead
    // of toggling the row they sit on.
    bool clicked = false, hovered = false, held = false;
    if ( ImGui::ItemAdd( bb, item_id ) )
        clicked = ImGui::ButtonBehavior( bb, item_id, &hovered, &held );
    const bool up = ImGui::IsPopupOpen( ImGui::GetID( popup_name.c_str( ) ), ImGuiPopupFlags_None );
    const float t = cfg_anim( item_id, hovered || up );

    ImDrawList* draw = window->DrawList;
    if ( t > 0.01f )
        draw->AddRectFilled( bb.Min, bb.Max, frame_inactive.to_im_color( 0.85f * t ), 6.f * s );
    const ImU32 col = cfg_mix( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), ImMax( t, up ? 1.f : 0.f ) );
    const ImVec2 c = bb.GetCenter( );
    for ( int i = -1; i <= 1; ++i )
        draw->AddCircleFilled( ImVec2( IM_ROUND( c.x + i * 4.f * s ), IM_ROUND( c.y ) ), 1.4f * s, col, 12 );

    bool wants_open = false;
    const float anim = popup_animation( popup_name.c_str( ), clicked, wants_open );
    const ImVec2 click_pos = popup_click_pos( popup_name.c_str( ), clicked, ImVec2( bb.Max.x, c.y ) );
    if ( begin_popup_card( popup_name.c_str( ), wants_open, anim, click_pos, width, true ) )
        return true; // PopID in row_options_end()
    ImGui::PopID( );
    return false;
}

void c_gui::row_options_end( ) {
    end_popup_card( );
    ImGui::PopID( );
}

// Segmented icon bar (the Players tab's header): the active segment opens up to show its label,
// the others stay icon-only, and the widths ease between the two. Right-aligned on `top_right`.
// `trailing_icon` is an extra button of its own on the end; true on the frame it is clicked.
int c_gui::icon_tabs( const char* id, const char* const* icons, const char* const* labels, int count,
                      int* active, ImVec2 top_right, const char* trailing_icon, int held ) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( window->SkipItems || count <= 0 || !active )
        return -1;
    // The bar is painted over the layout, not inside it: whatever is drawn next starts where it
    // would have without the bar.
    const ImVec2 saved_cursor = window->DC.CursorPos;
    const ImVec2 saved_cursor_max = window->DC.CursorMaxPos;
    int clicked_index = -1;

    const float s = m_scale, h = 26.f * s, pad = 9.f * s, gap = 4.f * s;
    ImFont* font = ImGui::GetFont( );
    const float fs = ImGui::GetFontSize( );
    ImDrawList* draw = window->DrawList;
    *active = ImClamp( *active, 0, count - 1 );

    ImGui::PushID( id );

    // Measure first: the bar hangs off its right edge, so every segment's width has to be known
    // before the first one is drawn.
    float widths[ 8 ] = {};
    float total = 0.f;
    const int n = ImMin( count, 8 );
    for ( int i = 0; i < n; ++i ) {
        ImGui::PushID( i );
        const ImGuiID seg_id = ImGui::GetID( "##seg" );
        ImGui::PopID( );
        const float open_t = cfg_anim_get( ImHashStr( "##open", 0, seg_id ) );
        const ImVec2 isz = font->CalcTextSizeA( fs, FLT_MAX, 0.f, icons[ i ] );
        const ImVec2 lsz = font->CalcTextSizeA( fs, FLT_MAX, 0.f, tr( labels[ i ] ) );
        widths[ i ] = pad * 2.f + isz.x + open_t * ( 6.f * s + lsz.x );
        total += widths[ i ] + gap;
    }
    float trailing_w = 0.f;
    if ( trailing_icon ) {
        trailing_w = pad * 2.f + font->CalcTextSizeA( fs, FLT_MAX, 0.f, trailing_icon ).x;
        total += trailing_w + gap * 2.f;
    }
    total -= gap;

    ImVec2 p( top_right.x - total, top_right.y );
    bool trailing_clicked = false;

    for ( int i = 0; i < n; ++i ) {
        ImGui::PushID( i );
        const ImGuiID seg_id = ImGui::GetID( "##seg" );
        ImGui::SetCursorScreenPos( p );
        const bool clicked = ImGui::InvisibleButton( "##seg", ImVec2( widths[ i ], h ) );
        const bool hovered = ImGui::IsItemHovered( );
        if ( clicked ) {
            *active = i;
            clicked_index = i;
        }
        const float open_t = cfg_anim( ImHashStr( "##open", 0, seg_id ), *active == i, 14.f );
        const float hot = ImMax( cfg_anim( seg_id, hovered ), i == held ? 1.f : 0.f );
        const ImRect bb( p, p + ImVec2( widths[ i ], h ) );

        const float plate = ImMax( open_t, hot * 0.5f );
        if ( plate > 0.01f )
            draw->AddRectFilled( bb.Min, bb.Max, frame_inactive.to_im_color( 0.9f * plate ), 8.f * s );

        ImVec4 col = ImLerp( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), hot );
        col = ImLerp( col, accent_color.to_vec4( 1.f, false ), open_t );
        const ImU32 ink = ImGui::GetColorU32( col );

        const ImVec2 isz = font->CalcTextSizeA( fs, FLT_MAX, 0.f, icons[ i ] );
        const float icon_x = bb.Min.x + pad;
        draw->AddText( font, fs, ImVec2( IM_ROUND( icon_x ),
            IM_ROUND( bb.GetCenter( ).y - TextInkCenterY( font, fs, icons[ i ] ) ) ), ink, icons[ i ] );

        if ( open_t > 0.01f ) { // label slides out of the icon as the segment opens
            const char* shown = tr( labels[ i ] );
            draw->PushClipRect( bb.Min, bb.Max, true );
            draw->AddText( font, fs, ImVec2( IM_ROUND( icon_x + isz.x + 6.f * s ),
                IM_ROUND( bb.GetCenter( ).y - TextLineCenterY( font, fs ) ) ),
                ImGui::GetColorU32( ImVec4( col.x, col.y, col.z, col.w * open_t ) ), shown );
            draw->PopClipRect( );
        }
        ImGui::PopID( );
        p.x += widths[ i ] + gap;
    }

    if ( trailing_icon ) {
        p.x += gap;
        ImGui::SetCursorScreenPos( p );
        const ImGuiID t_id = ImGui::GetID( "##trailing" );
        trailing_clicked = ImGui::InvisibleButton( "##trailing", ImVec2( trailing_w, h ) );
        if ( trailing_clicked )
            clicked_index = count;
        const float hot = cfg_anim( t_id, ImGui::IsItemHovered( ) );
        const ImRect bb( p, p + ImVec2( trailing_w, h ) );
        if ( hot > 0.01f )
            draw->AddRectFilled( bb.Min, bb.Max, frame_inactive.to_im_color( 0.45f * hot ), 8.f * s );
        draw->AddText( font, fs, ImVec2( IM_ROUND( bb.Min.x + pad ),
            IM_ROUND( bb.GetCenter( ).y - TextInkCenterY( font, fs, trailing_icon ) ) ),
            cfg_mix( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), hot ), trailing_icon );
    }

    ImGui::PopID( );
    window->DC.CursorPos = saved_cursor;
    window->DC.CursorMaxPos = saved_cursor_max;
    IM_UNUSED( trailing_clicked );
    return clicked_index;
}

// Cap band of 'H', in the units the text is actually drawn at: Y1 - Y0 is the cap height. A
// wordmark is stacked on its caps, not on its em boxes, or the two lines drift apart.
static float glyph_cap_h( const ImFont* font, float fs ) {
    const ImFontGlyph* g = font->FindGlyph( ( ImWchar )'H' );
    return g ? ( g->Y1 - g->Y0 ) * ( fs / font->FontSize ) : fs * 0.70f;
}

// The wordmark above the sidebar's divider. Every constant here is an unscaled pixel because the
// strip belongs to the menu chrome (imgui.cpp draws the side panel 170 wide and its divider at
// y = 60 at every menu scale) while the fonts are baked scaled - so the lockup is measured once
// against the strip and shrunk to fit instead.
void c_gui::name_plate( ImVec2 min, float width ) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( window->SkipItems )
        return;

    ImDrawList*  draw  = window->DrawList;
    ImFontAtlas* atlas = ImGui::GetIO( ).Fonts;
    ImFont* top_font = atlas->Fonts[ 4 ]; // Inter-SemiBold 17
    ImFont* bot_font = atlas->Fonts[ 6 ]; // Inter-SemiBold 12.5

    static const char* const k_top = "KILLARK";
    static const char* const k_bot = "DREAM TEAM";

    const ImVec2 rmin( min.x + 6.f, min.y + 5.f ), rmax( min.x + width - 6.f, min.y + 55.f );
    const float  bar_w = 3.f, bar_gap = 11.f;
    const float  tx0   = IM_ROUND( rmin.x + 8.f + bar_w + bar_gap );
    const float  avail = ImMax( rmax.x - tx0 - 4.f, 1.f );

    // tracking is part of the mark, so it is measured with it and shrinks with it
    float fs_top = top_font->FontSize, fs_bot = bot_font->FontSize;
    float tr_top = 0.055f * fs_top, tr_bot = 0.20f * fs_bot;
    const float raw_top = top_font->CalcTextSizeA( fs_top, FLT_MAX, 0.f, k_top ).x + tr_top * 6.f;
    const float raw_bot = bot_font->CalcTextSizeA( fs_bot, FLT_MAX, 0.f, k_bot ).x + tr_bot * 9.f;

    float k = 1.f;
    if ( raw_top > avail ) k = ImMin( k, avail / raw_top );
    if ( raw_bot > avail ) k = ImMin( k, avail / raw_bot );
    const float probe_h = glyph_cap_h( top_font, fs_top ) + 0.42f * fs_bot + glyph_cap_h( bot_font, fs_bot );
    if ( probe_h > 40.f ) k = ImMin( k, 40.f / probe_h );
    if ( k < 1.f ) {
        fs_top *= k; fs_bot *= k; tr_top *= k; tr_bot *= k;
    }

    const float cap_top = glyph_cap_h( top_font, fs_top ), cap_bot = glyph_cap_h( bot_font, fs_bot );
    const float gap_y   = 0.42f * fs_bot;
    const float block_h = cap_top + gap_y + cap_bot;
    const float cy      = ( rmin.y + rmax.y ) * 0.5f;
    const float top     = cy - block_h * 0.5f;
    const float y_top   = IM_ROUND( top + cap_top * 0.5f - TextLineCenterY( top_font, fs_top ) );
    const float y_bot   = IM_ROUND( top + cap_top + gap_y + cap_bot * 0.5f - TextLineCenterY( bot_font, fs_bot ) );

    // The rule carries the accent; the mark itself stays plain so the sidebar under it keeps
    // reading as the loudest thing on the screen.
    const ImVec2 bmin( IM_ROUND( rmin.x + 8.f ), IM_ROUND( top ) );
    //draw->AddRectFilled( bmin, ImVec2( bmin.x + bar_w, IM_ROUND( top + block_h ) ),
    //    accent_color.to_im_color( ), bar_w * 0.5f );

    // Tracked by hand: ImGui has no letter-spacing, and a wordmark without it reads as body text.
    const auto tracked = [ & ]( ImFont* font, float fs, float y, float track, ImU32 col, const char* s ) {
        float x = tx0;
        for ( const char* p = s; *p; ++p ) {
            if ( *p != ' ' )
                draw->AddText( font, fs, ImVec2( IM_ROUND( x ), y ), col, p, p + 1 );
            x += font->CalcTextSizeA( fs, FLT_MAX, 0.f, p, p + 1 ).x + track;
        }
    };

    draw->PushClipRect( ImVec2( tx0, rmin.y ), rmax, true );
    tracked( top_font, fs_top, y_top, tr_top, text.to_im_color( ), k_top );
    tracked( bot_font, fs_bot, y_bot, tr_bot, text.to_im_color( 0.62f ), k_bot );
    draw->PopClipRect( );
}

void c_gui::chevron_handle( ImGuiID id, ImVec2 pos, ImVec2 size, bool open ) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( window->SkipItems )
        return;
    const float s = m_scale;
    ImDrawList* draw = window->DrawList;
    const float hot = cfg_anim( id, ImGui::IsItemHovered( ) || open );
    const float turn = cfg_anim( ImHashStr( "##turn", 0, id ), open, 14.f );

    draw->AddRectFilled( pos, pos + size, frame_inactive.to_im_color( 0.55f + 0.35f * hot ),
        size.y * 0.5f, ImDrawFlags_RoundCornersTop );

    // A chevron drawn by hand so it can turn over as the card opens.
    const ImVec2 c( pos.x + size.x * 0.5f, pos.y + size.y * 0.5f );
    const float dir = ImLerp( 1.f, -1.f, turn ); // points up while closed, turns over once open
    const float half = 4.5f * s, rise = 3.f * s;
    const ImU32 col = cfg_mix( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), hot );
    const ImVec2 pts[ 3 ] = { ImVec2( c.x - half, c.y + rise * dir ), ImVec2( c.x, c.y - rise * dir ),
                              ImVec2( c.x + half, c.y + rise * dir ) };
    draw->AddPolyline( pts, 3, col, ImDrawFlags_None, 1.6f * s );
}

// Wrapped pill toggles - the ESP Items panel is built out of these.
void c_gui::chips( const char* id, const char* const* labels, bool* values, int count ) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( window->SkipItems || count <= 0 )
        return;

    const float s = m_scale, h = 24.f * s, pad = 11.f * s, gap = 6.f * s, gap_y = 10.f * s;
    const float avail = ImGui::GetContentRegionAvail( ).x;
    ImFont* font = ImGui::GetFont( );
    const float fs = ImGui::GetFontSize( );
    ImDrawList* draw = window->DrawList;
    const ImVec2 origin = ImGui::GetCursorScreenPos( );
    float x = 0.f, y = 0.f;

    ImGui::PushID( id );
    for ( int i = 0; i < count; ++i ) {
        const char* shown = tr( labels[ i ] );
        const float w = font->CalcTextSizeA( fs, FLT_MAX, 0.f, shown ).x + pad * 2.f;
        if ( x > 0.f && x + w > avail ) { // next line
            x = 0.f;
            y += h + gap_y;
        }
        const ImVec2 cp = origin + ImVec2( x, y );
        ImGui::PushID( i );
        ImGui::SetCursorScreenPos( cp );
        const ImGuiID chip_id = ImGui::GetID( "##chip" );
        if ( ImGui::InvisibleButton( "##chip", ImVec2( w, h ) ) )
            values[ i ] = !values[ i ];
        const float hot = cfg_anim( chip_id, ImGui::IsItemHovered( ) );
        const ImGuiID on_key = ImHashStr( "##on", 0, chip_id );
        if ( cfg_anims( ).find( on_key ) == cfg_anims( ).end( ) )
            cfg_anims( )[ on_key ] = values[ i ] ? 1.f : 0.f; // first sight: no animation into state
        const float on = cfg_anim( on_key, values[ i ], 14.f );

        const ImVec4 off_bg = frame_inactive.to_vec4( 0.55f + 0.35f * hot, false );
        const ImVec4 on_bg  = accent_color.to_vec4( 0.22f + 0.08f * hot, false );
        draw->AddRectFilled( cp, cp + ImVec2( w, h ), ImGui::GetColorU32( ImLerp( off_bg, on_bg, on ) ), 8.f * s );
        if ( on > 0.01f )
            draw->AddRect( cp, cp + ImVec2( w, h ), accent_color.to_im_color( 0.55f * on ), 8.f * s, 0, 1.f * s );

        ImVec4 col = ImLerp( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), hot );
        col = ImLerp( col, accent_color.to_vec4( 1.f, false ), on );
        draw->AddText( font, fs, ImVec2( IM_ROUND( cp.x + pad ),
            IM_ROUND( cp.y + h * 0.5f - TextLineCenterY( font, fs ) ) ), ImGui::GetColorU32( col ), shown );
        ImGui::PopID( );
        x += w + gap;
    }
    ImGui::PopID( );

    ImGui::SetCursorScreenPos( origin );
    ImGui::Dummy( ImVec2( avail, y + h + 4.f * s ) ); // breathing room before whatever follows
}

// A panel of its own, floating over the menu (the Ambience window): same surface as a popup card,
// a "..." and a close button in its corner, dragged by any empty space. Unlike a popup it stays
// up until it is closed, so clicking around the menu behind it does nothing to it.
bool c_gui::window_begin( const char* id, bool* open, ImVec2 size, bool* dots_clicked ) {
    if ( !open )
        return false;
    sync_popup_context( );

    ImGuiContext& g = *GImGui;
    const ImGuiID wid = ImGui::GetID( id );
    const float alpha = popup_fade_alpha( wid, *open );
    if ( alpha <= 0.f ) {
        m_window_open_frame.erase( wid );
        return false;
    }

    const float s = m_scale;
    const float base_alpha = g.Style.Alpha; // see overlay_begin(): read it before pushing the fade
    const ImVec2 full = size * s;
    popup_animation_state& state = m_popup_animations[ wid ];

    // First frame up: centre it on the menu (and keep whatever the user dragged it to after that).
    if ( m_window_open_frame.find( wid ) == m_window_open_frame.end( ) ) {
        ImGuiWindow* menu = ImGui::FindWindowByName( "Hello, world!" );
        const ImVec2 center = menu ? menu->Pos + menu->Size * 0.5f : ImGui::GetIO( ).DisplaySize * 0.5f;
        state.pos = center - full * 0.5f;
        state.drag = state.drag_target = ImVec2( 0.f, 0.f );
        state.dragging = false;
        m_window_open_frame[ wid ] = g.FrameCount;
    }

    ImGui::SetNextWindowPos( state.pos + state.drag, ImGuiCond_Always );
    ImGui::SetNextWindowSize( full, ImGuiCond_Always );
    ImGui::PushStyleVar( ImGuiStyleVar_Alpha, g.Style.Alpha * alpha );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 16.f * s, 14.f * s ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 8.f * s, 8.f * s ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) ); // the surface is drawn below
    char window_name[ 48 ];
    ImFormatString( window_name, IM_ARRAYSIZE( window_name ), "##Window_%08x", wid );
    ImGui::Begin( window_name, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing );

    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    ImGui::BringWindowToDisplayFront( window ); // above the menu, without taking its focus
    popup_surface( window->DrawList, window->Pos, window->Pos + window->Size, 16.f * s, base_alpha * alpha );

    // corner buttons: "..." and close
    {
        ImFont* font = ImGui::GetFont( );
        const float fs = ImGui::GetFontSize( ), btn = 22.f * s;
        const ImVec2 top_right( window->Pos.x + window->Size.x - 12.f * s, window->Pos.y + 10.f * s );
        const char* icons[ 2 ] = { ICON_FA_ELLIPSIS_H, ICON_FA_TIMES };
        for ( int i = 1; i >= 0; --i ) {
            const ImVec2 bmin( top_right.x - btn * ( 2 - i ) - ( i == 0 ? 2.f * s : 0.f ), top_right.y );
            ImGui::SetCursorScreenPos( bmin );
            ImGui::PushID( i );
            const ImGuiID bid = ImGui::GetID( "##win_btn" );
            const bool pressed = ImGui::InvisibleButton( "##win_btn", ImVec2( btn, btn ) );
            const float t = cfg_anim( bid, ImGui::IsItemHovered( ) );
            ImGui::PopID( );
            if ( t > 0.01f )
                window->DrawList->AddRectFilled( bmin, bmin + ImVec2( btn, btn ), frame_inactive.to_im_color( 0.7f * t ), 6.f * s );
            const ImVec2 isz = font->CalcTextSizeA( fs, FLT_MAX, 0.f, icons[ i ] );
            window->DrawList->AddText( font, fs, ImVec2( IM_ROUND( bmin.x + ( btn - isz.x ) * 0.5f ),
                IM_ROUND( bmin.y + btn * 0.5f - TextInkCenterY( font, fs, icons[ i ] ) ) ),
                cfg_mix( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), t ), icons[ i ] );
            if ( pressed ) {
                if ( i == 1 ) *open = false;
                else if ( dots_clicked ) *dots_clicked = true;
            }
        }
        ImGui::SetCursorScreenPos( window->Pos + ImVec2( 16.f * s, 14.f * s + btn * 0.35f ) );
    }

    m_window_scopes.push_back( wid );
    m_row_started = false; // rows drawn straight into the window start a fresh run
    return true;
}

void c_gui::window_end( ) {
    ImGuiIO& io = ImGui::GetIO( );
    IM_ASSERT( !m_window_scopes.empty( ) );
    const ImGuiID wid = m_window_scopes.back( );
    m_window_scopes.pop_back( );
    popup_animation_state& state = m_popup_animations[ wid ];

    // Drag by empty space, eased like the menu and the popup cards.
    if ( !state.dragging && ImGui::IsMouseClicked( 0 ) && !ImGui::IsAnyItemHovered( ) &&
         ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_NoPopupHierarchy ) )
        state.dragging = true;
    if ( state.dragging ) {
        if ( ImGui::IsMouseDown( 0 ) )
            state.drag_target += io.MouseDelta;
        else
            state.dragging = false;
    }
    state.drag = ImLerp( state.drag, state.drag_target, 1.f - expf( -20.f * ImMin( io.DeltaTime, 0.05f ) ) );
    if ( ImLengthSqr( state.drag_target - state.drag ) < 0.25f )
        state.drag = state.drag_target;

    ImGui::End( );
    ImGui::PopStyleColor( );
    ImGui::PopStyleVar( 4 );
}

// A screen inside the menu: the whole content area (everything right of the sidebar) is covered
// by the menu's own blurred surface and the caller lays its panels out on top. Not a window and
// not a popup - the sidebar stays live behind it, and only its close button dismisses it.
bool c_gui::overlay_begin( const char* id, bool* open, bool* dots_clicked ) {
    if ( !open )
        return false;
    sync_popup_context( );

    ImGuiContext& g = *GImGui;
    const ImGuiID oid = ImGui::GetID( id );
    const float alpha = popup_fade_alpha( oid, *open );
    if ( alpha <= 0.f )
        return false;

    ImGuiWindow* menu = ImGui::FindWindowByName( "Hello, world!" );
    if ( !menu )
        return false;

    const float s = m_scale;
    const float base_alpha = g.Style.Alpha; // read before the fade is pushed, or it counts twice
    // 170 and 20, not 170 * s: the menu's own chrome (imgui.cpp, the hijacked NoBackground path)
    // paints its side panel and its corners at fixed pixel sizes, so scaling these would leave a
    // strip of the tab showing beside the panel at any menu scale other than 100%.
    const float side_w = 170.f;                           // the sidebar keeps working underneath
    const ImVec2 min( menu->Pos.x + side_w, menu->Pos.y );
    const ImVec2 max( menu->Pos.x + menu->Size.x, menu->Pos.y + menu->Size.y );

    // Blur and panel both fade, but not at the same time: on the way out the panel dissolves
    // while the blur still hides the tab completely, and only then does the blur itself clear
    // (opening runs the other way round). Fading the two together is a cross-dissolve of two full
    // interfaces and reads as a glitch - halfway through you can see both at once.
    const float content = ImSaturate( ( alpha - 0.45f ) / 0.55f );
    const float scrim   = ImSaturate( alpha / 0.45f );

    ImGui::SetNextWindowPos( min, ImGuiCond_Always );
    ImGui::SetNextWindowSize( max - min, ImGuiCond_Always );
    // Begin() gets the un-faded alpha: ImGui drops a whole window (HiddenFramesCanSkipItems) when
    // the style alpha is 0 at that moment, and the blur lives in this window. The content's own
    // alpha is pushed further down, once the blur has been drawn.
    ImGui::PushStyleVar( ImGuiStyleVar_Alpha, base_alpha );
    // One gap everywhere: the margin round the panel and the gap between its columns are the
    // same number, so the two children sit symmetrically inside it.
    const float gap = 14.f * s;
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( gap, gap ) );
    ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( gap, gap ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
    char window_name[ 48 ];
    ImFormatString( window_name, IM_ARRAYSIZE( window_name ), "##Overlay_%08x", oid );
    ImGui::Begin( window_name, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing );

    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    ImGui::BringWindowToDisplayFront( window ); // over the tab it replaces, under any popup it opens

    // The scrim: the same blurred surface every popup uses, with the menu's rounding on the right.
    ImDrawList* draw = window->DrawList;
    const float rounding = 20.f;
    draw->PushClipRect( min, max, false );
    // The blur samples the back buffer as it stands *now* - the tab underneath has already been
    // drawn, so it is the menu itself that gets blurred. The tint has to stay translucent or it
    // would paint straight over that.
    draw_blur_rounded( draw, min - ImVec2( rounding, 0.f ), max, base_alpha * scrim, rounding,
        ImColor( panel_body.r, panel_body.g, panel_body.b, 0.62f ) );
    draw->AddLine( ImVec2( min.x, min.y ), ImVec2( min.x, max.y ),
        border.to_im_color( 0.6f * base_alpha * scrim, false ), 1.f * s );
    draw->PopClipRect( );

    // Everything from here on is the panel itself. Clamped off zero so a group box's child window
    // is never skipped outright - a skipped box measures its auto height as empty and pops back
    // to the wrong size on the frame it returns.
    ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImMax( base_alpha * content, 0.0001f ) );

    // corner buttons: "..." and close
    {
        ImFont* font = ImGui::GetFont( );
        const float fs = ImGui::GetFontSize( ), btn = 24.f * s;
        const char* icons[ 2 ] = { ICON_FA_ELLIPSIS_H, ICON_FA_TIMES };
        const ImVec2 top_right( max.x - 14.f * s, min.y + 12.f * s );
        for ( int i = 1; i >= 0; --i ) {
            const ImVec2 bmin( top_right.x - btn * ( 2 - i ) - ( i == 0 ? 2.f * s : 0.f ), top_right.y );
            ImGui::SetCursorScreenPos( bmin );
            ImGui::PushID( i );
            const ImGuiID bid = ImGui::GetID( "##overlay_btn" );
            const bool pressed = ImGui::InvisibleButton( "##overlay_btn", ImVec2( btn, btn ) );
            const float t = cfg_anim( bid, ImGui::IsItemHovered( ) );
            ImGui::PopID( );
            if ( t > 0.01f )
                draw->AddRectFilled( bmin, bmin + ImVec2( btn, btn ), frame_inactive.to_im_color( 0.7f * t ), 6.f * s );
            const ImVec2 isz = font->CalcTextSizeA( fs, FLT_MAX, 0.f, icons[ i ] );
            draw->AddText( font, fs, ImVec2( IM_ROUND( bmin.x + ( btn - isz.x ) * 0.5f ),
                IM_ROUND( bmin.y + btn * 0.5f - TextInkCenterY( font, fs, icons[ i ] ) ) ),
                cfg_mix( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), t ), icons[ i ] );
            if ( pressed ) {
                if ( i == 1 ) *open = false;
                else if ( dots_clicked ) *dots_clicked = true;
            }
        }
    }

    // content starts under the corner buttons, on the menu's usual content line
    ImGui::SetCursorScreenPos( ImVec2( min.x + gap, min.y + 46.f * s ) );
    m_row_started = false;
    return true;
}

void c_gui::overlay_end( ) {
    ImGui::PopStyleVar( ); // the content alpha - popped inside the window, or End() trips its stack check
    ImGui::End( );
    ImGui::PopStyleColor( );
    ImGui::PopStyleVar( 4 );
}

// One line of a framed list (the Ambience map list): a box with the entry's name, accent-bordered
// while selected, plus an optional trailing button - "..." dots, or a labelled one like "Add".
bool c_gui::framed_row( const char* label, bool selected, const char* action_icon, const char* action_label,
                        bool* action_clicked, float width ) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( window->SkipItems )
        return false;

    const float s = m_scale, h = 36.f * s;
    // Without a box around it the content region is the whole window, so a column hands its own
    // width in (see gui.grid_width()).
    const float w = width > 0.f ? width : ImGui::GetContentRegionAvail( ).x;
    const ImVec2 p = ImGui::GetCursorScreenPos( );
    ImDrawList* draw = window->DrawList;
    ImFont* font = ImGui::GetFont( );
    const float fs = ImGui::GetFontSize( );

    ImGui::PushID( label );

    // trailing button first: it owns its own clicks, the rest of the box selects the entry
    float action_w = 0.f;
    if ( action_icon || action_label ) {
        const char* shown = action_label ? tr( action_label ) : nullptr;
        const ImVec2 isz = action_icon ? font->CalcTextSizeA( fs, FLT_MAX, 0.f, action_icon ) : ImVec2( 0.f, 0.f );
        const ImVec2 lsz = shown ? font->CalcTextSizeA( fs, FLT_MAX, 0.f, shown ) : ImVec2( 0.f, 0.f );
        action_w = isz.x + lsz.x + ( action_icon && shown ? 6.f * s : 0.f ) + 16.f * s;
        const ImVec2 amin( p.x + w - 6.f * s - action_w, p.y + ( h - 24.f * s ) * 0.5f );
        ImGui::SetCursorScreenPos( amin );
        const ImGuiID aid = ImGui::GetID( "##action" );
        const bool pressed = ImGui::InvisibleButton( "##action", ImVec2( action_w, 24.f * s ) );
        const float t = cfg_anim( aid, ImGui::IsItemHovered( ) );
        if ( t > 0.01f )
            draw->AddRectFilled( amin, amin + ImVec2( action_w, 24.f * s ), frame_inactive.to_im_color( 0.8f * t ), 6.f * s );
        const ImU32 col = cfg_mix( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), t );
        float ax = amin.x + 8.f * s;
        if ( action_icon ) {
            draw->AddText( font, fs, ImVec2( IM_ROUND( ax ), IM_ROUND( amin.y + 12.f * s - TextInkCenterY( font, fs, action_icon ) ) ), col, action_icon );
            ax += isz.x + 6.f * s;
        }
        if ( shown )
            draw->AddText( font, fs, ImVec2( IM_ROUND( ax ), IM_ROUND( amin.y + 12.f * s - TextLineCenterY( font, fs ) ) ), col, shown );
        if ( pressed && action_clicked )
            *action_clicked = true;
        action_w += 6.f * s;
    }

    ImGui::SetCursorScreenPos( p );
    const ImGuiID id = ImGui::GetID( "##entry" );
    const bool clicked = ImGui::InvisibleButton( "##entry", ImVec2( w, h ) );
    const float hot = cfg_anim( id, ImGui::IsItemHovered( ) );
    const ImGuiID sel_key = ImHashStr( "##sel", 0, id );
    if ( cfg_anims( ).find( sel_key ) == cfg_anims( ).end( ) )
        cfg_anims( )[ sel_key ] = selected ? 1.f : 0.f;
    const float on = cfg_anim( sel_key, selected, 14.f );

    draw->AddRectFilled( p, p + ImVec2( w, h ), frame_inactive.to_im_color( 0.45f + 0.35f * hot ), 8.f * s );
    draw->AddRect( p, p + ImVec2( w, h ),
        cfg_mix( popup_border.to_vec4( 0.55f, false ), accent_color.to_vec4( 1.f, false ), on ), 8.f * s, 0, 1.f * s );

    const char* shown = tr( label );
    draw->PushClipRect( p, ImVec2( p.x + w - action_w - 6.f * s, p.y + h ), true );
    draw->AddText( font, fs, ImVec2( p.x + 12.f * s, IM_ROUND( p.y + h * 0.5f - TextLineCenterY( font, fs ) ) ),
        cfg_mix( text.to_vec4( 0.85f, false ), text.to_vec4( 1.f, false ), ImMax( hot, on ) ), shown );
    draw->PopClipRect( );

    ImGui::PopID( );
    // No extra offset here: the list's gap is the window's ItemSpacing.y, the same number the
    // columns around it are spaced by.
    ImGui::SetCursorScreenPos( ImVec2( p.x, p.y + h ) );
    ImGui::Dummy( ImVec2( w, 0.f ) );
    return clicked;
}

void c_gui::config_popup( const char* popup_name, bool clicked, std::vector<config_entry>& configs, int& selected, ImVec2 anchor ) {
    // Anchored under its button (rather than at the cursor) so the button stays clickable -
    // a card drawn over it swallows the click that was meant to close it again.
    const bool  anchored = anchor.x > -FLT_MAX;
    const ImVec2 click_pos = popup_click_pos( popup_name, clicked, anchor );
    bool wants_open = false;
    const float anim = popup_animation( popup_name, clicked, wants_open );
    if ( !begin_popup_card( popup_name, wants_open, anim, click_pos, 340.f, false, anchored ) )
        return;

    ImGuiIO& io = ImGui::GetIO( );
    ImDrawList* draw = ImGui::GetWindowDrawList( );
    ImFont* bold = io.Fonts->Fonts.Size > 4 ? io.Fonts->Fonts[ 4 ] : ImGui::GetFont( );

    const float  s = m_scale;
    const ImVec2 origin = ImGui::GetCursorScreenPos( );
    const float  w = ImGui::GetContentRegionAvail( ).x;

    const ImVec4 c_idle   = text_disabled.to_vec4( 1.f, false );
    const ImVec4 c_hot    = text.to_vec4( 1.f, false );
    const ImVec4 c_accent = accent_color.to_vec4( 1.f, false );
    const ImVec4 c_red    = ImVec4( 0.898f, 0.286f, 0.290f, 1.f );
    const ImU32  c_white  = ImGui::GetColorU32( ImVec4( 1.f, 1.f, 1.f, 1.f ) );

    bool bin_clicked = false, sort_clicked = false, menu_clicked = false;

    // ---- header: "Presets" badge, then cloud upload / recycle bin / new ----
    const float header_h = 30.f * s;
    {
        const ImVec2 icon_sz  = ImGui::CalcTextSize( ICON_FA_CLOUD );
        const ImVec2 label_sz = bold->CalcTextSizeA( bold->FontSize, FLT_MAX, 0.f, tr( "Presets" ) );
        const float  badge_w  = 12.f * s + icon_sz.x + 7.f * s + label_sz.x + 12.f * s;

        draw->AddRectFilled( origin, origin + ImVec2( badge_w, header_h ), accent_color.to_im_color( ), 8.f * s );
        draw->AddText( ImVec2( origin.x + 12.f * s, IM_ROUND( origin.y + ( header_h - icon_sz.y ) * 0.5f ) ), c_white, ICON_FA_CLOUD );
        draw->AddText( bold, bold->FontSize, ImVec2( origin.x + 12.f * s + icon_sz.x + 7.f * s,
                       IM_ROUND( origin.y + ( header_h - label_sz.y ) * 0.5f ) ), c_white, tr( "Presets" ) );

        const ImVec2 btn( 28.f * s, 28.f * s );
        const float  gap = 6.f * s;
        const float  top = origin.y + ( header_h - btn.y ) * 0.5f;
        float x = origin.x + w - btn.x;

        if ( cfg_icon_button( "##cfg_new", ICON_FA_PLUS, ImVec2( x, top ), btn, c_idle, c_hot ) ) {
            config_entry fresh;
            fresh.name = "Unnamed";
            fresh.created = fresh.modified = m_config_seq++;
            configs.push_back( fresh );
            selected = (int)configs.size( ) - 1;
            PushNotification( tr( "Config Created" ), tr( "Created new config" ) );
        }
        x -= btn.x + gap;
        bin_clicked = cfg_icon_button( "##cfg_bin", ICON_FA_TRASH_RESTORE, ImVec2( x, top ), btn, c_idle, c_hot );
        x -= btn.x + gap;
        if ( cfg_icon_button( "##cfg_upload", ICON_FA_CLOUD_UPLOAD_ALT, ImVec2( x, top ), btn, c_accent, c_hot ) ) {
            if ( selected >= 0 && selected < (int)configs.size( ) )
                PushNotification( tr( "Cloud" ), trf( "Uploaded %s to the cloud", configs[ selected ].name.c_str( ) ) );
        }
    }

    // ---- search field + sort ----
    const float search_y = origin.y + header_h + 14.f * s;
    const float search_h = 34.f * s;
    const float sort_w   = 34.f * s;
    const float field_w  = w - sort_w - 8.f * s;
    {
        const ImVec2 fp( origin.x, search_y );
        draw->AddRectFilled( fp, fp + ImVec2( field_w, search_h ), frame_inactive.to_im_color( 0.85f ), 8.f * s );
        draw->AddRect( fp, fp + ImVec2( field_w, search_h ), popup_border.to_im_color( 0.6f ), 8.f * s, 0, 1.f * s );

        // Measured: centring the em box leaves the glyphs ~2px low in the field, because the
        // descender space below the baseline is empty in "Search". Lift the whole row (icon,
        // placeholder and the live text alike) so the ink sits on the box's centre line.
        const float ink_nudge = 2.f * s;      // text: measured 2px low when the em box is centred
        const float icon_nudge = 1.f * s;     // the magnifier's ink sits lower inside its own em box
        const ImVec2 mag = ImGui::CalcTextSize( ICON_FA_SEARCH );
        draw->AddText( ImVec2( fp.x + 12.f * s, IM_ROUND( fp.y + ( search_h - mag.y ) * 0.5f - icon_nudge ) ), text_disabled.to_im_color( ), ICON_FA_SEARCH );

        // InputText right-aligns its box to the host window's width, so it gets a child of
        // exactly the text area - the box then lands where the cursor is and clips there too.
        const float text_x = fp.x + 12.f * s + mag.x + 10.f * s;
        const float text_w = ImMax( 1.f, fp.x + field_w - 12.f * s - text_x );
        const float text_y = IM_ROUND( fp.y + ( search_h - ImGui::GetFontSize( ) ) * 0.5f - ink_nudge );
        // NB: no ImGuiWindowFlags_NoBackground here - this fork repurposes that flag on plain
        // windows to paint the whole menu chrome (shadow + blur + panels), which lands inside
        // the field. A transparent ChildBg is what actually keeps the child invisible.
        ImGui::SetCursorScreenPos( ImVec2( text_x, text_y ) );
        ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) ); // "caller paints its own box"
        ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0.f, 0.f ) );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.f, 0.f ) );
        ImGui::BeginChild( "##cfg_search_box", ImVec2( text_w, ImGui::GetFontSize( ) ), false,
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
        // InputText() hands InputTextEx a fixed 130px box and that box is right-aligned to the
        // host window's width, which is what pushed the caret into the middle of the field.
        // Passing the real width as the size makes it fill the child from its left edge.
        ImGui::InputTextEx( "##cfg_search", NULL, m_config_search, IM_ARRAYSIZE( m_config_search ),
                            ImVec2( text_w, 0.f ), ImGuiInputTextFlags_None, NULL, NULL );
        const bool search_active = ImGui::IsItemActive( );
        ImGui::EndChild( );
        ImGui::PopStyleVar( 2 );
        ImGui::PopStyleColor( 2 );
        if ( m_config_search[ 0 ] == '\0' && !search_active )
            draw->AddText( ImVec2( text_x, text_y ), text_disabled.to_im_color( 0.75f ), tr( "Search" ) );

        const ImVec2 sp( origin.x + w - sort_w, search_y );
        sort_clicked = cfg_icon_button( "##cfg_sort", ICON_FA_SORT_AMOUNT_DOWN, sp, ImVec2( sort_w, search_h ),
                                        c_idle, c_hot, frame_inactive.to_im_color( 0.85f ), 8.f * s );
        draw->AddRect( sp, sp + ImVec2( sort_w, search_h ), popup_border.to_im_color( 0.6f ), 8.f * s, 0, 1.f * s );
    }

    // ---- visible order: search filter, then the active sort ----
    std::vector<int> order;
    order.reserve( configs.size( ) );
    const std::string needle = cfg_lower( m_config_search );
    for ( int i = 0; i < (int)configs.size( ); ++i )
        if ( needle.empty( ) || cfg_lower( configs[ i ].name ).find( needle ) != std::string::npos )
            order.push_back( i );

    const int sort_mode = m_config_sort;
    std::stable_sort( order.begin( ), order.end( ), [ & ]( int a, int b ) {
        if ( sort_mode == 1 ) return configs[ a ].modified > configs[ b ].modified;
        if ( sort_mode == 2 ) return cfg_lower( configs[ a ].name ) < cfg_lower( configs[ b ].name );
        return configs[ a ].created > configs[ b ].created;
    } );

    // ---- preset list ----
    const float row_h   = 36.f * s;
    const float row_gap = 8.f * s;
    const float list_y  = search_y + search_h + 12.f * s;
    const int   rows    = ImMax( 1, (int)order.size( ) );
    const float list_h  = ImMin( rows, 7 ) * ( row_h + row_gap ) - row_gap;

    int request_delete = -1, request_duplicate = -1;

    ImGui::SetCursorScreenPos( ImVec2( origin.x, list_y ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.f, 0.f ) );
    ImGui::BeginChild( "##cfg_list", ImVec2( w, list_h ), false, ImGuiWindowFlags_NoScrollbar );
    {
        ImDrawList* list_draw = ImGui::GetWindowDrawList( );
        const ImVec2 list_origin = ImGui::GetCursorScreenPos( );

        if ( order.empty( ) ) {
            const ImVec2 tsz = ImGui::CalcTextSize( tr( "No presets found" ) );
            list_draw->AddText( ImVec2( IM_ROUND( list_origin.x + ( w - tsz.x ) * 0.5f ),
                                        cfg_text_y( list_origin.y, row_h, tr( "No presets found" ) ) ), text_disabled.to_im_color( ), tr( "No presets found" ) );
        }

        for ( int n = 0; n < (int)order.size( ); ++n ) {
            const int i = order[ n ];
            config_entry& cfg = configs[ i ];
            const bool   is_selected = ( i == selected );
            const ImVec2 rp( list_origin.x, list_origin.y + n * ( row_h + row_gap ) );

            ImGui::PushID( i );
            const ImGuiID row_id = ImGui::GetID( "##row" );

            // Background first: the hover amount is read, not updated, so it can be painted
            // before the items that decide the hover state have been submitted.
            const float hov = cfg_anim_get( row_id );
            list_draw->AddRectFilled( rp, rp + ImVec2( w, row_h ), frame_inactive.to_im_color( 0.45f + 0.4f * hov ), 8.f * s );
            if ( is_selected )
                list_draw->AddRectFilled( rp, rp + ImVec2( w, row_h ), accent_color.to_im_color( 0.10f ), 8.f * s );
            list_draw->AddRect( rp, rp + ImVec2( w, row_h ),
                                is_selected ? accent_color.to_im_color( ) : popup_border.to_im_color( 0.5f ), 8.f * s, 0, 1.f * s );

            // trailing actions, right to left
            const ImVec2 act( 26.f * s, row_h );
            const float  act_x = rp.x + w - 10.f * s - act.x;
            const ImVec2 menu_pos( act_x - act.x - 2.f * s, rp.y );
            const float  actions_left = menu_pos.x;

            float name_x = rp.x + 12.f * s;
            if ( cfg.cloud ) {
                const ImVec2 gem = ImGui::CalcTextSize( ICON_FA_GEM );
                list_draw->AddText( ImVec2( name_x, IM_ROUND( rp.y + ( row_h - gem.y ) * 0.5f ) ), accent_color.to_im_color( ), ICON_FA_GEM );
                name_x += gem.x + 7.f * s;
            }

            if ( m_config_rename_row == i ) {
                const float rename_w = ImMax( 1.f, actions_left - 6.f * s - name_x );
                ImGui::SetCursorScreenPos( ImVec2( name_x, IM_ROUND( rp.y + ( row_h - ImGui::GetFontSize( ) ) * 0.5f ) ) );
                ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
                ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0.f, 0.f ) );
                ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.f, 0.f ) );
                ImGui::BeginChild( "##rename_box", ImVec2( rename_w, ImGui::GetFontSize( ) ), false,
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
                if ( m_config_rename_focus ) { // the field only exists from the frame after the double-click
                    ImGui::SetKeyboardFocusHere( );
                    m_config_rename_focus = false;
                }
                if ( ImGui::InputTextEx( "##rename", NULL, m_config_rename, IM_ARRAYSIZE( m_config_rename ),
                                         ImVec2( rename_w, 0.f ), ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL ) ) {
                    const std::string old_name = cfg.name;
                    cfg.name = m_config_rename;
                    cfg.modified = m_config_seq++;
                    m_config_rename_row = -1;
                    PushNotification( tr( "Config Renamed" ), trf( "Renamed %s to %s", old_name.c_str( ), cfg.name.c_str( ) ) );
                } else if ( ImGui::IsItemDeactivated( ) ) {
                    m_config_rename_row = -1;
                }
                ImGui::EndChild( );
                ImGui::PopStyleVar( 2 );
                ImGui::PopStyleColor( 2 );
            } else {
                list_draw->PushClipRect( rp, ImVec2( actions_left - 4.f * s, rp.y + row_h ), true );
                list_draw->AddText( ImVec2( name_x, cfg_text_y( rp.y, row_h, cfg.name.c_str( ) ) ),
                                    cfg_mix( is_selected ? c_hot : text.to_vec4( 0.85f, false ), c_hot, hov ), cfg.name.c_str( ) );
                list_draw->PopClipRect( );
            }

            // The action buttons are submitted before the row itself so a click on them is
            // never also read as "load this preset".
            if ( cfg_icon_button( "##dots", ICON_FA_ELLIPSIS_H, menu_pos, act, c_idle, c_hot ) ) {
                m_config_menu_row = i;
                menu_clicked = true;
            }
            if ( is_selected ) {
                if ( cfg_icon_button( "##save", ICON_FA_SAVE, ImVec2( act_x, rp.y ), act, c_idle, c_hot ) ) {
                    cfg.modified = m_config_seq++;
                    PushNotification( tr( "Config Saved" ), trf( "Saved changes to %s", cfg.name.c_str( ) ) );
                }
            } else if ( cfg_icon_button( "##download", ICON_FA_CLOUD_DOWNLOAD_ALT, ImVec2( act_x, rp.y ), act, c_idle, c_hot ) ) {
                PushNotification( tr( "Cloud" ), trf( "Downloaded %s", cfg.name.c_str( ) ) );
            }

            ImGui::SetCursorScreenPos( rp );
            const bool row_pressed = ImGui::InvisibleButton( "##row", ImVec2( w, row_h ) );
            const bool row_hovered = ImGui::IsItemHovered( );
            if ( row_hovered && ImGui::IsMouseDoubleClicked( 0 ) ) {
                m_config_rename_row = i;
                m_config_rename_focus = true;
                ImFormatString( m_config_rename, IM_ARRAYSIZE( m_config_rename ), "%s", cfg.name.c_str( ) );
            } else if ( row_pressed && !is_selected ) {
                selected = i;
                PushNotification( tr( "Config Loaded" ), trf( "Loaded config: %s", cfg.name.c_str( ) ) );
            }
            cfg_anim( row_id, row_hovered || is_selected );

            ImGui::PopID( );
        }

        ImGui::SetCursorScreenPos( ImVec2( list_origin.x, list_origin.y + rows * ( row_h + row_gap ) - row_gap ) );
        ImGui::Dummy( ImVec2( w, 0.f ) );
    }
    ImGui::EndChild( );
    ImGui::PopStyleVar( );

    ImGui::SetCursorScreenPos( ImVec2( origin.x, list_y + list_h ) );
    ImGui::Dummy( ImVec2( w, 0.f ) );

    // ---- "..." row menu: Share / Duplicate / Delete ----
    {
        const ImVec2 cp = popup_click_pos( "##CfgRowMenu", menu_clicked );
        bool open = false;
        const float a = popup_animation( "##CfgRowMenu", menu_clicked, open );
        if ( begin_popup_card( "##CfgRowMenu", open, a, cp + ImVec2( 8.f * s, 14.f * s ), 152.f, false, true ) ) {
            const float mw = ImGui::GetContentRegionAvail( ).x, mh = 34.f * s;
            if ( cfg_menu_row( "##share", ICON_FA_SHARE, "Share", mw, mh, c_idle, c_hot ) ) {
                if ( m_config_menu_row >= 0 && m_config_menu_row < (int)configs.size( ) )
                    PushNotification( tr( "Share" ), trf( "Copied a share link for %s", configs[ m_config_menu_row ].name.c_str( ) ) );
                ImGui::CloseCurrentPopup( );
            }
            if ( cfg_menu_row( "##duplicate", ICON_FA_COPY, "Duplicate", mw, mh, c_idle, c_hot ) ) {
                request_duplicate = m_config_menu_row;
                ImGui::CloseCurrentPopup( );
            }
            if ( cfg_menu_row( "##delete", ICON_FA_TRASH, "Delete", mw, mh, c_red, c_red ) ) {
                request_delete = m_config_menu_row;
                ImGui::CloseCurrentPopup( );
            }
            ImGui::Dummy( ImVec2( mw, 0.f ) );
            end_popup_card( );
        }
    }

    // ---- sort menu ----
    {
        const ImVec2 cp = popup_click_pos( "##CfgSort", sort_clicked );
        bool open = false;
        const float a = popup_animation( "##CfgSort", sort_clicked, open );
        const char* modes[] = { "Newest First", "Recently Modified", "Alphabetical" };
        const float sort_w_units = cfg_choice_width( modes, IM_ARRAYSIZE( modes ) );
        // right-aligned on its button, whatever the longest option in this language needs
        if ( begin_popup_card( "##CfgSort", open, a, cp + ImVec2( -( sort_w_units - 28.f ) * s, 24.f * s ), sort_w_units, false, true ) ) {
            const float mw = ImGui::GetContentRegionAvail( ).x, mh = 34.f * s;
            for ( int i = 0; i < 3; ++i ) {
                ImGui::PushID( i );
                cfg_choice_row("##mode", modes[i], mw, mh, &m_config_sort, i); // stays up after a pick
                ImGui::PopID( );
            }
            ImGui::Dummy( ImVec2( mw, 0.f ) );
            end_popup_card( );
        }
    }

    // ---- recycle bin: restore or purge deleted presets ----
    {
        const ImVec2 cp = popup_click_pos( "##CfgBin", bin_clicked );
        bool open = false;
        const float a = popup_animation( "##CfgBin", bin_clicked, open );
        if ( begin_popup_card( "##CfgBin", open, a, cp + ImVec2( -110.f * s, 24.f * s ), 216.f, false, true ) ) {
            ImDrawList* bin_draw = ImGui::GetWindowDrawList( );
            const float mw = ImGui::GetContentRegionAvail( ).x, mh = 34.f * s;

            if ( m_config_trash.empty( ) ) {
                const ImVec2 pos = ImGui::GetCursorScreenPos( );
                const ImVec2 tsz = ImGui::CalcTextSize( tr( "Bin is empty" ) );
                bin_draw->AddText( ImVec2( IM_ROUND( pos.x + ( mw - tsz.x ) * 0.5f ), IM_ROUND( pos.y + ( mh - tsz.y ) * 0.5f ) ),
                                   text_disabled.to_im_color( ), tr( "Bin is empty" ) );
                ImGui::SetCursorScreenPos( ImVec2( pos.x, pos.y + mh ) );
            }

            for ( int i = 0; i < (int)m_config_trash.size( ); ++i ) {
                const ImVec2 pos = ImGui::GetCursorScreenPos( );
                ImGui::PushID( i );

                bin_draw->PushClipRect( pos, ImVec2( pos.x + mw - 64.f * s, pos.y + mh ), true );
                const ImVec2 tsz = ImGui::CalcTextSize( m_config_trash[ i ].name.c_str( ) );
                bin_draw->AddText( ImVec2( pos.x + 2.f * s, IM_ROUND( pos.y + ( mh - tsz.y ) * 0.5f ) ), text.to_im_color( ), m_config_trash[ i ].name.c_str( ) );
                bin_draw->PopClipRect( );

                const ImVec2 btn( 26.f * s, mh );
                if ( cfg_icon_button( "##purge", ICON_FA_TRASH, ImVec2( pos.x + mw - btn.x * 2.f - 6.f * s, pos.y ), btn, c_red, c_red ) ) {
                    m_config_trash.erase( m_config_trash.begin( ) + i );
                    ImGui::PopID( );
                    break;
                }
                if ( cfg_icon_button( "##restore", ICON_FA_UNDO, ImVec2( pos.x + mw - btn.x, pos.y ), btn, c_idle, c_hot ) ) {
                    config_entry restored = m_config_trash[ i ];
                    restored.modified = m_config_seq++;
                    configs.push_back( restored );
                    m_config_trash.erase( m_config_trash.begin( ) + i );
                    PushNotification( tr( "Config Restored" ), trf( "Restored %s", restored.name.c_str( ) ) );
                    ImGui::PopID( );
                    break;
                }

                ImGui::SetCursorScreenPos( ImVec2( pos.x, pos.y + mh ) );
                ImGui::PopID( );
            }

            ImGui::Dummy( ImVec2( mw, 0.f ) );
            end_popup_card( );
        }
    }

    // Deferred so the list is never resized while it is being walked.
    if ( request_duplicate >= 0 && request_duplicate < (int)configs.size( ) ) {
        config_entry copy = configs[ request_duplicate ];
        copy.cloud = false;
        copy.created = copy.modified = m_config_seq++;
        configs.insert( configs.begin( ) + request_duplicate + 1, copy );
        if ( selected > request_duplicate ) selected++;
        PushNotification( tr( "Config Duplicated" ), trf( "Duplicated %s", copy.name.c_str( ) ) );
    }
    if ( request_delete >= 0 && request_delete < (int)configs.size( ) ) {
        m_config_trash.push_back( configs[ request_delete ] );
        PushNotification( tr( "Config Deleted" ), trf( "Removed config: %s", configs[ request_delete ].name.c_str( ) ) );
        configs.erase( configs.begin( ) + request_delete );
        if ( selected >= request_delete ) selected = ImMax( 0, selected - 1 );
        if ( m_config_rename_row == request_delete ) m_config_rename_row = -1;
        m_config_menu_row = -1;
    }
    if ( selected >= (int)configs.size( ) ) selected = (int)configs.size( ) - 1;

    end_popup_card( );
}

void c_gui::popup_section_begin( const char* id, bool border, float pad ) {
    m_popup_section_pos = ImGui::GetCursorScreenPos();
    m_popup_section_border = border;
    m_popup_section_pad = border ? pad * m_scale : 0.f;
    const float horizontal_pad = 14.f * m_scale;

    float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth( ImMax(1.f, w - horizontal_pad * 2.f) );
    ImGui::SetCursorScreenPos( m_popup_section_pos + ImVec2( horizontal_pad, m_popup_section_pad ) );
    ImGui::PushID( id );
    ImGui::BeginGroup();
}

void c_gui::popup_section_end() {
    ImGui::EndGroup();
    ImVec2 content_size = ImGui::GetItemRectSize();
    ImGui::PopID();
    ImGui::PopItemWidth();

    float w = ImGui::GetContentRegionAvail().x;
    float total_h = content_size.y + m_popup_section_pad * 2.f;

    if ( m_popup_section_border )
        ImGui::GetWindowDrawList()->AddRect( m_popup_section_pos, m_popup_section_pos + ImVec2( w, total_h ), gui.popup_border.to_im_color(), 12.f * m_scale, 0, 1.f * m_scale );

    // advance past this section, plus a gap, ready for the next section or widget
    ImGui::SetCursorScreenPos( m_popup_section_pos + ImVec2( 0, total_h + 8.f * m_scale ) );
}

// ---------------------------------------------------------------------------------------
//  Theme
// ---------------------------------------------------------------------------------------

// Menu chrome colors for imgui.cpp's window-background code (it doesn't see gui.hpp).
ImU32 menu_panel_color( int panel ) {
    const color_t& c = panel == 0 ? gui.panel_side : panel == 1 ? gui.panel_top : gui.panel_body;
    return ImGui::ColorConvertFloat4ToU32( ImVec4( c.r, c.g, c.b, c.a ) );
}

void c_gui::apply_theme( int new_theme ) {
    theme = new_theme;
    if ( theme == 0 ) { // Dark - the menu's original values
        text           = { 0.922f, 0.922f, 0.941f, 1.f };
        text_disabled  = { 0.51f, 0.52f, 0.56f, 1.f };
        text_soft      = { 0.76f, 0.77f, 0.81f, 1.f };
        border         = { 0.118f, 0.129f, 0.173f, 1.f };
        popup_border   = { 0.165f, 0.180f, 0.235f, 1.f };
        frame_inactive = { 0.098f, 0.110f, 0.149f, 1.f };
        frame_active   = { 0.043f, 0.07f, 0.137f, 1.f };
        tab_active     = { 0.153f, 0.169f, 0.212f, 1.f };
        button         = { 0.102f, 0.102f, 0.133f, 1.f };
        button_hovered = { 0.050f, 0.054f, 0.078f, 1.f };
        button_active  = { 0.07f, 0.074f, 0.098f, 1.f };
        group_box_bg   = { 0.071f, 0.078f, 0.114f, 1.f };
        panel_side     = { 0.071f, 0.078f, 0.114f, 0.80f };
        panel_top      = { 0.043f, 0.059f, 0.090f, 0.90f };
        panel_body     = { 0.043f, 0.055f, 0.086f, 0.90f };
        popup_bg       = { 0.071f, 0.078f, 0.114f, 0.70f };
        field_bg       = { 25.f / 255.f, 27.f / 255.f, 36.f / 255.f, 1.f };
        switch_off     = { 0.055f, 0.071f, 0.098f, 1.f };
        switch_rim     = { 1.f, 1.f, 1.f, 0.07f };
        knob_off       = { 0.498f, 0.529f, 0.557f, 1.f };
        divider        = { 1.f, 1.f, 1.f, 30.f / 255.f };
        label_off      = { 150.f / 255.f, 150.f / 255.f, 150.f / 255.f, 1.f };
        label_on       = { 1.f, 1.f, 1.f, 1.f };
    } else {            // Light - frosted panels, ink text, same accent
        text           = { 0.110f, 0.118f, 0.145f, 1.f };
        text_disabled  = { 0.463f, 0.482f, 0.529f, 1.f };
        text_soft      = { 0.310f, 0.325f, 0.369f, 1.f };
        border         = { 0.855f, 0.867f, 0.894f, 1.f };
        popup_border   = { 0.800f, 0.816f, 0.855f, 1.f };
        frame_inactive = { 0.890f, 0.902f, 0.929f, 1.f };
        frame_active   = { 0.820f, 0.859f, 0.957f, 1.f };
        tab_active     = { 0.851f, 0.871f, 0.918f, 1.f };
        button         = { 0.906f, 0.914f, 0.937f, 1.f };
        button_hovered = { 0.867f, 0.878f, 0.906f, 1.f };
        button_active  = { 0.831f, 0.843f, 0.875f, 1.f };
        // translucent like the dark panels (0.80 / 0.90) so the blur behind reads through the frost
        group_box_bg   = { 0.988f, 0.988f, 0.992f, 0.78f };
        panel_side     = { 0.918f, 0.925f, 0.945f, 0.62f };
        panel_top      = { 0.957f, 0.961f, 0.973f, 0.70f };
        panel_body     = { 0.945f, 0.949f, 0.965f, 0.70f };
        popup_bg       = { 0.965f, 0.969f, 0.980f, 0.66f };
        field_bg       = { 0.898f, 0.906f, 0.933f, 0.85f };
        switch_off     = { 0.792f, 0.808f, 0.847f, 1.f };
        switch_rim     = { 0.f, 0.f, 0.f, 0.06f };
        knob_off       = { 1.f, 1.f, 1.f, 1.f };
        divider        = { 0.f, 0.f, 0.f, 0.10f };
        label_off      = { 0.463f, 0.482f, 0.529f, 1.f };
        label_on       = { 0.110f, 0.118f, 0.145f, 1.f };
    }
}

void c_gui::apply_style_colors( ) {
    if ( theme == 0 )
        return; // the app's default style already is the dark look - leave it byte for byte
    ImVec4* c = ImGui::GetStyle( ).Colors;
    c[ ImGuiCol_Text ]           = text.to_vec4( 1.f, false );
    c[ ImGuiCol_TextDisabled ]   = text_disabled.to_vec4( 1.f, false );
    c[ ImGuiCol_Separator ]      = border.to_vec4( 1.f, false );
    c[ ImGuiCol_Border ]         = border.to_vec4( 1.f, false );
    c[ ImGuiCol_TextSelectedBg ] = accent_color.to_vec4( 0.30f, false );
}

// ---------------------------------------------------------------------------------------
//  Language
// ---------------------------------------------------------------------------------------

namespace {
    struct tr_entry { const char* en; const char* ru; const char* tr; };

    // Keyed by the English text the menu passes around. Strings with %s are used through trf().
    const tr_entry k_translations[] = {
        { "Search functions...", "Поиск функций...", "İşlev ara..." },
        { "No matching functions", "Ничего не найдено", "Eşleşen işlev yok" },
        { "Functions", "Функции", "İşlevler" },
        // Visuals -> Players
        { "ENEMY",             "ВРАГ",                "DÜŞMAN" },
        { "ENEMY MODEL",       "МОДЕЛЬ ВРАГА",        "DÜŞMAN MODELİ" },
        { "Offscreen Arrow",   "Стрелка за экраном",  "Ekran Dışı Ok" },
        { "Sounds",            "Звуки",               "Sesler" },
        { "Player",            "Игрок",               "Oyuncu" },
        { "Behind Walls",      "Сквозь стены",        "Duvar Arkası" },
        { "On Shot",           "При выстреле",        "Ateş Anında" },
        { "History",           "История",             "Geçmiş" },
        { "Ragdolls",          "Рэгдоллы",            "Ragdoll" },
        { "Flat",              "Плоский",             "Düz" },
        { "Chams",             "Chams",               "Chams" },
        { "Enemies",           "Враги",               "Düşmanlar" },
        { "Friends",           "Союзники",            "Dostlar" },
        { "ESP Items",         "Элементы ESP",        "ESP Öğeleri" },
        { "Main",              "Основное",            "Ana" },
        { "Flags",             "Флаги",               "Bayraklar" },
        { "Weapon",            "Оружие",              "Silah" },
        { "Aimbot",            "Аимбот",              "Aimbot" },
        { "Bounding Box",      "Рамка",               "Çerçeve" },
        { "Distance",          "Дистанция",           "Mesafe" },
        { "Health Bar",        "Полоса здоровья",     "Can Barı" },
        { "Ammo Bar",          "Полоса патронов",     "Mermi Barı" },
        { "Skeleton",          "Скелет",              "İskelet" },
        { "Unarmored",         "Без брони",           "Zırhsız" },
        { "Defuser",           "Дефузер",             "Defuser" },
        { "Blind",             "Ослеплён",            "Kör" },
        { "Scoped",            "В прицеле",           "Dürbünde" },
        { "Reload",            "Перезарядка",         "Şarjör" },
        { "Immunity",          "Иммунитет",           "Dokunulmazlık" },
        { "Slowed",            "Замедлен",            "Yavaşlamış" },
        { "Vulnerable",        "Уязвим",              "Savunmasız" },
        { "Hostage",           "Заложник",            "Rehine" },
        { "Defuse",            "Разминирование",      "Bomba İmha" },
        { "Pin Pulled",        "Чека выдернута",      "Pimi Çekili" },
        { "Money",             "Деньги",              "Para" },
        { "Order Priority",    "Приоритет",           "Öncelik" },
        { "Delay",             "Задержка",            "Gecikme" },
        { "Icon",              "Иконка",              "İkon" },
        { "Readiness Bar",     "Готовность",          "Hazırlık Barı" },
        { "Taser",             "Тазер",               "Tazer" },
        { "Hit Chance",        "Шанс попадания",      "İsabet Şansı" },
        { "Hitboxes",          "Хитбоксы",            "Hitbox" },
        // Visuals -> World
        { "VIEW",              "ОБЗОР",               "GÖRÜŞ" },
        { "HUD",               "ИНТЕРФЕЙС",           "HUD" },
        { "WORLD ESP",         "ESP МИРА",            "DÜNYA ESP" },
        { "MISCELLANEOUS",     "ПРОЧЕЕ",              "ÇEŞİTLİ" },
        { "View Options",      "Настройки обзора",    "Görüş Ayarları" },
        { "Scope Options",     "Настройки прицела",   "Dürbün Ayarları" },
        { "Viewmodel Options", "Настройки вьюмодели", "Viewmodel Ayarları" },
        { "Perspective Options", "Настройки перспективы", "Perspektif Ayarları" },
        { "Unlock Spectating", "Свободный обзор",     "Serbest İzleme" },
        { "Visual Recoil",     "Визуальная отдача",   "Görsel Geri Tepme" },
        { "Radar",             "Радар",               "Radar" },
        { "Scope Overlay",     "Оверлей прицела",     "Dürbün Kaplaması" },
        { "Inaccuracy Overlay", "Оверлей разброса",   "Sapma Göstergesi" },
        { "Death Notices",     "Сообщения о смертях", "Ölüm Bildirimleri" },
        { "Scoreboard",        "Таблица счёта",       "Skor Tablosu" },
        { "Crosshairs",        "Прицелы",             "Nişangahlar" },
        { "Bomb",              "Бомба",               "Bomba" },
        { "Weapons",           "Оружие",              "Silahlar" },
        { "Hostages",          "Заложники",           "Rehineler" },
        { "Grenades",          "Гранаты",             "El Bombaları" },
        { "Grenade Trajectory", "Траектория гранат",  "Bomba Yörüngesi" },
        { "Grenade Proximity Warnings", "Предупреждения о гранатах", "Bomba Yakınlık Uyarısı" },
        { "Windows",           "Окна",                "Pencereler" },
        { "Removals",          "Удаления",            "Kaldırmalar" },
        { "Ambience",          "Атмосфера",           "Atmosfer" },
        { "Hit Marker",        "Хитмаркер",           "İsabet İşareti" },
        { "Bullet Tracers",    "Трассеры",            "Mermi İzleri" },
        { "Bullet Impacts",    "Попадания",           "Mermi Çarpmaları" },
        // Ambience
        { "MAP SELECTION",     "ВЫБОР КАРТЫ",         "HARİTA SEÇİMİ" },
        { "EFFECTS",           "ЭФФЕКТЫ",             "EFEKTLER" },
        { "Global",            "Глобально",           "Genel" },
        { "Main Menu",         "Главное меню",        "Ana Menü" },
        { "Add",               "Добавить",            "Ekle" },
        { "Nightmode",         "Ночной режим",        "Gece Modu" },
        { "Fullbright",        "Полная яркость",      "Tam Aydınlık" },
        { "Exposure",          "Экспозиция",          "Pozlama" },
        { "Sunlight",          "Солнечный свет",      "Güneş Işığı" },
        { "Sky",               "Небо",                "Gökyüzü" },
        { "Fog",               "Туман",               "Sis" },
        { "Bloom",             "Свечение",            "Parlama" },
        { "Vignette",          "Виньетка",            "Vinyet" },
        { "Local Contrast",    "Локальный контраст",  "Yerel Kontrast" },
        { "Color Correction",  "Цветокоррекция",      "Renk Düzeltme" },
        { "Depth of Field",    "Глубина резкости",    "Alan Derinliği" },
        { "Smoke",             "Дым",                 "Duman" },
        { "Flashbang",         "Флешка",              "Flash" },
        { "Scope Blur",        "Размытие прицела",    "Dürbün Bulanıklığı" },
        { "Added the current map", "Карта добавлена", "Geçerli harita eklendi" },
        // shared row bits
        { "Show On Death",     "Показывать после смерти", "Ölümde Göster" },
        { "Blur On Shader", "Размытие шейдера", "Shader Bulanıklığı" },
        { "Blur Intensity", "Сила размытия", "Bulanıklık Yoğunluğu" },
        { "Blur the selected effect behind the menu.", "Размывает выбранный эффект за меню.", "Menünün arkasındaki seçili efekti bulanıklaştırır." },
        { "Opacity",           "Прозрачность",        "Saydamlık" },
        { "Mode",              "Режим",               "Mod" },
        { "Amount",            "Сила",                "Miktar" },
        { "Line",              "Линия",               "Çizgi" },
        { "Dots",              "Точки",               "Noktalar" },
        { "Arrows",            "Стрелки",             "Oklar" },
        { "Affect Foliage",    "Влиять на листву",    "Bitkileri Etkile" },
        { "Players Only",      "Только игроки",       "Sadece Oyuncular" },
        { "Size",              "Размер",              "Boyut" },
        { "Outline",           "Обводка",             "Dış Çizgi" },
        { "Filled",            "Заливка",             "Dolu" },
        { "Outlined",          "Обведённый",          "Çizgili" },
        { "Soft",              "Мягкий",              "Yumuşak" },
        { "Strong",            "Сильный",             "Güçlü" },
        { "Always",            "Всегда",              "Her Zaman" },
        { "In Air",            "В воздухе",           "Havadayken" },
        { "On Key",            "По клавише",          "Tuşla" },
        { "Classic",           "Классический",        "Klasik" },
        { "Minimal",           "Минимальный",         "Sade" },
        // navigation
        { "AIMBOT",            "ПРИЦЕЛ",              "NİŞAN" },
        { "COMMON",            "ОБЩЕЕ",               "GENEL" },
        { "Rage",              "Рейдж",               "Rage" },
        { "Legit",             "Легит",               "Legit" },
        { "Visuals",           "Визуалы",             "Görseller" },
        { "Players",           "Игроки",              "Oyuncular" },
        { "World",             "Мир",                 "Dünya" },
        { "Miscellaneous",     "Разное",              "Çeşitli" },
        { "MAIN",              "ОСНОВНОЕ",            "ANA" },
        { "SELECTION",         "ВЫБОР",               "SEÇİM" },
        { "OTHER",             "ДРУГОЕ",              "DİĞER" },
        { "ANTI-AIM",          "АНТИ-АИМ",            "ANTI-AIM" },
        // rows
        { "Enabled",           "Включено",            "Etkin" },
        { "Silent Aim",        "Тихий аим",           "Sessiz Nişan" },
        { "Automatic Fire",    "Автострельба",        "Otomatik Ateş" },
        { "Aim Through Walls", "Сквозь стены",        "Duvar Arkasına Nişan" },
        { "Field Of View",     "Поле зрения",         "Görüş Alanı" },
        { "Pitch",             "Питч",                "Pitch" },
        { "Yaw",               "Яв",                  "Yaw" },
        { "Freestanding",      "Фристендинг",         "Freestanding" },
        { "Mouse Override",    "Управление мышью",    "Fare Kontrolü" },
        { "Checkbox",          "Флажок",              "Onay Kutusu" },
        { "Combo",             "Список",              "Liste" },
        { "Multi",             "Мульти",              "Çoklu" },
        { "Slider",            "Ползунок",            "Kaydırıcı" },
        { "Kill Effect",       "Эффект убийства",     "Öldürme Efekti" },
        { "Additional Colors", "Доп. цвета",          "Ek Renkler" },
        { "Glow",              "Свечение",            "Parlama" },
        { "Name",              "Имя",                 "İsim" },
        { "Weather",           "Погода",              "Hava Durumu" },
        { "Wind",              "Ветер",               "Rüzgar" },
        { "Model Wetness",     "Мокрые модели",       "Model Islaklığı" },
        { "Thunder",           "Гром",                "Gök Gürültüsü" },
        { "Rain",              "Дождь",               "Yağmur" },
        { "Snow",              "Снег",                "Kar" },
        { "Layout",            "Макет",               "Düzen" },
        { "Background",        "Фон",                 "Arka Plan" },
        { "Border",            "Рамка",               "Kenarlık" },
        { "Accent",            "Акцент",              "Vurgu" },
        { "Text",              "Текст",               "Metin" },
        { "Spread Force",      "Сила разброса",       "Yayılma Gücü" },
        { "Speed",             "Скорость",            "Hız" },
        { "Font",              "Шрифт",               "Yazı Tipi" },
        { "Case",              "Регистр",             "Harf Düzeni" },
        { "Draw Avatar",       "Показывать аватар",   "Avatarı Göster" },
        { "Regular",           "Обычный",             "Normal" },
        { "Bold",              "Жирный",              "Kalın" },
        { "Italic",            "Курсив",              "İtalik" },
        { "Default",           "По умолчанию",        "Varsayılan" },
        { "Uppercase",         "Прописные",           "Büyük Harf" },
        { "Lowercase",         "Строчные",            "Küçük Harf" },
        { "Select",            "Выбрать",             "Seç" },
        { "Head",              "Голова",              "Kafa" },
        { "Chest",             "Грудь",               "Göğüs" },
        { "Stomach",           "Живот",               "Karın" },
        { "Legs",              "Ноги",                "Bacaklar" },
        // tooltips
        { "Click to enable or disable this option.", "Нажмите, чтобы включить или выключить.", "Açmak veya kapatmak için tıklayın." },
        { "Open this section to adjust its settings.", "Откройте раздел, чтобы настроить его.", "Ayarlarını değiştirmek için bu bölümü açın." },
        { "Adjust the field of view in degrees.", "Настройка поля зрения в градусах.", "Görüş alanını derece cinsinden ayarlayın." },
        { "Select one item from the list.", "Выберите один пункт из списка.", "Listeden bir öğe seçin." },
        { "Select one or more items. Click a selected item again to remove it.",
          "Выберите один или несколько пунктов. Нажмите на выбранный ещё раз, чтобы убрать его.",
          "Bir veya daha fazla öğe seçin. Kaldırmak için seçili öğeye tekrar tıklayın." },
        { "Drag the bar to adjust the value from 0 to 100.", "Перетащите ползунок, чтобы задать значение от 0 до 100.", "Değeri 0 ile 100 arasında ayarlamak için çubuğu sürükleyin." },
        { "Click the swatch to edit the color.", "Нажмите на образец, чтобы изменить цвет.", "Rengi düzenlemek için renk kutusuna tıklayın." },
        { "Click the swatch to edit the colors.", "Нажмите на образец, чтобы изменить цвета.", "Renkleri düzenlemek için renk kutusuna tıklayın." },
        // account card
        { "Language",          "Язык",                "Dil" },
        { "Menu Scale",        "Масштаб меню",        "Menü Ölçeği" },
        { "ESP Scale",         "Масштаб ESP",         "ESP Ölçeği" },
        { "Windows Scale",     "Масштаб окон",        "Pencere Ölçeği" },
        { "Units",             "Единицы",             "Birimler" },
        { "Style",             "Стиль",               "Stil" },
        { "Safe Mode",         "Безопасный режим",    "Güvenli Mod" },
        { "Synchronization",   "Синхронизация",       "Senkronizasyon" },
        { "About",             "О программе",         "Hakkında" },
        { "Chat",              "Чат",                 "Sohbet" },
        { "Renew",             "Продлить",            "Yenile" },
        { "Lifetime",          "Навсегда",            "Ömür Boyu" },
        { "Till: Lifetime",    "До: навсегда",        "Bitiş: Ömür Boyu" },
        { "English",           "English",             "English" },   // every language shows its own name
        { "Russian",           "Русский",             "Русский" },
        { "Turkish",           "Türkçe",              "Türkçe" },
        { "Dark",              "Тёмная",              "Koyu" },
        { "Light",             "Светлая",             "Açık" },
        { "Auto",              "Авто",                "Otomatik" },
        { "Metric",            "Метрические",         "Metrik" },
        { "Imperial",          "Имперские",           "Emperyal" },
        { "Disabled",          "Выключено",           "Kapalı" },
        // config browser
        { "Presets",           "Пресеты",             "Ön Ayarlar" },
        { "Search",            "Поиск",               "Ara" },
        { "Newest First",      "Сначала новые",       "Önce En Yeni" },
        { "Recently Modified", "Недавно изменённые",  "Son Değiştirilen" },
        { "Alphabetical",      "По алфавиту",         "Alfabetik" },
        { "Share",             "Поделиться",          "Paylaş" },
        { "Duplicate",         "Дублировать",         "Çoğalt" },
        { "Delete",            "Удалить",             "Sil" },
        { "Bin is empty",      "Корзина пуста",       "Çöp kutusu boş" },
        { "No presets found",  "Пресеты не найдены",  "Ön ayar bulunamadı" },
        // notifications
        { "Cloud",             "Облако",              "Bulut" },
        { "Config Created",    "Конфиг создан",       "Yapılandırma Oluşturuldu" },
        { "Config Saved",      "Конфиг сохранён",     "Yapılandırma Kaydedildi" },
        { "Config Loaded",     "Конфиг загружен",     "Yapılandırma Yüklendi" },
        { "Config Deleted",    "Конфиг удалён",       "Yapılandırma Silindi" },
        { "Config Renamed",    "Конфиг переименован", "Yapılandırma Yeniden Adlandırıldı" },
        { "Config Restored",   "Конфиг восстановлен", "Yapılandırma Geri Yüklendi" },
        { "Config Duplicated", "Конфиг продублирован", "Yapılandırma Çoğaltıldı" },
        { "Created new config", "Создан новый конфиг", "Yeni yapılandırma oluşturuldu" },
        { "Saved changes to %s",         "Изменения сохранены: %s",     "%s kaydedildi" },
        { "Loaded config: %s",           "Загружен конфиг: %s",         "Yüklendi: %s" },
        { "Removed config: %s",          "Удалён конфиг: %s",           "Silindi: %s" },
        { "Uploaded %s to the cloud",    "%s загружен в облако",        "%s buluta yüklendi" },
        { "Downloaded %s",               "Скачан %s",                   "%s indirildi" },
        { "Copied a share link for %s",  "Ссылка на %s скопирована",    "%s için paylaşım bağlantısı kopyalandı" },
        { "Duplicated %s",               "Продублирован %s",            "%s çoğaltıldı" },
        { "Restored %s",                 "Восстановлен %s",             "%s geri yüklendi" },
        { "Renamed %s to %s",            "%s переименован в %s",        "%s, %s olarak yeniden adlandırıldı" },
    };
}

const char* c_gui::tr( const char* text ) const {
    if ( !text || language <= 0 || language > 2 )
        return text;
    static std::unordered_map<ImGuiID, int> index;
    if ( index.empty( ) )
        for ( int i = 0; i < IM_ARRAYSIZE( k_translations ); ++i )
            index[ ImHashData( k_translations[ i ].en, strlen( k_translations[ i ].en ) ) ] = i;
    // "Label##id" is looked up by its visible part; the translation comes back without the id
    const char* end = ImGui::FindRenderedTextEnd( text );
    if ( end == text )
        return text;
    const auto it = index.find( ImHashData( text, ( size_t )( end - text ) ) );
    if ( it == index.end( ) )
        return text;
    const tr_entry& e = k_translations[ it->second ];
    return language == 1 ? e.ru : e.tr;
}

std::string c_gui::trf( const char* fmt, const char* a, const char* b ) const {
    char buf[ 512 ];
    ImFormatString( buf, IM_ARRAYSIZE( buf ), tr( fmt ), a ? a : "", b ? b : "" );
    return buf;
}

// ---------------------------------------------------------------------------------------
//  Color pickers
// ---------------------------------------------------------------------------------------

// Fills a rounded rect with a linear gradient and keeps its anti-aliased edge: the rect is
// drawn opaque white, then every vertex it produced is recoloured. The alpha AddRectFilled wrote
// into each vertex is the AA coverage (0 on the outer fringe), so it is multiplied back in.
// (The fork's AddRectFilledMultiColorRounded fakes its corners by painting over them with a
// background color, which shows up as patches on the translucent, blurred cards.)
static void fill_gradient( ImDrawList* draw, ImVec2 min, ImVec2 max, const ImVec4& c0, const ImVec4& c1,
                           bool vertical, float rounding, ImDrawFlags flags = 0 ) {
    if ( max.x <= min.x || max.y <= min.y )
        return;
    const int first = draw->VtxBuffer.Size;
    draw->AddRectFilled( min, max, IM_COL32_WHITE, rounding, flags );
    const float alpha = GImGui->Style.Alpha;
    const float span = vertical ? ( max.y - min.y ) : ( max.x - min.x );
    for ( int i = first; i < draw->VtxBuffer.Size; ++i ) {
        ImDrawVert& v = draw->VtxBuffer[ i ];
        const float t = ImSaturate( ( vertical ? v.pos.y - min.y : v.pos.x - min.x ) / span );
        ImVec4 c = ImLerp( c0, c1, t );
        c.w *= alpha * ( float )( ( v.col >> IM_COL32_A_SHIFT ) & 0xFF ) / 255.f;
        v.col = ImGui::ColorConvertFloat4ToU32( c );
    }
}

// Stacked AA fills leave the lower layer's color in their shared fringe - on the color field
// that showed as a light halo around the dark bottom corners. So layered shapes are filled
// with anti-aliasing off (the layers then end exactly on the edge) and get one AA edge ring,
// colored with the shape's final color at each point.
template < typename Body >
static void with_aa_fill_off( ImDrawList* draw, Body body ) {
    const ImDrawListFlags saved = draw->Flags;
    draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
    body( );
    draw->Flags = saved;
}

// ImGui tessellates a rounded corner from its circle table, and at the default max error that is
// three segments per quadrant. An AA fill hides the facets; these shapes are filled with AA off
// (see above), so the facets are the edge - a visible staircase as soon as the menu is scaled up
// and the corner radius grows with it. Raised for the length of one shape, fill and rim together,
// so the two keep sharing the exact same outline.
struct hi_res_arcs {
    ImDrawListSharedData* data;
    float saved;
    hi_res_arcs( ImDrawList* draw ) : data( draw->_Data ), saved( draw->_Data->CircleSegmentMaxError ) {
        data->SetCircleTessellationMaxError( 0.06f );
    }
    ~hi_res_arcs( ) { data->SetCircleTessellationMaxError( saved ); }
};

// One-pixel anti-aliased rim around a rounded rect (the same geometry ImGui's AA fill uses),
// opaque `color_at(point)` on the edge fading to clear just outside it.
template < typename ColorAt >
static void aa_edge_ring( ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, ColorAt color_at ) {
    draw->PathRect( min, max, rounding );
    ImVector<ImVec2> pts = draw->_Path;
    draw->PathClear( );
    const int n = pts.Size;
    if ( n < 3 )
        return;

    ImVector<ImVec2> normals;
    normals.resize( n );
    for ( int i0 = n - 1, i1 = 0; i1 < n; i0 = i1++ ) {
        float dx = pts[ i1 ].x - pts[ i0 ].x, dy = pts[ i1 ].y - pts[ i0 ].y;
        const float d2 = dx * dx + dy * dy;
        if ( d2 > 0.f ) { const float inv = 1.f / sqrtf( d2 ); dx *= inv; dy *= inv; }
        normals[ i0 ] = ImVec2( dy, -dx );
    }

    const float alpha = GImGui->Style.Alpha;
    const ImVec2 uv = draw->_Data->TexUvWhitePixel;
    draw->PrimReserve( n * 6, n * 2 );
    const unsigned int base = draw->_VtxCurrentIdx;
    for ( int i0 = n - 1, i1 = 0; i1 < n; i0 = i1++ ) {
        float mx = ( normals[ i0 ].x + normals[ i1 ].x ) * 0.5f, my = ( normals[ i0 ].y + normals[ i1 ].y ) * 0.5f;
        const float d2 = mx * mx + my * my;
        if ( d2 > 0.000001f ) { const float inv = ImMin( 1.f / d2, 100.f ); mx *= inv; my *= inv; }
        mx *= 0.5f; my *= 0.5f;
        ImVec4 c = color_at( pts[ i1 ] );
        c.w *= alpha;
        const ImU32 inner = ImGui::ColorConvertFloat4ToU32( c );
        c.w = 0.f;
        const ImU32 outer = ImGui::ColorConvertFloat4ToU32( c );
        draw->PrimWriteVtx( ImVec2( pts[ i1 ].x - mx, pts[ i1 ].y - my ), uv, inner );
        draw->PrimWriteVtx( ImVec2( pts[ i1 ].x + mx, pts[ i1 ].y + my ), uv, outer );
    }
    for ( int i0 = n - 1, i1 = 0; i1 < n; i0 = i1++ ) {
        const ImDrawIdx in0 = ( ImDrawIdx )( base + i0 * 2 ), out0 = ( ImDrawIdx )( in0 + 1 );
        const ImDrawIdx in1 = ( ImDrawIdx )( base + i1 * 2 ), out1 = ( ImDrawIdx )( in1 + 1 );
        draw->PrimWriteIdx( in1 ); draw->PrimWriteIdx( in0 ); draw->PrimWriteIdx( out0 );
        draw->PrimWriteIdx( out0 ); draw->PrimWriteIdx( out1 ); draw->PrimWriteIdx( in1 );
    }
}

static ImVec4 sv_color( float s, float v, float hr, float hg, float hb ) {
    return ImVec4( v * ( 1.f + ( hr - 1.f ) * s ), v * ( 1.f + ( hg - 1.f ) * s ), v * ( 1.f + ( hb - 1.f ) * s ), 1.f );
}

// Saturation x value field for one hue: white -> hue across, fading to black downwards.
static void fill_sv_field( ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float hr, float hg, float hb ) {
    const hi_res_arcs arcs( draw );
    with_aa_fill_off( draw, [ & ] {
        fill_gradient( draw, min, max, ImVec4( 1.f, 1.f, 1.f, 1.f ), ImVec4( hr, hg, hb, 1.f ), false, rounding );
        fill_gradient( draw, min, max, ImVec4( 0.f, 0.f, 0.f, 0.f ), ImVec4( 0.f, 0.f, 0.f, 1.f ), true, rounding );
    } );
    aa_edge_ring( draw, min, max, rounding, [ & ]( ImVec2 p ) {
        const float s = ImSaturate( ( p.x - min.x ) / ( max.x - min.x ) );
        const float v = 1.f - ImSaturate( ( p.y - min.y ) / ( max.y - min.y ) );
        return sv_color( s, v, hr, hg, hb );
    } );
}

// Checkerboard under a color that goes from clear (left) to `rgb` (right).
static void fill_alpha_bar( ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, const ImVec4& rgb ) {
    const hi_res_arcs arcs( draw );
    with_aa_fill_off( draw, [ & ] {
        ImGui::RenderColorRectWithAlphaCheckerboard( draw, min, max, IM_COL32( 0, 0, 0, 0 ), ( max.y - min.y ) * 0.5f, ImVec2( 0.f, 0.f ), rounding );
        fill_gradient( draw, min, max, ImVec4( rgb.x, rgb.y, rgb.z, 0.f ), ImVec4( rgb.x, rgb.y, rgb.z, 1.f ), false, rounding );
    } );
    const float mid = 166.f / 255.f; // the checkerboard's two grays, averaged
    aa_edge_ring( draw, min, max, rounding, [ & ]( ImVec2 p ) {
        const float t = ImSaturate( ( p.x - min.x ) / ( max.x - min.x ) );
        return ImVec4( ImLerp( mid, rgb.x, t ), ImLerp( mid, rgb.y, t ), ImLerp( mid, rgb.z, t ), 1.f );
    } );
}

// Hue spectrum as six linear segments (exact for HSV - each sextant is linear in RGB). The
// rounded end caps go first; the square middle segments are drawn over their inner AA fringe,
// one pixel wide on each side, so no seam shows where they meet.
static void fill_hue_bar( ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding ) {
    const hi_res_arcs arcs( draw );
    const float seg = ( max.x - min.x ) / 6.f;
    ImVec4 keys[ 7 ];
    for ( int i = 0; i <= 6; ++i ) {
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB( ( i % 6 ) / 6.f, 1.f, 1.f, r, g, b );
        keys[ i ] = ImVec4( r, g, b, 1.f );
    }
    fill_gradient( draw, min, ImVec2( min.x + seg, max.y ), keys[ 0 ], keys[ 1 ], false, rounding, ImDrawFlags_RoundCornersLeft );
    fill_gradient( draw, ImVec2( max.x - seg, min.y ), max, keys[ 5 ], keys[ 6 ], false, rounding, ImDrawFlags_RoundCornersRight );
    for ( int i = 1; i < 5; ++i ) {
        const float x0 = min.x + seg * i, x1 = min.x + seg * ( i + 1 );
        const float one = 1.f;
        const ImVec4 c0 = ImLerp( keys[ i ], keys[ i + 1 ], -one / seg );
        const ImVec4 c1 = ImLerp( keys[ i ], keys[ i + 1 ], 1.f + one / seg );
        fill_gradient( draw, ImVec2( x0 - one, min.y ), ImVec2( x1 + one, max.y ), c0, c1, false, 0.f, ImDrawFlags_RoundCornersNone );
    }
}

// Swatch / preview: the color or its gradient, over a checkerboard when anything is translucent.
static void draw_color_preview( ImDrawList* draw, ImVec2 min, ImVec2 max, const color_value& c, float rounding ) {
    const ImVec4& c1 = c.gradient ? c.b : c.a;
    if ( c.a.w >= 1.f && c1.w >= 1.f ) {
        fill_gradient( draw, min, max, c.a, c1, false, rounding ); // single layer: its own AA edge is right
    } else {
        with_aa_fill_off( draw, [ & ] {
            ImGui::RenderColorRectWithAlphaCheckerboard( draw, min, max, IM_COL32( 0, 0, 0, 0 ),
                ImMax( 2.f, ( max.y - min.y ) / 3.f ), ImVec2( 0.f, 0.f ), rounding );
            fill_gradient( draw, min, max, c.a, c1, false, rounding );
        } );
        const float mid = 166.f / 255.f;
        aa_edge_ring( draw, min, max, rounding, [ & ]( ImVec2 p ) {
            const ImVec4 col = ImLerp( c.a, c1, ImSaturate( ( p.x - min.x ) / ( max.x - min.x ) ) );
            return ImVec4( ImLerp( mid, col.x, col.w ), ImLerp( mid, col.y, col.w ), ImLerp( mid, col.z, col.w ), 1.f );
        } );
    }
    // hairline so near-black (dark theme) / near-white (light theme) colors don't vanish
    draw->AddRect( min, max, ImGui::GetColorU32( gui.theme ? ImVec4( 0.f, 0.f, 0.f, 0.12f ) : ImVec4( 1.f, 1.f, 1.f, 0.08f ) ), rounding, 0, 1.f );
}

static void draw_knob( ImDrawList* draw, ImVec2 c, float r, const ImVec4& fill, float s, float alpha = 1.f ) {
    draw->AddCircleFilled( c + ImVec2( 0.f, 1.f * s ), r + 1.5f * s, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.35f * alpha ) ), 32 );
    draw->AddCircleFilled( c, r, ImGui::GetColorU32( ImVec4( 1.f, 1.f, 1.f, alpha ) ), 32 );
    draw->AddCircleFilled( c, r - 2.f * s, ImGui::GetColorU32( ImVec4( fill.x, fill.y, fill.z, alpha ) ), 32 );
}

void c_gui::draw_switch_body(ImDrawList* draw, const ImRect& bb, float t) {
    t = ImSaturate(t);
    const float rounding = bb.GetHeight() * 0.5f;
    draw->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImLerp(
        switch_off.to_vec4(1.f, false), accent_color.to_vec4(1.f, false), t)), rounding);
    draw->AddRect(bb.Min + ImVec2(0.5f, 0.5f), bb.Max - ImVec2(0.5f, 0.5f),
        switch_rim.to_im_color(1.f - t), rounding - 0.5f, 0, 1.f);
    const float inset = 2.f * m_scale;
    const float radius = ImMax(0.f, rounding - inset);
    const float x = ImLerp(bb.Min.x + rounding, bb.Max.x - rounding, t);
    draw->AddCircleFilled(ImVec2(x, bb.GetCenter().y), radius, ImGui::GetColorU32(ImLerp(
        knob_off.to_vec4(1.f, false), ImVec4(1.f, 1.f, 1.f, 1.f), t)), 32);
}

static void draw_switch(ImDrawList* draw, const ImRect& bb, float t) {
    gui.draw_switch_body(draw, bb, t);
}

static int hex_digit( char c ) {
    if ( c >= '0' && c <= '9' ) return c - '0';
    if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
    return -1;
}

// Pulls up to `max_count` non-negative integers out of free text ("255, 55 75" -> 3).
static int parse_ints( const char* s, int* out, int max_count ) {
    int n = 0, cur = 0;
    bool in_number = false;
    for ( ;; ++s ) {
        if ( *s >= '0' && *s <= '9' ) {
            cur = ImMin( cur * 10 + ( *s - '0' ), 9999 );
            in_number = true;
        } else {
            if ( in_number && n < max_count )
                out[ n++ ] = cur;
            cur = 0;
            in_number = false;
            if ( !*s )
                break;
        }
    }
    return n;
}

// Text field without a frame of its own, at an exact spot and font size. InputText() hands
// InputTextEx a fixed 130px box right-aligned to the host window, so the field gets a child of
// exactly its own size (ChildBg transparent - NoBackground would trigger the menu chrome).
static bool bare_input( const char* id, char* buf, int buf_size, ImVec2 pos, float width, float font_size, ImGuiInputTextFlags flags, bool focus = false ) {
    ImGui::SetCursorScreenPos( pos );
    ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
    ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0.f, 0.f ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.f, 0.f ) );
    ImGui::BeginChild( id, ImVec2( width, font_size ), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
    // Relative to the font's base size, not GetFontSize(): the child keeps its scale between
    // frames, so a ratio to the already-scaled size would flip back and forth every frame.
    ImGui::SetWindowFontScale( font_size / GImGui->FontBaseSize );
    if ( focus )
        ImGui::SetKeyboardFocusHere( ); // requested from inside the child - the field is the next item here
    const bool edited = ImGui::InputTextEx( "##input", NULL, buf, buf_size, ImVec2( width, 0.f ), flags, NULL, NULL );
    ImGuiContext& g = *GImGui;
    const ImGuiLastItemData item = g.LastItemData;
    ImGui::EndChild( );
    g.LastItemData = item; // callers ask IsItemActive()/IsItemDeactivated() about the field, not the child
    ImGui::PopStyleVar( 2 );
    ImGui::PopStyleColor( 2 );
    return edited;
}

static bool same_color( const color_value& x, const color_value& y ) {
    return x.gradient == y.gradient &&
        x.a.x == y.a.x && x.a.y == y.a.y && x.a.z == y.a.z && x.a.w == y.a.w &&
        x.b.x == y.b.x && x.b.y == y.b.y && x.b.z == y.b.z && x.b.w == y.b.w;
}

// Runs a *_begin() form, closes its card straight away and reports whether a color changed.
template < typename Begin >
static bool picker_once( color_slot* slots, int count, Begin begin ) {
    color_value before[ 8 ];
    const int n = ImMin( count, 8 );
    for ( int i = 0; i < n; ++i )
        before[ i ] = *slots[ i ].value;
    if ( begin( ) )
        gui.color_picker_end( );
    for ( int i = 0; i < n; ++i )
        if ( !same_color( before[ i ], *slots[ i ].value ) )
            return true;
    return false;
}

ImRect c_gui::next_checkbox_swatch_rect( ) const {
    // Mirrors ImGui::Checkbox's row geometry (row width, row height, toggle on the right edge)
    // so the swatch lands in the gap just left of the toggle about to be drawn.
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    const float avail = ImGui::GetContentRegionAvail( ).x;
    const float w = is_popup_window( window ) ? ImMin( ImGui::CalcItemWidth( ), avail ) : avail;
    const float row_h = ImMax( switch_h( ), ImGui::GetFontSize( ) ) + ImGui::GetStyle( ).FramePadding.y * 2.f;
    const float sz = 16.f * m_scale;
    const ImVec2 pos = window->DC.CursorPos;
    const float right = pos.x + w - switch_w( ) - 8.f * m_scale;
    const float top = pos.y + IM_ROUND( ( row_h - sz ) * 0.5f );
    return ImRect( ImVec2( right - sz, top ), ImVec2( right, top + sz ) );
}

bool c_gui::color_swatch_item( const char* id, const color_value& shown, const ImRect& bb ) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    const ImGuiID item_id = window->GetID( id );
    // ItemAdd without ItemSize: the swatch takes its clicks but leaves the row's layout alone.
    // Submitted before the row's own widget it also wins the hover, so a click on the swatch
    // opens the picker instead of toggling the checkbox the swatch sits on.
    if ( !ImGui::ItemAdd( bb, item_id ) )
        return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior( bb, item_id, &hovered, &held );
    const float t = cfg_anim( item_id, hovered );
    draw_color_preview( window->DrawList, bb.Min, bb.Max, shown, 4.f * m_scale );
    if ( t > 0.01f )
        window->DrawList->AddRect( bb.Min - ImVec2( 1.f, 1.f ) * m_scale, bb.Max + ImVec2( 1.f, 1.f ) * m_scale,
            ImGui::GetColorU32( ImVec4( 1.f, 1.f, 1.f, 0.35f * t ) ), 5.f * m_scale, 0, 1.f * m_scale );
    return pressed;
}

bool c_gui::color_card_begin( color_slot* slots, int count, bool clicked, const ImRect& swatch, bool allow_gradient ) {
    const float s = m_scale;
    const float card_w = 274.f;
    const float pad = 16.f; // room for a knob sitting on the field's corner (radius + shadow)
    const ImGuiID picker_id = ImGui::GetID( "##picker_card" );

    // Beside the swatch - to its right when there is room on screen, mirrored to its left if not.
    const bool right = swatch.Max.x + 12.f * s + card_w * s <= ImGui::GetIO( ).DisplaySize.x - 10.f * s;
    const ImVec2 anchor( right ? swatch.Max.x + 12.f * s : swatch.Min.x - 12.f * s, swatch.Min.y - 18.f * s );
    bool open = false;
    const float anim = popup_animation( "##picker_card", clicked, open );
    if ( !begin_popup_card_at( "##picker_card", open, anim, anchor, ImVec2( right ? 0.f : 1.f, 0.f ), card_w, pad ) )
        return false;

    int& slot = m_color_slot[ picker_id ];
    slot = ImClamp( slot, 0, count - 1 );

    if ( count > 1 ) {
        // Chip row, centered: one chip per color, a ring that slides to the chip being edited,
        // hovered chips grow a little, and the edited color's name fades in above them.
        color_view_state& view = m_color_views[ picker_id ];
        ImGuiIO& io = ImGui::GetIO( );
        ImDrawList* draw = ImGui::GetWindowDrawList( );
        ImFont* font = ImGui::GetFont( );
        const float dt = ImMin( io.DeltaTime, 0.05f );
        const float ease = 1.f - expf( -18.f * dt );
        const float w = ImGui::GetContentRegionAvail( ).x;
        const ImVec2 p = ImGui::GetCursorScreenPos( );
        const float chip = 16.f * s, gap = 9.f * s, name_fs = 14.f * s, name_h = 16.f * s;
        const float row_w = count * chip + ( count - 1 ) * gap;
        const float x0 = p.x + IM_ROUND( ( w - row_w ) * 0.5f );
        const float cy = p.y + name_h + 8.f * s;

        for ( int i = 0; i < count; ++i ) {
            const ImVec2 cmin( x0 + i * ( chip + gap ), cy );
            ImGui::PushID( i );
            const ImGuiID chip_id = ImGui::GetID( "##chip" );
            ImGui::SetCursorScreenPos( cmin );
            if ( ImGui::InvisibleButton( "##chip", ImVec2( chip, chip ) ) )
                slot = i;
            const float ht = cfg_anim( chip_id, ImGui::IsItemHovered( ) && i != slot, 14.f );
            ImGui::PopID( );
            const float grow = 1.5f * s * ht;
            draw_color_preview( draw, cmin - ImVec2( grow, grow ), cmin + ImVec2( chip + grow, chip + grow ), *slots[ i ].value, 4.f * s );
            if ( ht > 0.01f )
                draw->AddRect( cmin - ImVec2( 3.f, 3.f ) * s, cmin + ImVec2( chip + 3.f * s, chip + 3.f * s ),
                    ImGui::GetColorU32( text.to_vec4( 0.25f * ht, false ) ), 6.f * s, 0, 1.f * s );
        }

        const float target = slot * ( chip + gap );
        if ( view.shown_slot < 0 )
            view.ring_x = target;
        view.ring_x = ImLerp( view.ring_x, target, ease );
        if ( fabsf( target - view.ring_x ) < 0.25f )
            view.ring_x = target;
        const float r = 3.f * s;
        draw->AddRect( ImVec2( x0 + view.ring_x - r, cy - r ), ImVec2( x0 + view.ring_x + chip + r, cy + chip + r ),
            accent_color.to_im_color( ), 6.f * s, 0, 1.5f * s );

        if ( view.shown_slot != slot ) {
            view.shown_slot = slot;
            view.label_t = 0.f;
        }
        view.label_t = ImMin( 1.f, view.label_t + dt / 0.2f );
        if ( const char* name = tr( slots[ slot ].name ) ) {
            const float lt = 1.f - ( 1.f - view.label_t ) * ( 1.f - view.label_t ) * ( 1.f - view.label_t );
            const char* end = ImGui::FindRenderedTextEnd( name );
            const ImVec2 tsz = font->CalcTextSizeA( name_fs, FLT_MAX, 0.f, name, end );
            draw->AddText( font, name_fs, ImVec2( IM_ROUND( p.x + ( w - tsz.x ) * 0.5f ), IM_ROUND( p.y + ( 1.f - lt ) * 5.f * s ) ),
                text.to_im_color( lt ), name, end );
        }
        ImGui::SetCursorScreenPos( ImVec2( p.x, cy + chip + 16.f * s ) );
    }

    const int state_slot = slot;
    color_card_body( ImHashData( &state_slot, sizeof( state_slot ), picker_id ), picker_id, picker_id, slots[ slot ].value, allow_gradient );
    return true;
}

void c_gui::color_card_body( ImGuiID state_id, ImGuiID mode_id, ImGuiID view_id, color_value* col, bool allow_gradient ) {
    color_edit_state& st = m_color_states[ state_id ];
    int& mode = m_color_mode[ mode_id ];
    ImGuiIO& io = ImGui::GetIO( );
    ImDrawList* draw = ImGui::GetWindowDrawList( );
    ImFont* font = ImGui::GetFont( );
    const float s = m_scale;
    const float w = ImGui::GetContentRegionAvail( ).x;
    const ImVec2 p = ImGui::GetCursorScreenPos( );
    const ImVec2 m = io.MousePos;

    if ( !allow_gradient )
        col->gradient = false;
    ImVec4* stops[ 2 ] = { &col->a, &col->b };

    // Re-derive HSV only when the color changed from outside (or on first use). Grays have no
    // hue and black no saturation - keep the previous ones so the knobs don't jump.
    for ( int i = 0; i < 2; ++i ) {
        const ImVec4& c = *stops[ i ];
        const ImVec4& k = st.synced[ i ];
        if ( c.x != k.x || c.y != k.y || c.z != k.z || c.w != k.w ) {
            float h, sat, val;
            ImGui::ColorConvertRGBtoHSV( c.x, c.y, c.z, h, sat, val );
            if ( sat > 0.f && val > 0.f ) st.h[ i ] = h;
            if ( val > 0.f ) st.s[ i ] = sat;
            st.v[ i ] = val;
            st.synced[ i ] = c;
        }
    }
    if ( !col->gradient )
        st.stop = 0;

    auto commit = [ & ]( int i ) {
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB( st.h[ i ], st.s[ i ], st.v[ i ], r, g, b );
        stops[ i ]->x = r; stops[ i ]->y = g; stops[ i ]->z = b;
        st.synced[ i ] = *stops[ i ];
    };
    auto start_gradient = [ & ]( float sat, float val ) {
        col->gradient = true;
        st.h[ 1 ] = st.h[ 0 ];
        st.s[ 1 ] = sat;
        st.v[ 1 ] = val;
        col->b.w = col->a.w;
        commit( 1 );
        st.stop = 1;
    };

    // ---- color field (saturation x value) ----
    const ImRect sv( p, p + ImVec2( w, IM_ROUND( w * 0.82f ) ) ); // squarish, not a square - a full
                                                                 // square made the card very tall
    const float knob_r = 7.f * s;
    auto knob_pos = [ & ]( int i ) { // real position: what clicks are tested against
        return ImVec2( sv.Min.x + st.s[ i ] * sv.GetWidth( ), sv.Min.y + ( 1.f - st.v[ i ] ) * sv.GetHeight( ) );
    };
    auto near_knob = [ & ]( int i ) {
        const float reach = knob_r + 4.f * s;
        return ImLengthSqr( m - knob_pos( i ) ) <= reach * reach;
    };
    const float ms = ImSaturate( ( m.x - sv.Min.x ) / sv.GetWidth( ) );
    const float mv = 1.f - ImSaturate( ( m.y - sv.Min.y ) / sv.GetHeight( ) );

    ImGui::SetCursorScreenPos( sv.Min );
    ImGui::InvisibleButton( "##sv", sv.GetSize( ) );
    const bool sv_hovered = ImGui::IsItemHovered( );
    const bool sv_active = ImGui::IsItemActive( );
    if ( ImGui::IsItemActivated( ) ) {
        // Left click grabs whichever knob is under the cursor; anywhere else moves the knob
        // being edited there.
        st.grab = st.stop;
        if ( col->gradient ) {
            const int nearest = ImLengthSqr( m - knob_pos( 0 ) ) <= ImLengthSqr( m - knob_pos( 1 ) ) ? 0 : 1;
            if ( near_knob( nearest ) )
                st.grab = nearest;
        }
        st.stop = st.grab;
    }
    if ( sv_active && st.grab >= 0 ) {
        st.s[ st.grab ] = ms;
        st.v[ st.grab ] = mv;
        commit( st.grab );
    } else if ( !sv_active ) {
        st.grab = -1;
    }
    if ( allow_gradient && sv_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) ) {
        // Right click: first one adds the second stop right there; on a knob it removes that
        // knob (the other one stays as the plain color); elsewhere it moves the second stop.
        if ( !col->gradient ) {
            start_gradient( ms, mv );
        } else if ( near_knob( 1 ) ) {
            col->gradient = false;
            st.stop = 0;
        } else if ( near_knob( 0 ) ) {
            col->a = col->b;
            st.h[ 0 ] = st.h[ 1 ]; st.s[ 0 ] = st.s[ 1 ]; st.v[ 0 ] = st.v[ 1 ];
            st.synced[ 0 ] = col->a;
            col->gradient = false;
            st.stop = 0;
        } else {
            st.s[ 1 ] = ms;
            st.v[ 1 ] = mv;
            commit( 1 );
            st.stop = 1;
        }
    }

    // ---- hue / alpha bars (hit area taller than the bar, easier to grab) ----
    const float bar_h = 8.f * s;
    const ImRect hue( ImVec2( p.x, sv.Max.y + 14.f * s ), ImVec2( p.x + w, sv.Max.y + 14.f * s + bar_h ) );
    ImGui::SetCursorScreenPos( hue.Min - ImVec2( 0.f, 5.f * s ) );
    ImGui::InvisibleButton( "##hue", ImVec2( w, bar_h + 10.f * s ) );
    const bool hue_active = ImGui::IsItemActive( );
    if ( hue_active ) {
        st.h[ st.stop ] = ImClamp( ( m.x - hue.Min.x ) / hue.GetWidth( ), 0.f, 0.9999f );
        commit( st.stop );
    }

    const ImRect alp( ImVec2( p.x, hue.Max.y + 12.f * s ), ImVec2( p.x + w, hue.Max.y + 12.f * s + bar_h ) );
    ImGui::SetCursorScreenPos( alp.Min - ImVec2( 0.f, 5.f * s ) );
    ImGui::InvisibleButton( "##alpha", ImVec2( w, bar_h + 10.f * s ) );
    const bool alpha_active = ImGui::IsItemActive( );
    if ( alpha_active ) {
        stops[ st.stop ]->w = ImSaturate( ( m.x - alp.Min.x ) / alp.GetWidth( ) );
        st.synced[ st.stop ] = *stops[ st.stop ];
    }

    // ---- input row: [v HEX] [swatch #RRGGBBAA] [82%] [gradient] ----
    const int e = st.stop;
    ImVec4& cur = *stops[ e ];
    // The whole row (HEX / RGB, the value, the percentage) is drawn with the caption face at its
    // baked size - asking the 15px body font for 14px text is a scaled bitmap, and that is what
    // made this line look rough next to the rest of the card.
    ImFont* row_font = small_font( );
    const float fs = row_font->FontSize;
    const float row_h = 26.f * s;
    const float ry = alp.Max.y + 14.f * s;
    const float ty = IM_ROUND( ry + row_h * 0.5f - TextLineCenterY( row_font, fs ) );
    const ImU32 box_col = field_bg.to_im_color( );
    const ImVec4 c_dim = text_disabled.to_vec4( 1.f, false ), c_text = text.to_vec4( 1.f, false );

    const char* fmt = mode == 0 ? "HEX" : "RGB";
    const float chev_fs = 11.f * s;
    const ImVec2 chev_sz = row_font->CalcTextSizeA( chev_fs, FLT_MAX, 0.f, ICON_FA_CHEVRON_DOWN );
    const ImVec2 fmt_sz = row_font->CalcTextSizeA( fs, FLT_MAX, 0.f, fmt );
    const float fmt_w = chev_sz.x + 5.f * s + fmt_sz.x;
    ImGui::SetCursorScreenPos( ImVec2( p.x, ry ) );
    const ImGuiID fmt_id = ImGui::GetID( "##fmt" );
    if ( ImGui::InvisibleButton( "##fmt", ImVec2( fmt_w + 4.f * s, row_h ) ) ) {
        mode ^= 1;
        st.text[ 0 ] = '\0';
    }
    const float fmt_t = cfg_anim( fmt_id, ImGui::IsItemHovered( ) );

    const float btn_w = allow_gradient ? 22.f * s : 0.f;
    const float pct_w = 44.f * s, gap = 6.f * s;
    const float btn_x = p.x + w - btn_w;
    const float pct_x = btn_x - ( allow_gradient ? gap : 0.f ) - pct_w;
    const float val_x = p.x + fmt_w + 12.f * s;
    const float val_w = ImMax( 40.f * s, pct_x - gap - val_x );

    // value box
    const float sw = 12.f * s;
    const ImVec2 sw_min( val_x + 8.f * s, ry + IM_ROUND( ( row_h - sw ) * 0.5f ) );
    float tx = sw_min.x + sw + 7.f * s;
    if ( mode == 0 )
        tx += row_font->CalcTextSizeA( fs, FLT_MAX, 0.f, "#" ).x + 1.f * s;
    if ( !st.text_active ) {
        const int r = ( int )IM_ROUND( cur.x * 255.f ), g = ( int )IM_ROUND( cur.y * 255.f );
        const int b = ( int )IM_ROUND( cur.z * 255.f ), a = ( int )IM_ROUND( cur.w * 255.f );
        if ( mode == 0 ) ImFormatString( st.text, IM_ARRAYSIZE( st.text ), "%02X%02X%02X%02X", r, g, b, a );
        else             ImFormatString( st.text, IM_ARRAYSIZE( st.text ), "%d, %d, %d", r, g, b );
    }
    const ImGuiInputTextFlags text_flags = ImGuiInputTextFlags_AutoSelectAll |
        ( mode == 0 ? ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase : ImGuiInputTextFlags_None );
    ImGui::PushFont( row_font ); // so the field is drawn at the size the face was baked at
    const bool val_edited = bare_input( "##val", st.text, IM_ARRAYSIZE( st.text ), ImVec2( tx, ty ), ImMax( 1.f, val_x + val_w - 8.f * s - tx ), fs, text_flags );
    ImGui::PopFont( );
    if ( val_edited ) {
        if ( mode == 0 ) {
            unsigned int bits = 0;
            int digits = 0;
            for ( const char* c = st.text; *c; ++c )
                if ( hex_digit( *c ) >= 0 ) { bits = ( bits << 4 ) | ( unsigned int )hex_digit( *c ); ++digits; }
            if ( digits == 6 || digits == 8 ) {
                if ( digits == 6 ) bits = ( bits << 8 ) | ( unsigned int )IM_ROUND( cur.w * 255.f );
                cur = ImVec4( ( ( bits >> 24 ) & 0xFF ) / 255.f, ( ( bits >> 16 ) & 0xFF ) / 255.f,
                              ( ( bits >> 8 ) & 0xFF ) / 255.f, ( bits & 0xFF ) / 255.f );
            }
        } else {
            int rgb[ 3 ] = {};
            if ( parse_ints( st.text, rgb, 3 ) == 3 )
                cur = ImVec4( ImMin( rgb[ 0 ], 255 ) / 255.f, ImMin( rgb[ 1 ], 255 ) / 255.f, ImMin( rgb[ 2 ], 255 ) / 255.f, cur.w );
        }
    }
    st.text_active = ImGui::IsItemActive( );

    // alpha box: shows "82%", becomes a field on click, mouse wheel nudges it by 1%
    const int pct = ( int )IM_ROUND( cur.w * 100.f );
    if ( st.pct_editing ) {
        if ( st.pct_focus == 3 )
            ImGui::SetKeyboardFocusHere( );
        const float pf = ImMax( 1.f, pct_w - 16.f * s );
        ImGui::PushFont( row_font );
        const bool pct_edited = bare_input( "##pct", st.pct, IM_ARRAYSIZE( st.pct ), ImVec2( pct_x + 8.f * s, ty ), pf, fs,
                                            ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll );
        ImGui::PopFont( );
        if ( pct_edited ) {
            int v = 0;
            if ( parse_ints( st.pct, &v, 1 ) == 1 )
                cur.w = ImMin( v, 100 ) / 100.f;
        }
        // the field is only focusable from the frame after the click - give it a few frames
        if ( st.pct_focus > 0 ) --st.pct_focus;
        else if ( !ImGui::IsItemActive( ) ) st.pct_editing = false;
    } else {
        ImGui::SetCursorScreenPos( ImVec2( pct_x, ry ) );
        if ( ImGui::InvisibleButton( "##pct_box", ImVec2( pct_w, row_h ) ) ) {
            st.pct_editing = true;
            st.pct_focus = 3;
            ImFormatString( st.pct, IM_ARRAYSIZE( st.pct ), "%d", pct );
        }
        if ( ImGui::IsItemHovered( ) && io.MouseWheel != 0.f )
            cur.w = ImSaturate( cur.w + io.MouseWheel * 0.01f );
    }

    // gradient toggle (same as right-clicking the field)
    float grad_t = 0.f;
    if ( allow_gradient ) {
        ImGui::SetCursorScreenPos( ImVec2( btn_x, ry ) );
        const ImGuiID grad_id = ImGui::GetID( "##grad" );
        if ( ImGui::InvisibleButton( "##grad", ImVec2( btn_w, row_h ) ) ) {
            if ( col->gradient ) {
                col->gradient = false;
                st.stop = 0;
            } else {
                start_gradient( st.s[ 0 ], st.v[ 0 ] > 0.5f ? st.v[ 0 ] - 0.4f : st.v[ 0 ] + 0.4f );
            }
        }
        grad_t = cfg_anim( grad_id, ImGui::IsItemHovered( ) );
    }

    // ---- what the card shows: eased towards the real values (see color_view_state) ----
    // Typed values, switched colors (multi picker) and toggled stops glide; anything the mouse
    // is dragging right now follows it exactly.
    color_view_state& view = m_color_views[ view_id ];
    const int d = st.stop;
    {
        const float ease = 1.f - expf( -18.f * ImMin( io.DeltaTime, 0.05f ) );
        auto approach = [ & ]( float& value, float target ) {
            value = ImLerp( value, target, ease );
            if ( fabsf( target - value ) < 0.0005f )
                value = target;
        };
        if ( !view.init || sv_active || hue_active || alpha_active ) {
            view.h = st.h[ d ];
            view.a = stops[ d ]->w;
            for ( int i = 0; i < 2; ++i ) { view.s[ i ] = st.s[ i ]; view.v[ i ] = st.v[ i ]; }
            if ( !view.init )
                view.grad = col->gradient ? 1.f : 0.f;
            view.init = true;
        } else {
            float dh = st.h[ d ] - view.h; // the short way round the hue wheel
            if ( dh > 0.5f ) dh -= 1.f; else if ( dh < -0.5f ) dh += 1.f;
            view.h += dh * ease;
            if ( view.h < 0.f ) view.h += 1.f; else if ( view.h >= 1.f ) view.h -= 1.f;
            if ( fabsf( dh ) < 0.0005f ) view.h = st.h[ d ];
            approach( view.a, stops[ d ]->w );
            for ( int i = 0; i < 2; ++i ) { approach( view.s[ i ], st.s[ i ] ); approach( view.v[ i ], st.v[ i ] ); }
        }
        approach( view.grad, col->gradient ? 1.f : 0.f );
    }
    auto view_pos = [ & ]( int i ) {
        return ImVec2( sv.Min.x + view.s[ i ] * sv.GetWidth( ), sv.Min.y + ( 1.f - view.v[ i ] ) * sv.GetHeight( ) );
    };

    // ---- draw (after every item above, so it all reflects this frame's edits) ----
    float hr, hg, hb;
    ImGui::ColorConvertHSVtoRGB( view.h, 1.f, 1.f, hr, hg, hb );
    fill_sv_field( draw, sv.Min, sv.Max, 8.f * s, hr, hg, hb );
    if ( view.grad > 0.01f ) {
        const ImVec2 k0 = view_pos( 0 ), k1 = view_pos( 1 );
        const ImVec2 dir = k1 - k0;
        const float len = sqrtf( ImLengthSqr( dir ) );
        if ( len > knob_r * 2.f ) {
            const ImVec2 u = dir / len;
            const ImVec2 a = k0 + u * knob_r, b = k1 - u * knob_r;
            draw->AddLine( a, b, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.35f * view.grad ) ), 3.5f * s );
            draw->AddLine( a, b, ImGui::GetColorU32( ImVec4( 1.f, 1.f, 1.f, 0.95f * view.grad ) ), 1.5f * s );
        }
        draw_knob( draw, view_pos( 1 - d ), knob_r - 1.f * s, *stops[ 1 - d ], s, view.grad );
    }
    const ImVec4 shown = sv_color( view.s[ d ], view.v[ d ], hr, hg, hb );
    draw_knob( draw, view_pos( d ), knob_r, shown, s );

    fill_hue_bar( draw, hue.Min, hue.Max, bar_h * 0.5f );
    {
        const float hx = ImClamp( hue.Min.x + view.h * hue.GetWidth( ), hue.Min.x + 3.5f * s, hue.Max.x - 3.5f * s );
        const ImVec2 pmin( hx - 3.5f * s, hue.Min.y - 4.f * s ), pmax( hx + 3.5f * s, hue.Max.y + 4.f * s );
        draw->AddRectFilled( pmin - ImVec2( 1.f, 0.f ) * s, pmax + ImVec2( 1.f, 1.5f ) * s, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.3f ) ), 4.5f * s );
        draw->AddRectFilled( pmin, pmax, ImGui::GetColorU32( ImVec4( 1.f, 1.f, 1.f, 1.f ) ), 3.5f * s );
        draw->AddRectFilled( pmin + ImVec2( 1.5f, 1.5f ) * s, pmax - ImVec2( 1.5f, 1.5f ) * s, ImGui::GetColorU32( ImVec4( hr, hg, hb, 1.f ) ), 2.f * s );
    }

    fill_alpha_bar( draw, alp.Min, alp.Max, bar_h * 0.5f, shown );
    {
        const float ax = ImClamp( alp.Min.x + view.a * alp.GetWidth( ), alp.Min.x + 6.f * s, alp.Max.x - 6.f * s );
        const ImVec2 c( ax, alp.GetCenter( ).y );
        draw->AddCircleFilled( c + ImVec2( 0.f, 1.f * s ), 7.5f * s, ImGui::GetColorU32( ImVec4( 0.f, 0.f, 0.f, 0.35f ) ), 32 );
        draw->AddCircleFilled( c, 6.f * s, ImGui::GetColorU32( ImVec4( 1.f, 1.f, 1.f, 1.f ) ), 32 );
    }

    const ImU32 fmt_col = cfg_mix( c_dim, c_text, fmt_t );
    draw->AddText( row_font, chev_fs, ImVec2( p.x, IM_ROUND( ry + row_h * 0.5f - TextInkCenterY( row_font, chev_fs, ICON_FA_CHEVRON_DOWN ) ) ), fmt_col, ICON_FA_CHEVRON_DOWN );
    draw->AddText( row_font, fs, ImVec2( p.x + chev_sz.x + 5.f * s, IM_ROUND( ry + row_h * 0.5f - TextLineCenterY( row_font, fs ) ) ), fmt_col, fmt );

    draw->AddRectFilled( ImVec2( val_x, ry ), ImVec2( val_x + val_w, ry + row_h ), box_col, 6.f * s );
    draw_color_preview( draw, sw_min, sw_min + ImVec2( sw, sw ), *col, 3.f * s );
    if ( mode == 0 )
        draw->AddText( row_font, fs, ImVec2( sw_min.x + sw + 7.f * s, ty ), text_disabled.to_im_color( ), "#" );

    draw->AddRectFilled( ImVec2( pct_x, ry ), ImVec2( pct_x + pct_w, ry + row_h ), box_col, 6.f * s );
    if ( !st.pct_editing ) {
        char label[ 8 ];
        ImFormatString( label, IM_ARRAYSIZE( label ), "%d%%", pct );
        const ImVec2 lsz = font->CalcTextSizeA( fs, FLT_MAX, 0.f, label );
        draw->AddText( row_font, fs, ImVec2( IM_ROUND( pct_x + ( pct_w - lsz.x ) * 0.5f ), ty ), text.to_im_color( ), label );
    }

    if ( allow_gradient ) {
        const ImVec2 isz = font->CalcTextSizeA( fs, FLT_MAX, 0.f, ICON_FA_ADJUST );
        const ImU32 icol = col->gradient ? accent_color.to_im_color( ) : cfg_mix( c_dim, c_text, grad_t );
        draw->AddText( row_font, fs, ImVec2( IM_ROUND( btn_x + ( btn_w - isz.x ) * 0.5f ), IM_ROUND( ry + row_h * 0.5f - TextInkCenterY( row_font, fs, ICON_FA_ADJUST ) ) ), icol, ICON_FA_ADJUST );
    }

    // Register the card's content bottom, then leave room for a separator and any extras. The
    // room only moves the cursor, so a card without extras keeps its normal bottom padding -
    // and color_card_end() only draws the separator when something was added below.
    ImGui::SetCursorScreenPos( ImVec2( p.x, ry + row_h ) );
    ImGui::Dummy( ImVec2( w, 0.f ) );
    m_picker_marks.push_back( { ry + row_h, IM_ROUND( ry + row_h + 12.f * s ), p.x, w } );
    ImGui::SetCursorScreenPos( ImVec2( p.x, ry + row_h + 20.f * s ) );
}

void c_gui::color_card_end( ) {
    if ( !m_picker_marks.empty( ) ) {
        const picker_mark mark = m_picker_marks.back( );
        m_picker_marks.pop_back( );
        ImGuiWindow* card = ImGui::GetCurrentWindow( );
        if ( card->DC.CursorMaxPos.y > mark.bottom + 1.f )
            card->DrawList->AddLine( ImVec2( mark.x, mark.line_y ), ImVec2( mark.x + mark.w, mark.line_y ),
                popup_border.to_im_color( 0.55f ), 1.f * m_scale );
    }
    end_popup_card( );
}

bool c_gui::color_begin_at( const char* id, color_slot* slots, int count, const ImRect& swatch, bool allow_gradient ) {
    ImGui::PushID( id );
    const int slot = ImClamp( m_color_slot[ ImGui::GetID( "##picker_card" ) ], 0, count - 1 );
    const bool clicked = color_swatch_item( "##swatch", *slots[ slot ].value, swatch );
    if ( color_card_begin( slots, count, clicked, swatch, allow_gradient ) )
        return true; // PopID in color_picker_end()
    ImGui::PopID( );
    return false;
}

bool c_gui::color_picker_begin( const char* id, color_slot* slots, int count ) {
    if ( count <= 0 || ImGui::GetCurrentWindow( )->SkipItems )
        return false;
    return color_begin_at( id, slots, count, next_checkbox_swatch_rect( ), true );
}

bool c_gui::color_picker_begin( const char* id, color_value* col ) {
    color_slot one{ nullptr, col };
    return color_picker_begin( id, &one, 1 );
}

bool c_gui::color_picker( const char* id, color_slot* slots, int count ) {
    return picker_once( slots, count, [ & ] { return color_picker_begin( id, slots, count ); } );
}

bool c_gui::color_picker( const char* id, color_value* col ) {
    color_slot one{ nullptr, col };
    return color_picker( id, &one, 1 );
}

bool c_gui::row_color_begin( const char* label, color_slot* slots, int count, const char* description ) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if (count <= 0) return false;
    if (window->SkipItems) { search_note(label); return false; }
    row_begin( );

    const ImGuiStyle& style = ImGui::GetStyle( );
    const float avail = ImGui::GetContentRegionAvail( ).x;
    const float w = is_popup_window( window ) ? ImMin( ImGui::CalcItemWidth( ), avail ) : avail;
    const char* shown = tr( label ); // IDs below stay on the English label
    const ImVec2 lsz = ImGui::CalcTextSize( shown, NULL, true );
    const float row_h = ImMax( switch_h( ), lsz.y ) + style.FramePadding.y * 2.f;
    const ImVec2 pos = window->DC.CursorPos;
    const float sz = 16.f * m_scale;
    const ImRect swatch( ImVec2( pos.x + w - sz, pos.y + IM_ROUND( ( row_h - sz ) * 0.5f ) ),
                         ImVec2( pos.x + w, pos.y + IM_ROUND( ( row_h - sz ) * 0.5f ) + sz ) );

    ImGui::PushID( label );
    const ImGuiID picker_id = ImGui::GetID( "##picker_card" );
    const int slot = ImClamp( m_color_slot[ picker_id ], 0, count - 1 );
    const bool clicked = color_swatch_item( "##swatch", *slots[ slot ].value, swatch ); // first: owns the hover

    // The row itself: layout, hover and tooltip for the label (the swatch is the only button).
    const ImRect row( pos, pos + ImVec2( w, row_h ) );
    const ImGuiID row_id = window->GetID( "##row" );
    const float flash = search_note( label );
    ImGui::ItemSize( row, style.FramePadding.y );
    if ( ImGui::ItemAdd( row, row_id ) ) {
        const bool open = ImGui::IsPopupOpen( picker_id, ImGuiPopupFlags_None );
        const bool row_hovered = ImGui::IsItemHovered( );
        const float t = cfg_anim( row_id, row_hovered || open );
        ImVec4 col = ImLerp( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), t );
        col = ImLerp( col, accent_color.to_vec4( 1.f, false ), flash ); // jumped here from the search
        RenderScrollingText( window->DrawList, row_id, ImRect( pos.x, row.Min.y, swatch.Min.x - 8.f * m_scale, row.Max.y ),
                             shown, ImGui::GetColorU32( col ), row_hovered );
        info_tooltip( label, description );
    }

    if ( color_card_begin( slots, count, clicked, swatch, true ) )
        return true; // PopID in color_picker_end()
    ImGui::PopID( );
    return false;
}

bool c_gui::row_color_begin( const char* label, color_value* col, const char* description ) {
    color_slot one{ label, col };
    return row_color_begin( label, &one, 1, description );
}

bool c_gui::row_color( const char* label, color_slot* slots, int count, const char* description ) {
    return picker_once( slots, count, [ & ] { return row_color_begin( label, slots, count, description ); } );
}

bool c_gui::row_color( const char* label, color_value* col, const char* description ) {
    color_slot one{ label, col };
    return row_color( label, &one, 1, description );
}

bool c_gui::row_checkbox( const char* label, bool* v, color_value* col, const char* description ) {
    row_begin( );
    color_picker( label, col );
    const bool changed = Checkbox( label, v );
    info_tooltip( label, description );
    return changed;
}

ImRect c_gui::next_combo_swatch_rect( ) const {
    // Mirrors ImGui::BeginCombo's row geometry, the way next_checkbox_swatch_rect() mirrors
    // Checkbox's: the swatch lands in the gap just left of the combo box about to be drawn.
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    const float avail = ImGui::GetContentRegionAvail( ).x;
    const float w = is_popup_window( window ) ? ImMin( ImGui::CalcItemWidth( ), avail ) : avail;
    const float row_h = ImMax( 19.f * m_scale, ImGui::GetFontSize( ) ) + ImGui::GetStyle( ).FramePadding.y * 2.f;
    const float combo_w = ImMin( 130.f * m_scale, ImMax( 70.f * m_scale, w - 72.f * m_scale ) );
    const float sz = 16.f * m_scale;
    const ImVec2 pos = window->DC.CursorPos;
    const float right = pos.x + w - combo_w - 8.f * m_scale;
    const float top = pos.y + IM_ROUND( ( row_h - sz ) * 0.5f );
    return ImRect( ImVec2( right - sz, top ), ImVec2( right, top + sz ) );
}

bool c_gui::row_combo( const char* label, color_value* col, int* index, const char* const items[], int count ) {
    row_begin( );
    if ( col ) {
        color_slot one{ label, col };
        picker_once( &one, 1, [ & ] { return color_begin_at( label, &one, 1, next_combo_swatch_rect( ), true ); } );
    }
    return ImGui::Combo( label, index, items, count );
}

void c_gui::color_picker_end( ) {
    color_card_end( );
    ImGui::PopID( );
}

// ---------------------------------------------------------------------------------------
//  Rows for popup cards
// ---------------------------------------------------------------------------------------

float c_gui::popup_row_width( ) const {
    // Same rule as Checkbox: inside a popup section the section's item width bounds the row.
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    const float avail = ImGui::GetContentRegionAvail( ).x;
    return is_popup_window( window ) ? ImMin( ImGui::CalcItemWidth( ), avail ) : avail;
}

c_gui::popup_row_state c_gui::popup_row_impl( const char* icon, const char* label, bool lit ) {
    ImDrawList* draw = ImGui::GetWindowDrawList( );
    ImFont* font = ImGui::GetFont( );
    const float s = m_scale, w = popup_row_width( ), row_h = 32.f * s, icon_w = 22.f * s;
    const float fs = ImGui::GetFontSize( ), small = 15.f * s;
    const ImVec4 c_text = text.to_vec4( 1.f, false ), c_dim = text_disabled.to_vec4( 1.f, false );

    const ImVec2 rp = ImGui::GetCursorScreenPos( );
    const ImGuiID rid = ImGui::GetID( label );
    const bool clicked = ImGui::InvisibleButton( label, ImVec2( w, row_h ) );
    const float t = cfg_anim( rid, ImGui::IsItemHovered( ) || lit );
    const float cy = rp.y + row_h * 0.5f;

    float lx = rp.x + 4.f * s;
    if ( icon ) {
        const ImVec2 isz = font->CalcTextSizeA( small, FLT_MAX, 0.f, icon );
        draw->AddText( font, small, ImVec2( IM_ROUND( lx + ( icon_w - isz.x ) * 0.5f ), IM_ROUND( cy - isz.y * 0.5f ) ),
            cfg_mix( c_dim, c_text, t * 0.6f ), icon );
        lx += icon_w + 10.f * s;
    }
    const char* shown = tr( label );
    const char* end = ImGui::FindRenderedTextEnd( shown );
    draw->AddText( font, fs, ImVec2( lx, IM_ROUND( cy - TextLineCenterY( font, fs ) ) ),
        cfg_mix( ImVec4( c_text.x, c_text.y, c_text.z, 0.8f ), c_text, t ), shown, end );
    const float label_end = lx + font->CalcTextSizeA( fs, FLT_MAX, 0.f, shown, end ).x;
    return { clicked, rp, t, w, label_end, ImGui::IsItemHovered( ) };
}

void c_gui::popup_row_value( const popup_row_state& r, const char* value ) {
    ImDrawList* draw = ImGui::GetWindowDrawList( );
    ImFont* font = ImGui::GetFont( );
    const float s = m_scale, row_h = 32.f * s, small = 15.f * s;
    const float cy = r.pos.y + row_h * 0.5f;
    ImFont* caption = small_font( );
    const float chev_fs = caption->FontSize;
    const ImVec2 csz = caption->CalcTextSizeA( chev_fs, FLT_MAX, 0.f, ICON_FA_CHEVRON_RIGHT );
    const float cx = r.pos.x + r.w - 4.f * s - csz.x;
    const ImU32 col = cfg_mix( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), r.t * 0.6f );
    draw->AddText( caption, chev_fs, ImVec2( cx, IM_ROUND( cy - csz.y * 0.5f ) ), col, ICON_FA_CHEVRON_RIGHT );
    const char* shown = tr( value );
    const ImVec2 vsz = font->CalcTextSizeA( small, FLT_MAX, 0.f, shown );
    const float right = cx - 6.f * s;
    const float left = r.label_end + 12.f * s;

    if ( vsz.x <= right - left ) {
        draw->AddText( font, small, ImVec2( IM_ROUND( right - vsz.x ), IM_ROUND( cy - TextLineCenterY( font, small ) ) ), col, shown );
    }
    else {
        // Too long for the room the label leaves. Same treatment the multi-combo preview gets:
        // clipped with a fade on the leading edge, and it scrolls itself while the row is hovered.
        const ImRect vr( ImVec2( left, r.pos.y ), ImVec2( right, r.pos.y + row_h ) );
        RenderScrollingText( draw, ImHashStr( "##rowval", 0, ImGui::GetID( value ) ), vr, shown, col,
            r.hovered, font, small );
    }
}

// Option cards and config sorting share the same animated selection row.
bool c_gui::popup_option_card( const char* popup_name, bool clicked, float row_y, const char* const* options, int count, int* index ) {
    ImGuiWindow* card = ImGui::GetCurrentWindow( );
    const float s = m_scale;
    bool open = false;
    const float a = popup_animation( popup_name, clicked, open );
    const ImVec2 at( card->Pos.x + card->Size.x + 8.f * s, row_y - 10.f * s );
    bool changed = false;
    if ( begin_popup_card_at( popup_name, open, a, at, ImVec2( 0.f, 0.f ), cfg_choice_width( options, count ) ) ) {
        const ImVec4 c_text = text.to_vec4( 1.f, false ), c_dim = text_disabled.to_vec4( 1.f, false );
        const ImVec4 c_accent = accent_color.to_vec4( 1.f, false );
        const float mw = ImGui::GetContentRegionAvail( ).x, mh = 34.f * s;
        for ( int i = 0; i < count; ++i ) {
            ImGui::PushID( i );
            const int previous = *index;
            if (cfg_choice_row("##opt", options[i], mw, mh, index, i))
                changed = previous != *index; // the card stays up: pick again without reopening it
            ImGui::PopID( );
        }
        ImGui::Dummy( ImVec2( mw, 0.f ) );
        end_popup_card( );
    }
    return changed;
}

void c_gui::popup_separator( ) {
    const float s = m_scale, w = popup_row_width( );
    const ImVec2 sp = ImGui::GetCursorScreenPos( ) + ImVec2( 0.f, 6.f * s );
    ImGui::GetWindowDrawList( )->AddLine( sp, sp + ImVec2( w, 0.f ), popup_border.to_im_color( 0.55f ), 1.f * s );
    ImGui::Dummy( ImVec2( w, 13.f * s ) );
}

bool c_gui::popup_row( const char* icon, const char* label ) {
    return popup_row_impl( icon, label, false ).clicked;
}

bool c_gui::popup_option( const char* icon, const char* label, const char* const* options, int count, int* index ) {
    if ( count <= 0 )
        return false;
    *index = ImClamp( *index, 0, count - 1 );
    const std::string popup_name = std::string( "##opt_" ) + label;
    const bool up = ImGui::IsPopupOpen( ImGui::GetID( popup_name.c_str( ) ), ImGuiPopupFlags_None );
    const popup_row_state r = popup_row_impl( icon, label, up );
    popup_row_value( r, options[ *index ] );
    return popup_option_card( popup_name.c_str( ), r.clicked, r.pos.y, options, count, index );
}

bool c_gui::popup_toggle( const char* icon, const char* label, bool* v ) {
    const popup_row_state r = popup_row_impl( icon, label, false );
    if ( r.clicked )
        *v = !*v;
    const float s = m_scale;
    const ImGuiID sid = ImHashStr( "##switch", 0, ImGui::GetID( label ) );
    if ( cfg_anims( ).find( sid ) == cfg_anims( ).end( ) )
        cfg_anims( )[ sid ] = *v ? 1.f : 0.f; // first sight: show the current state, don't animate into it
    const float st = cfg_anim( sid, *v, 12.f );
    const ImVec2 sz( switch_w( ), switch_h( ) );
    const ImVec2 smin( r.pos.x + r.w - 4.f * s - sz.x, r.pos.y + IM_ROUND( ( 32.f * s - sz.y ) * 0.5f ) );
    draw_switch( ImGui::GetWindowDrawList( ), ImRect( smin, smin + sz ), st );
    return r.clicked;
}

// ---------------------------------------------------------------------------------------
//  Account block
// ---------------------------------------------------------------------------------------

// ---- Spotify player -------------------------------------------------------------------------
//
// Driven by the real Spotify desktop app through spotify.cpp (Windows' media session API, which
// works the same on Free and on Premium). Everything on the card is live: title, artist, album,
// the real cover art, the transport, shuffle / repeat and Spotify's own mixer level. With Spotify
// closed the card turns into a single "Open Spotify" call to action instead of pretending.
//
// The colour and the motif behind the artwork are derived from the track itself, so there is
// always something in the sleeve while the real cover is still being fetched.

namespace {

    struct sp_ui_t {
        float play_t = 0.f;      // 0 paused .. 1 playing - the meter, the glow and the glyph ride it
        float alive_t = 0.f;     // 0 disconnected .. 1 connected
        float change_t = 1.f;    // 0 the instant the track changed .. 1 settled
        float fill_t = 0.f;      // the progress fill, eased so a seek slides instead of jumping
        float beat = 0.f;        // free-running phase for anything that has to breathe
        bool  seeking = false;
        float lyrics_t = 0.f;      // 0 panel closed .. 1 open, and the card widens with it
        float lyrics_scroll = 0.f; // eased, so the active line glides into place
        int   lyrics_line = -1;
        float lyrics_line_t = 1.f; // 0 the instant the active line changed .. 1 settled
        std::string last_title;

        void tick( float dt, const spotify::snapshot_t& s ) {
            dt = ImClamp( dt, 0.f, 0.1f );
            play_t  = ImLerp( play_t,  s.playing ? 1.f : 0.f, 1.f - expf( -dt * 9.f ) );
            alive_t = ImLerp( alive_t, s.status == spotify::status_t::connected ? 1.f : 0.f,
                              1.f - expf( -dt * 8.f ) );
            beat += dt * ( 1.1f + 2.4f * play_t );

            if ( s.title != last_title ) {
                last_title = s.title;
                change_t = 0.f;
            }
            change_t = ImMin( 1.f, change_t + dt * 3.4f );

            const float target = s.duration > 0.0 ? ( float )ImSaturate( s.position / s.duration ) : 0.f;
            fill_t = seeking ? target : ImLerp( fill_t, target, 1.f - expf( -dt * 20.f ) );

            lyrics_line_t = ImMin( 1.f, lyrics_line_t + dt * 4.5f );
        }
    } sp_ui;

    void sp_format_time( char* buf, int size, double seconds ) {
        const int total = ( int )ImMax( 0.0, seconds );
        ImFormatString( buf, size, "%d:%02d", total / 60, total % 60 );
    }

    // The sleeve's own palette, hashed out of the track so it is stable for that track and
    // different for the next one.
    void sp_palette( const std::string& key, ImU32& a, ImU32& b, int& motif ) {
        unsigned int h = 2166136261u;
        for ( char c : key ) {
            h ^= ( unsigned char )c;
            h *= 16777619u;
        }
        const float hue = ( h & 0xffffu ) / 65535.f;
        float r = 0.f, g = 0.f, bl = 0.f;
        ImGui::ColorConvertHSVtoRGB( hue, 0.58f, 0.26f, r, g, bl );
        a = IM_COL32( ( int )( r * 255.f ), ( int )( g * 255.f ), ( int )( bl * 255.f ), 255 );
        ImGui::ColorConvertHSVtoRGB( ImFmod( hue + 0.11f, 1.f ), 0.66f, 0.82f, r, g, bl );
        b = IM_COL32( ( int )( r * 255.f ), ( int )( g * 255.f ), ( int )( bl * 255.f ), 255 );
        motif = ( int )( ( h >> 17 ) & 3u );
    }

    void sp_draw_cover( ImDrawList* draw, ImU32 col_a, ImU32 col_b, int motif, ImVec2 min, ImVec2 max,
                        float rounding, float alpha, float phase ) {
        if ( alpha <= 0.004f )
            return;

        const ImU32 white = IM_COL32( 255, 255, 255, ( int )( 255.f * alpha ) );
        const int v0 = draw->VtxBuffer.Size;
        draw->AddRectFilled( min, max, white, rounding );
        ImGui::ShadeVertsLinearColorGradientKeepAlpha( draw, v0, draw->VtxBuffer.Size,
            ImVec2( min.x, min.y ), ImVec2( max.x, max.y ), col_a, col_b );

        const ImVec2 c( ( min.x + max.x ) * 0.5f, ( min.y + max.y ) * 0.5f );
        const float  sz = max.x - min.x;
        const ImU32  ink = IM_COL32( 255, 255, 255, ( int )( 46.f * alpha ) );
        const ImU32  ink2 = IM_COL32( 0, 0, 0, ( int )( 50.f * alpha ) );

        // The motif is inset far enough that this square clip never reaches the rounded corners.
        draw->PushClipRect( min, max, true );
        switch ( motif ) {
        case 0:
            for ( int i = 1; i <= 4; ++i )
                draw->AddCircle( c, sz * ( 0.10f + i * 0.085f ), i & 1 ? ink : ink2, 40, sz * 0.022f );
            draw->AddCircleFilled( c, sz * 0.055f, ink, 24 );
            break;
        case 1:
            for ( int i = -3; i <= 5; ++i ) {
                const float x = min.x + sz * ( 0.16f * i );
                draw->AddQuadFilled( ImVec2( x, max.y ), ImVec2( x + sz * 0.07f, max.y ),
                                     ImVec2( x + sz * 0.07f + sz, min.y ), ImVec2( x + sz, min.y ),
                                     i & 1 ? ink : ink2 );
            }
            break;
        case 2:
            draw->AddCircleFilled( ImVec2( c.x + sz * 0.10f, c.y - sz * 0.06f ), sz * 0.30f, ink, 48 );
            draw->AddCircleFilled( ImVec2( c.x - sz * 0.14f, c.y + sz * 0.14f ), sz * 0.20f, ink2, 40 );
            break;
        default:
            for ( int gy = 0; gy < 5; ++gy )
                for ( int gx = 0; gx < 5; ++gx )
                    draw->AddCircleFilled( ImVec2( min.x + sz * ( 0.18f + gx * 0.16f ),
                                                   min.y + sz * ( 0.18f + gy * 0.16f ) ),
                                           sz * ( ( gx + gy ) & 1 ? 0.030f : 0.016f ),
                                           ( gx + gy ) & 1 ? ink : ink2, 16 );
            break;
        }

        const float band = sz * 0.34f, travel = min.x - band + ( sz + band * 2.f ) * ImFmod( phase, 1.f );
        draw->AddQuadFilled( ImVec2( travel, max.y ), ImVec2( travel + band, max.y ),
                             ImVec2( travel + band + sz * 0.4f, min.y ), ImVec2( travel + sz * 0.4f, min.y ),
                             IM_COL32( 255, 255, 255, ( int )( 16.f * alpha ) ) );
        draw->PopClipRect( );

        draw->AddRect( min, max, IM_COL32( 255, 255, 255, ( int )( 28.f * alpha ) ), rounding, 0, 1.f );
    }
}

void c_gui::spotify_tick( ) {
    const float dt = ImGui::GetIO( ).DeltaTime;
    spotify::tick( dt );
    sp_ui.tick( dt, spotify::state( ) );

    const spotify::snapshot_t& now = spotify::state( );
    gifbg::set_enabled( spotify_gif );
    gifbg::set_track( now.artist, now.title );
    gifbg::tick( dt );
}

void c_gui::spotify_widget_draw( ) {
    const float s = m_scale;
    ImGuiIO& io = ImGui::GetIO( );

    // Toggled, not opened: once the switch in the profile card is on, the widget stays where the
    // user parked it until they turn it off - clicking elsewhere must not dismiss it, which is
    // exactly what a popup would have done.
    static float shown = 0.f;
    static float bg_t = 0.f, bg_drift = 0.f; // the card backdrop's fade and its drift clock
    shown = ImLerp( shown, spotify_widget ? 1.f : 0.f, 1.f - expf( -ImClamp( io.DeltaTime, 0.f, 0.1f ) * 12.f ) );
    if ( !spotify_widget && shown <= 0.004f )
        return;

    // The panel wants room for 22px type, so the whole card eases wider while it is open rather
    // than wrapping every line in half.
    const float content_w = ImLerp( 268.f, 340.f, sp_ui.lyrics_t ) * s;
    const float pad = 11.f * s;

    // header 24 + sleeve 78 + progress 30 + transport 44 = 176 of advance, and the buttons' ink
    // stops 6 short of that at 170. 173 leaves the same air under them as the padding leaves over
    // the header. With lyrics the panel starts 6 below the ink and keeps 3 under itself.
    const spotify::snapshot_t& st = spotify::state( );
    const bool  connected  = st.status == spotify::status_t::connected;
    const float lyrics_h   = connected ? IM_ROUND( 164.f * s * sp_ui.lyrics_t ) : 0.f;
    const float lyrics_top = 176.f * s;
    const float total_h    = !connected        ? 145.f * s
                           : lyrics_h > 1.f    ? lyrics_top + lyrics_h + 3.f * s
                                               : 173.f * s;

    // Dragged the way the menu is dragged: the cursor sets a target and the window eases toward
    // it, which is what gives the move its trailing weight instead of snapping to the pointer.
    // ImGui still owns the grab itself - it already knows which press belongs to the window and
    // which belongs to a button on it - and this only re-times the motion it asks for.
    static bool   drag_ready = false;
    static ImVec2 drag_pos, drag_target;
    if ( drag_ready ) {
        drag_pos = ImLerp( drag_pos, drag_target, 1.f - expf( -9.f * ImMin( io.DeltaTime, 0.05f ) ) );
        if ( ImLengthSqr( drag_target - drag_pos ) < 0.25f )
            drag_pos = drag_target;
        ImGui::SetNextWindowPos( drag_pos, ImGuiCond_Always );
    }

    ImGui::SetNextWindowPos( ImVec2( io.DisplaySize.x - content_w - 90.f * s, 130.f * s ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( content_w + pad * 2.f, total_h + pad * 2.f ), ImGuiCond_Always );

    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( pad, pad ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 16.f * s );
    ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImMax( shown, 0.02f ) );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );

    // NoBackground is hijacked in imgui.cpp to mean "paint the menu chrome" for any non-popup
    // window, so the background is cleared through the style colour instead and the surface is
    // drawn by hand below. AlwaysAutoResize with a pinned width lets the two states (playing /
    // Spotify closed) size themselves.
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing;

    const bool visible = ImGui::Begin( "##spotify_widget", nullptr, flags );
    if ( !visible ) {
        ImGui::End( );
        ImGui::PopStyleColor( );
        ImGui::PopStyleVar( 4 );
        return;
    }

    ImGuiWindow* self = ImGui::GetCurrentWindow( );
    ImDrawList* draw = self->DrawList;

    if ( !drag_ready ) {
        drag_pos = drag_target = self->Pos; // whatever the ini restored, or the first-use default
        drag_ready = true;
    }

    // While ImGui is moving this window, take the position it wants as the target and let the ease
    // above carry the window there. ActiveIdClickOffset is where inside the card the grab landed.
    if ( GImGui->MovingWindow == self ) {
        drag_target = io.MousePos - GImGui->ActiveIdClickOffset;
        drag_target.x = ImClamp( drag_target.x, -self->Size.x + 60.f * s, io.DisplaySize.x - 60.f * s );
        drag_target.y = ImClamp( drag_target.y, 0.f, io.DisplaySize.y - 32.f * s );
    }

    // Its own surface: heavier than the popup cards' 0.70, where the menu's own group box edges
    // read straight through the blur as horizontal bands across the player. No border ring either,
    // because a widget that lives on top of everything should not look like a dialog.
    {
        const ImVec2 wmin = self->Pos, wmax = self->Pos + self->Size;
        const float radius = 16.f * s;
        const float alpha = ImGui::GetStyle( ).Alpha;
        popup_shadow( draw, wmin, wmax, radius, alpha );
        draw->PushClipRect( wmin - ImVec2( 2.f, 2.f ) * s, wmax + ImVec2( 2.f, 2.f ) * s, false );
        draw_blur_rounded( draw, wmin, wmax, alpha, radius,
            ImColor( popup_bg.r, popup_bg.g, popup_bg.b, 0.80f ) );
        shader_bg::draw_into( draw, wmin, wmax, radius, alpha );

        // The backdrop: the track's GIF when one is sitting in spotify_gif\, and the cover itself
        // when there is none, so the switch always answers with something that belongs to the song
        // instead of nothing or a stray file. Scrimmed back either way so the type stays legible.
        void* const  gif = gifbg::view( );
        void* const  art = ( !gif && connected ) ? spotify::art_view( ) : nullptr;
        const float  dts = ImClamp( io.DeltaTime, 0.f, 0.1f );
        bg_t = ImLerp( bg_t, ( spotify_gif && ( gif || art ) ) ? 1.f : 0.f, 1.f - expf( -dts * 9.f ) );
        bg_drift = ImFmod( bg_drift + dts, 1024.f );
        if ( bg_t > 0.004f && ( gif || art ) ) {
            // Cover-crop: keep the source's long axis and trim the short one, never letterbox.
            const float cw = ImMax( wmax.x - wmin.x, 1.f ), chh = ImMax( wmax.y - wmin.y, 1.f );
            const float aspect = cw / chh;
            float uvw = 1.f, uvh = 1.f;
            if ( aspect >= 1.f ) uvh = ImMin( 1.f, 1.f / aspect );
            else                 uvw = ImMin( 1.f, aspect );

            if ( gif ) {
                draw->AddImageRounded( ( ImTextureID )gif, wmin, wmax,
                    ImVec2( 0.5f - uvw * 0.5f, 0.5f - uvh * 0.5f ),
                    ImVec2( 0.5f + uvw * 0.5f, 0.5f + uvh * 0.5f ),
                    IM_COL32( 255, 255, 255, ( int )( 190.f * alpha * bg_t ) ), radius );
            }
            else {
                // A sleeve blown straight up over the card would read as a thumbnail behind the
                // type, so it is over-zoomed and smeared across seventeen uv taps - the bilinear
                // upscale does half the work - then drifted so the wash breathes. A real gaussian
                // pass would cost a second render target for something the scrim swallows anyway.
                const float zoom = 1.58f + 0.06f * ImSin( bg_drift * 0.32f );
                uvw /= zoom; uvh /= zoom;

                // Two rings and a centre, the centre last so it carries the most weight: the taps
                // composite rather than average, so each one's alpha is solved back out of the
                // total the stack should reach.
                static const ImVec2 taps[ ] = {
                    ImVec2( -2.f, 0.f ), ImVec2( 2.f, 0.f ), ImVec2( 0.f, -2.f ), ImVec2( 0.f, 2.f ),
                    ImVec2( -1.4f, -1.4f ), ImVec2( 1.4f, -1.4f ), ImVec2( -1.4f, 1.4f ), ImVec2( 1.4f, 1.4f ),
                    ImVec2( -1.f, 0.f ), ImVec2( 1.f, 0.f ), ImVec2( 0.f, -1.f ), ImVec2( 0.f, 1.f ),
                    ImVec2( -0.7f, -0.7f ), ImVec2( 0.7f, -0.7f ), ImVec2( -0.7f, 0.7f ), ImVec2( 0.7f, 0.7f ),
                    ImVec2( 0.f, 0.f ) };
                const float spread = 0.026f;

                // The sampler wraps, so the drift has to stay inside the slack the zoom left, taps
                // included - one uv past the edge and the far side of the sleeve folds into frame.
                const float mx = ImMin( uvw * 0.5f + spread * 2.f, 0.5f );
                const float my = ImMin( uvh * 0.5f + spread * 2.f, 0.5f );
                const float cx = ImClamp( 0.5f + ( 0.5f - uvw * 0.5f ) * 0.55f * ImSin( bg_drift * 0.21f ), mx, 1.f - mx );
                const float cy = ImClamp( 0.5f + ( 0.5f - uvh * 0.5f ) * 0.55f * ImCos( bg_drift * 0.17f ), my, 1.f - my );

                const float total = ImClamp( 0.86f * alpha * bg_t, 0.f, 0.995f );
                const int   ink   = ( int )( ( 1.f - ImPow( 1.f - total, 1.f / ( float )IM_ARRAYSIZE( taps ) ) ) * 255.f + 0.5f );
                if ( ink > 0 ) {
                    for ( const ImVec2& t : taps ) {
                        const float ox = t.x * spread, oy = t.y * spread;
                        draw->AddImageRounded( ( ImTextureID )art, wmin, wmax,
                            ImVec2( cx - uvw * 0.5f + ox, cy - uvh * 0.5f + oy ),
                            ImVec2( cx + uvw * 0.5f + ox, cy + uvh * 0.5f + oy ),
                            IM_COL32( 255, 255, 255, ink ), radius );
                    }
                }
            }
            draw->AddRectFilled( wmin, wmax,
                IM_COL32( 6, 8, 14, ( int )( ( gif ? 150.f : 172.f ) * alpha * bg_t ) ), radius );
        }
        draw->PopClipRect( );
    }
    ImFontAtlas* atlas = io.Fonts;
    ImFont* bold  = atlas->Fonts[ 4 ]; // Inter-SemiBold 17
    ImFont* micro = atlas->Fonts[ 6 ]; // Inter-SemiBold 12.5
    ImFont* small = small_font( );     // Inter-Medium 13 + FA 13
    ImFont* body  = atlas->Fonts[ 0 ]; // Inter-Medium 15 + FA 14 - every icon on this card

    const float ca = ImGui::GetStyle( ).Alpha;
    const float w = content_w;
    const ImVec2 origin = ImGui::GetCursorScreenPos( );

    const ImVec4 c_text = text.to_vec4( 1.f, false );
    const ImVec4 c_dim  = text_disabled.to_vec4( 1.f, false );
    const ImVec4 c_acc  = accent_color.to_vec4( 1.f, false );

    const float ease = sp_ui.change_t * sp_ui.change_t * ( 3.f - 2.f * sp_ui.change_t );

    // A local button: an invisible hit box, an animated plate, and a glyph centred on its own ink.
    const auto icon_button = [ & ]( const char* id, ImVec2 centre, float box, const char* glyph,
                                    ImFont* font, float fs, float lit, float scale_hot, bool enabled ) -> bool {
        const ImVec2 half( box * 0.5f, box * 0.5f );
        ImGui::SetCursorScreenPos( centre - half );
        const bool pressed = ImGui::InvisibleButton( id, ImVec2( box, box ) ) && enabled;
        const bool hovered = enabled && ImGui::IsItemHovered( );
        const bool held    = enabled && ImGui::IsItemActive( );
        const float t = cfg_anim( ImGui::GetID( id ), hovered );
        const float k = 1.f + scale_hot * t - ( held ? 0.08f : 0.f );

        if ( t > 0.01f )
            draw->AddCircleFilled( centre, box * 0.5f * k, frame_inactive.to_im_color( 0.55f * t ), 28 );

        const ImVec2 sz = font->CalcTextSizeA( fs, FLT_MAX, 0.f, glyph );
        ImU32 col = cfg_mix( c_dim, lit > 0.5f ? c_acc : c_text, ImMax( t, lit ) );
        if ( !enabled )
            col = text_disabled.to_im_color( 0.45f );
        draw->AddText( font, fs, ImVec2( IM_ROUND( centre.x - sz.x * 0.5f ),
                                         IM_ROUND( centre.y - TextInkCenterY( font, fs, glyph ) ) ), col, glyph );
        return pressed;
    };

    bool opts_clicked = false;
    float y = origin.y;

    // ---- header -------------------------------------------------------------------------------
    {
        const float fs = micro->FontSize;
        const ImVec2 icon_sz = body->CalcTextSizeA( body->FontSize, FLT_MAX, 0.f, ICON_FA_MUSIC );
        draw->AddText( body, body->FontSize, ImVec2( IM_ROUND( origin.x ), IM_ROUND( y + 2.f * s ) ),
            accent_color.to_im_color( ), ICON_FA_MUSIC );
        draw->AddText( micro, fs, ImVec2( IM_ROUND( origin.x + icon_sz.x + 7.f * s ), IM_ROUND( y + 4.f * s ) ),
            text.to_im_color( 0.85f ), tr( "Spotify" ) );

        // Everything that is not a transport control lives behind the dots, so the face of the
        // player stays down to the cover, the bar and the five buttons.
        const ImVec2 dc( origin.x + w - 10.f * s, IM_ROUND( y + 8.f * s ) );
        const ImVec2 half( 11.f * s, 9.f * s );
        ImGui::SetCursorScreenPos( dc - half );
        opts_clicked = ImGui::InvisibleButton( "##sp_opts_btn", half * 2.f );
        const bool opts_up = ImGui::IsPopupOpen( ImGui::GetID( "##sp_opts" ), ImGuiPopupFlags_None );
        const float dt_ = cfg_anim( ImGui::GetID( "##sp_opts_btn" ), ImGui::IsItemHovered( ) || opts_up );
        if ( dt_ > 0.01f )
            draw->AddRectFilled( dc - half, dc + half, frame_inactive.to_im_color( 0.85f * dt_ ), 6.f * s );
        const ImU32 dots_col = cfg_mix( c_dim, c_text, ImMax( dt_, opts_up ? 1.f : 0.f ) );
        for ( int i = -1; i <= 1; ++i )
            draw->AddCircleFilled( ImVec2( IM_ROUND( dc.x + i * 4.f * s ), IM_ROUND( dc.y ) ), 1.4f * s, dots_col, 12 );
    }
    y += 24.f * s;

    // Submitted before the offline branch returns, so the card survives Spotify closing under it.
    {
        bool wants = false;
        const float a = popup_animation( "##sp_opts", opts_clicked, wants );
        if ( begin_popup_card_at( "##sp_opts", wants, a,
                                  ImVec2( self->Pos.x + self->Size.x + 8.f * s, self->Pos.y ),
                                  ImVec2( 0.f, 0.f ), 272.f ) ) {
            ImGui::PushItemWidth( ImGui::GetContentRegionAvail( ).x );

            // A row that cannot do anything goes dark, and eases its colour back when it can -
            // rather than popping. Each gate is scoped to exactly the rows it governs.
            const auto gate_begin = [ & ]( const char* id, bool on ) {
                const float lit = cfg_anim( ImGui::GetID( id ), on, 10.f );
                ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle( ).Alpha * ImLerp( 0.38f, 1.f, lit ) );
                ImGui::BeginDisabled( !on );
            };
            const auto gate_end = [ ]( ) {
                ImGui::EndDisabled( );
                ImGui::PopStyleVar( );
            };

            static float muted_at = 0.70f;
            const bool was_muted = st.has_volume && st.volume <= 0.001f;
            bool muted = was_muted;
            gate_begin( "##sp_vol_lit", st.has_volume );
            if ( popup_toggle( ICON_FA_VOLUME_MUTE, "Mute", &muted ) && muted != was_muted ) {
                if ( muted ) { muted_at = ImMax( st.volume, 0.05f ); spotify::set_volume( 0.f ); }
                else         { spotify::set_volume( muted_at ); }
            }
            gate_end( );

            popup_separator( );
            {
                const bool was = spotify_lyrics;
                popup_toggle( ICON_FA_MICROPHONE_ALT, "Show Lyrics", &spotify_lyrics );
                if ( spotify_lyrics != was )
                    spotify::set_lyrics_enabled( spotify_lyrics );
            }
            {
                static const char* const bg_modes[] = { "With Lyrics Background", "Without Lyrics Background" };
                gate_begin( "##sp_bg_lit", spotify_lyrics );
                popup_option( ICON_FA_IMAGE, "Background", bg_modes, IM_ARRAYSIZE( bg_modes ), &spotify_lyrics_bg );
                gate_end( );
            }

            {
                const bool was = spotify_gif;
                popup_toggle( ICON_FA_FILM, "Preview GIF", &spotify_gif );
                if ( spotify_gif != was )
                    gifbg::set_enabled( spotify_gif );
                info_tooltip( "Preview GIF",
                    "Plays a GIF behind the player. Drop files in spotify_gif\\ next to the "
                    "executable, named \"<artist> - <title>.gif\", \"<title>.gif\" or \"default.gif\". "
                    "Without a matching file the cover art is used instead." );
            }

            popup_separator( );
            {
                // Spotify's own channel in the Windows mixer - the media session API carries no
                // volume of its own. Indented by the rows' icon column so the label starts on the
                // same x as theirs, and the track ends on the same x as their switches.
                const float row_w = popup_row_width( ); // what the rows above actually span
                const float indent = 36.f * s;
                gate_begin( "##sp_vols_lit", st.has_volume );
                ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + indent );
                ImGui::PushItemWidth( ImMax( 1.f, row_w - indent ) );
                int vol = ( int )IM_ROUND( ( st.has_volume ? st.volume : 1.f ) * 100.f );
                if ( ImGui::SliderInt( "Volume", &vol, 0, 100, "%d%%" ) )
                    spotify::set_volume( vol / 100.f );
                ImGui::PopItemWidth( );
                gate_end( );
            }
            ImGui::PopItemWidth( );
            end_popup_card( );
        }
    }

    // ---- nothing to drive: one clear call to action ------------------------------------------
    if ( !connected ) {
        const float panel_h = 118.f * s;

        const char* head = st.status == spotify::status_t::not_running ? tr( "Spotify is not running" )
                         : st.status == spotify::status_t::idle        ? tr( "Nothing is playing" )
                                                                       : tr( "Media controls unavailable" );
        const char* sub  = st.status == spotify::status_t::not_running ? tr( "Start it and this connects on its own." )
                         : st.status == spotify::status_t::idle        ? tr( "Play something in Spotify to take over." )
                                                                       : tr( "This build of Windows exposes no media session." );

        // a slow accent breath behind the glyph, so the empty state is not a dead rectangle
        const float breath = 0.5f + 0.5f * sinf( sp_ui.beat * 1.4f );
        const ImVec2 gc( origin.x + w * 0.5f, y + 24.f * s );
        for ( int i = 3; i >= 1; --i )
            draw->AddCircle( gc, ( 13.f + i * 5.f + breath * 2.f ) * s,
                accent_color.to_im_color( 0.13f / i ), 40, 1.5f * s );

        const float gfs = body->FontSize; // baked size, not 1.5x of a 13px face
        const ImVec2 gsz = body->CalcTextSizeA( gfs, FLT_MAX, 0.f, ICON_FA_MUSIC );
        draw->AddText( body, gfs, ImVec2( IM_ROUND( gc.x - gsz.x * 0.5f ),
                                          IM_ROUND( gc.y - TextInkCenterY( body, gfs, ICON_FA_MUSIC ) ) ),
            accent_color.to_im_color( 0.85f ), ICON_FA_MUSIC );

        const ImVec2 hsz = bold->CalcTextSizeA( bold->FontSize, FLT_MAX, 0.f, head );
        draw->AddText( bold, bold->FontSize, ImVec2( IM_ROUND( origin.x + ( w - hsz.x ) * 0.5f ),
                                                     IM_ROUND( y + 46.f * s ) ), text.to_im_color( ), head );

        const ImVec2 ssz = micro->CalcTextSizeA( micro->FontSize, FLT_MAX, 0.f, sub );
        draw->AddText( micro, micro->FontSize, ImVec2( IM_ROUND( origin.x + ( w - ssz.x ) * 0.5f ),
                                                       IM_ROUND( y + 68.f * s ) ), text_disabled.to_im_color( ), sub );

        if ( st.status != spotify::status_t::unsupported ) {
            const char* label = st.status == spotify::status_t::idle ? tr( "Open Spotify" ) : tr( "Open Spotify" );
            const float bh = 30.f * s;
            const float bw = ImMin( w, micro->CalcTextSizeA( micro->FontSize, FLT_MAX, 0.f, label ).x + 52.f * s );
            const ImVec2 bmin( IM_ROUND( origin.x + ( w - bw ) * 0.5f ), IM_ROUND( y + 88.f * s ) );

            ImGui::SetCursorScreenPos( bmin );
            const bool pressed = ImGui::InvisibleButton( "##sp_open", ImVec2( bw, bh ) );
            const float t = cfg_anim( ImGui::GetID( "##sp_open" ), ImGui::IsItemHovered( ) );
            const float press = ImGui::IsItemActive( ) ? 1.f : 0.f;

            const ImVec2 grow( 2.f * s * t - 1.f * s * press, 1.f * s * t - 0.5f * s * press );
            draw->AddRectFilled( bmin - grow, bmin + ImVec2( bw, bh ) + grow,
                accent_color.to_im_color( 0.85f + 0.15f * t ), ( bh * 0.5f ) );
            if ( t > 0.01f )
                draw->AddRect( bmin - grow - ImVec2( 3.f, 3.f ) * s, bmin + ImVec2( bw, bh ) + grow + ImVec2( 3.f, 3.f ) * s,
                    accent_color.to_im_color( 0.30f * t ), bh * 0.5f + 3.f * s, 0, 1.5f * s );

            const ImVec2 lsz = micro->CalcTextSizeA( micro->FontSize, FLT_MAX, 0.f, label );
            const ImVec2 isz = body->CalcTextSizeA( body->FontSize, FLT_MAX, 0.f, ICON_FA_EXTERNAL_LINK_ALT );
            const float block = isz.x + 7.f * s + lsz.x;
            const float bx = bmin.x + ( bw - block ) * 0.5f;
            const float by = bmin.y + bh * 0.5f;
            draw->AddText( body, body->FontSize, ImVec2( IM_ROUND( bx ),
                IM_ROUND( by - TextInkCenterY( body, body->FontSize, ICON_FA_EXTERNAL_LINK_ALT ) ) ),
                IM_COL32( 255, 255, 255, ( int )( 255.f * ca ) ), ICON_FA_EXTERNAL_LINK_ALT );
            draw->AddText( micro, micro->FontSize, ImVec2( IM_ROUND( bx + isz.x + 7.f * s ),
                IM_ROUND( by - TextLineCenterY( micro, micro->FontSize ) ) ),
                IM_COL32( 255, 255, 255, ( int )( 255.f * ca ) ), label );

            if ( pressed )
                spotify::launch( );
        }

        ImGui::SetCursorScreenPos( ImVec2( origin.x, y + panel_h ) );
        ImGui::End( );
        ImGui::PopStyleColor( );
        ImGui::PopStyleVar( 4 );
        return;
    }

    // ---- connected ---------------------------------------------------------------------------
    const float art_sz = 64.f * s;

    ImU32 pal_a = 0, pal_b = 0;
    int   motif = 0;
    sp_palette( st.title + st.album, pal_a, pal_b, motif );

    // ---- cover + track ---------------------------------------------------------------------
    {
        const ImVec2 art_min( origin.x, y );
        const ImVec2 art_max = art_min + ImVec2( art_sz, art_sz );
        const float  art_round = 10.f * s;

        // The glow behind the sleeve is the only thing that moves on its own while a track plays -
        // it is what makes a paused player read as paused at a glance.
        const float glow = ( 0.35f + 0.65f * ( 0.5f + 0.5f * sinf( sp_ui.beat * 2.2f ) ) ) * sp_ui.play_t;
        for ( int i = 3; i >= 1; --i )
            draw->AddRect( art_min - ImVec2( i * 2.f, i * 2.f ) * s, art_max + ImVec2( i * 2.f, i * 2.f ) * s,
                accent_color.to_im_color( 0.10f * glow / i ), art_round + i * 2.f * s, 0, 2.f * s );

        // the hashed sleeve underneath, the real cover fading in over it on every track change
        sp_draw_cover( draw, pal_a, pal_b, motif, art_min, art_max, art_round, ca, sp_ui.beat * 0.11f );
        if ( void* art = spotify::art_view( ) )
            draw->AddImageRounded( ( ImTextureID )art, art_min, art_max, ImVec2( 0.f, 0.f ), ImVec2( 1.f, 1.f ),
                IM_COL32( 255, 255, 255, ( int )( 255.f * ca * ease ) ), art_round );

        // ---- title / artist / album ----
        const float tx = origin.x + art_sz + 14.f * s;
        const float link_w = 22.f * s;
        const float tw = origin.x + w - tx - link_w;
        const float slide = ( 1.f - ease ) * 10.f * s;

        const char* title  = st.title.empty( )  ? tr( "Unknown track" ) : st.title.c_str( );
        const char* artist = st.artist.empty( ) ? tr( "Unknown artist" ) : st.artist.c_str( );

        draw->PushClipRect( ImVec2( tx, y ), ImVec2( tx + tw, y + art_sz ), true );
        {
            // Marquee with a hold at each end, so a long title is readable instead of always moving.
            const float fs = bold->FontSize;
            const ImVec2 sz = bold->CalcTextSizeA( fs, FLT_MAX, 0.f, title );
            float x = tx;
            if ( sz.x > tw ) {
                const float u = ImFmod( ( float )ImGui::GetTime( ) * 0.16f, 1.f );
                float k = ImSaturate( ( u - 0.15f ) / 0.32f ) - ImSaturate( ( u - 0.66f ) / 0.32f );
                k = k * k * ( 3.f - 2.f * k );
                x -= ( sz.x - tw ) * k;
            }
            draw->AddText( bold, fs, ImVec2( IM_ROUND( x ), IM_ROUND( y + 4.f * s + slide ) ),
                text.to_im_color( ease ), title );
        }
        draw->AddText( small, small->FontSize, ImVec2( IM_ROUND( tx ), IM_ROUND( y + 26.f * s + slide * 1.4f ) ),
            cfg_mix( c_dim, c_text, 0.55f * ease ), artist );
        if ( !st.album.empty( ) )
            draw->AddText( micro, micro->FontSize, ImVec2( IM_ROUND( tx ), IM_ROUND( y + 44.f * s + slide * 1.8f ) ),
                text_disabled.to_im_color( 0.75f * ease ), st.album.c_str( ) );
        draw->PopClipRect( );

        if ( icon_button( "##sp_focus", ImVec2( origin.x + w - 10.f * s, y + 12.f * s ), 24.f * s,
                          ICON_FA_EXTERNAL_LINK_ALT, body, body->FontSize, 0.f, 0.10f, true ) )
            spotify::launch( ); // already running, so this brings its window forward
    }
    y += art_sz + 14.f * s;


    // ---- progress ----------------------------------------------------------------------------
    {
        const float bar_h = 4.f * s;
        const ImVec2 bar_min( origin.x, IM_ROUND( y + 4.f * s ) );
        const float  bar_w = w;

        ImGui::SetCursorScreenPos( ImVec2( bar_min.x, bar_min.y - 9.f * s ) );
        ImGui::InvisibleButton( "##sp_seek", ImVec2( bar_w, bar_h + 18.f * s ) );
        const bool bar_hot = st.can_seek && ( ImGui::IsItemHovered( ) || ImGui::IsItemActive( ) );
        const bool active  = st.can_seek && ImGui::IsItemActive( );

        // Committed on release, not per frame: a seek is an IPC round trip to Spotify and firing
        // one every frame of a drag makes the app stutter. spotify::seek() carries the local
        // position with it, so the bar stays where it was dropped instead of snapping back for the
        // poll or two it takes the app to report the new one.
        static float drag_t = 0.f;
        if ( active && bar_w > 0.f )
            drag_t = ImSaturate( ( io.MousePos.x - bar_min.x ) / bar_w );
        if ( sp_ui.seeking && !active && st.duration > 0.0 )
            spotify::seek( drag_t * st.duration );
        sp_ui.seeking = active;
        if ( active )
            sp_ui.fill_t = drag_t;

        const float hot = cfg_anim( ImGui::GetID( "##sp_seek" ), bar_hot );
        const float grow = bar_h * ( 1.f + 0.55f * hot );
        const ImVec2 bmin( bar_min.x, IM_ROUND( bar_min.y - ( grow - bar_h ) * 0.5f ) );
        const ImVec2 bmax( bar_min.x + bar_w, bmin.y + grow );

        draw->AddRectFilled( bmin, bmax, frame_inactive.to_im_color( 0.9f ), grow * 0.5f );
        const float fx = bmin.x + bar_w * sp_ui.fill_t;
        if ( fx > bmin.x + 0.5f ) {
            draw->AddRectFilled( bmin, ImVec2( fx, bmax.y ), accent_color.to_im_color( ), grow * 0.5f );
            const float sweep = ImFmod( sp_ui.beat * 0.35f, 1.f ) * ( fx - bmin.x );
            draw->PushClipRect( bmin, ImVec2( fx, bmax.y ), true );
            draw->AddRectFilled( ImVec2( bmin.x + sweep - 18.f * s, bmin.y ), ImVec2( bmin.x + sweep + 18.f * s, bmax.y ),
                IM_COL32( 255, 255, 255, ( int )( 34.f * ca * sp_ui.play_t ) ), grow * 0.5f );
            draw->PopClipRect( );
        }
        if ( hot > 0.01f )
            draw->AddCircleFilled( ImVec2( fx, ( bmin.y + bmax.y ) * 0.5f ), 5.f * s * hot, text.to_im_color( ), 24 );

        char a[ 16 ], b[ 16 ];
        sp_format_time( a, IM_ARRAYSIZE( a ), active ? drag_t * st.duration : st.position );
        sp_format_time( b, IM_ARRAYSIZE( b ), st.duration );
        const float fs = micro->FontSize;
        const ImVec2 bsz = micro->CalcTextSizeA( fs, FLT_MAX, 0.f, b );
        const float ty = IM_ROUND( bmax.y + 7.f * s );
        draw->AddText( micro, fs, ImVec2( IM_ROUND( origin.x ), ty ), text_disabled.to_im_color( ), a );
        draw->AddText( micro, fs, ImVec2( IM_ROUND( origin.x + w - bsz.x ), ty ), text_disabled.to_im_color( ), b );
    }
    y += 30.f * s;

    // ---- transport ---------------------------------------------------------------------------
    {
        const float cy = y + 20.f * s;
        const float cx = origin.x + w * 0.5f;
        const float gap = 36.f * s;

        if ( icon_button( "##sp_shuffle", ImVec2( cx - gap * 2.f, cy ), 24.f * s, ICON_FA_RANDOM,
                          body, body->FontSize, st.shuffle ? 1.f : 0.f, 0.12f, st.can_shuffle ) )
            spotify::set_shuffle( !st.shuffle );

        if ( icon_button( "##sp_prev", ImVec2( cx - gap, cy ), 27.f * s, ICON_FA_STEP_BACKWARD,
                          body, body->FontSize, 0.f, 0.12f, st.can_previous ) )
            spotify::previous( );

        // play / pause: the accent disc, with the two glyphs cross-fading through play_t
        {
            const float r = 18.f * s;
            ImGui::SetCursorScreenPos( ImVec2( cx - r, cy - r ) );
            const bool pressed = ImGui::InvisibleButton( "##sp_play", ImVec2( r * 2.f, r * 2.f ) );
            const bool hovered = ImGui::IsItemHovered( );
            const bool held = ImGui::IsItemActive( );
            const float t = cfg_anim( ImGui::GetID( "##sp_play" ), hovered );
            const float k = 1.f + 0.07f * t - ( held ? 0.06f : 0.f );

            draw->AddCircleFilled( ImVec2( cx, cy ), r * k * 1.28f, accent_color.to_im_color( 0.16f * t ), 40 );
            draw->AddCircleFilled( ImVec2( cx, cy ), r * k, accent_color.to_im_color( ), 48 );

            const float fs = body->FontSize; // the disc scales on hover, the glyph never does
            const auto glyph = [ & ]( const char* g, float a, float dx ) {
                if ( a <= 0.01f ) return;
                const ImVec2 sz = body->CalcTextSizeA( fs, FLT_MAX, 0.f, g );
                draw->AddText( body, fs, ImVec2( IM_ROUND( cx - sz.x * 0.5f + dx ),
                                                 IM_ROUND( cy - TextInkCenterY( body, fs, g ) ) ),
                    IM_COL32( 255, 255, 255, ( int )( 255.f * a * ca ) ), g );
            };
            glyph( ICON_FA_PLAY,  1.f - sp_ui.play_t, 1.f * s ); // the triangle's ink sits left of centre
            glyph( ICON_FA_PAUSE, sp_ui.play_t,       0.f );

            if ( pressed && st.can_play_pause )
                spotify::toggle_play( );
        }

        if ( icon_button( "##sp_next", ImVec2( cx + gap, cy ), 27.f * s, ICON_FA_STEP_FORWARD,
                          body, body->FontSize, 0.f, 0.12f, st.can_next ) )
            spotify::next( );

        if ( icon_button( "##sp_repeat", ImVec2( cx + gap * 2.f, cy ), 24.f * s,
                          st.repeat == spotify::repeat_t::one ? ICON_FA_REDO_ALT : ICON_FA_REDO,
                          body, body->FontSize, st.repeat != spotify::repeat_t::off ? 1.f : 0.f,
                          0.12f, st.can_repeat ) )
            spotify::cycle_repeat( );

        if ( st.repeat == spotify::repeat_t::one )
            draw->AddCircleFilled( ImVec2( cx + gap * 2.f, cy + 10.f * s ), 1.6f * s, accent_color.to_im_color( ), 12 );
    }
    y += 44.f * s;

    // ---- lyrics -------------------------------------------------------------------------------
    {
        const spotify::lyrics_t& ly = spotify::lyrics( );
        const bool want = spotify_lyrics && ly.state != spotify::lyrics_state_t::off;
        sp_ui.lyrics_t = ImLerp( sp_ui.lyrics_t, want ? 1.f : 0.f,
                                 1.f - expf( -ImClamp( io.DeltaTime, 0.f, 0.1f ) * 10.f ) );

        const float panel_h = lyrics_h;
        if ( panel_h > 1.f ) {
            const ImVec2 pmin( origin.x, IM_ROUND( origin.y + lyrics_top ) );
            const ImVec2 pmax( origin.x + w, pmin.y + panel_h );
            const float  pr = 12.f * s;

            // "With Lyrics Background": the sleeve itself, blown up well past its own resolution
            // and scrimmed. The magnification is the blur - there is no second pass to pay for.
            draw->AddRectFilled( pmin, pmax, frame_inactive.to_im_color( 0.55f ), pr );

            // The whole card is already wearing the backdrop, which runs behind this panel too, so
            // the panel gives up its own copy as that fades in - a second crop of the same image
            // inside the first is what turns a wash into noise - and pays for the loss with a
            // deeper scrim, which is all the lyrics needed from it.
            const float own_bg = 1.f - ImClamp( bg_t, 0.f, 1.f );
            if ( spotify_lyrics_bg == 0 && own_bg > 0.004f ) {
                if ( void* art = spotify::art_view( ) ) {
                    draw->PushClipRect( pmin, pmax, true );
                    // Cover-crop: the sleeve is square and the panel is wide, so it keeps the full
                    // width and gives up the top and bottom of the image.
                    const float uvh = ImMin( 1.f, panel_h / ImMax( pmax.x - pmin.x, 1.f ) );
                    const ImVec2 uv0( 0.f, 0.5f - uvh * 0.5f );
                    const ImVec2 uv1( 1.f, 0.5f + uvh * 0.5f );
                    draw->AddImageRounded( ( ImTextureID )art, pmin, pmax, uv0, uv1,
                        IM_COL32( 255, 255, 255, ( int )( 125.f * ca * own_bg ) ), pr );
                    draw->AddRectFilled( pmin, pmax, IM_COL32( 6, 8, 14, ( int )( 200.f * ca * own_bg ) ), pr );
                    draw->PopClipRect( );
                }
            }
            if ( bg_t > 0.004f )
                draw->AddRectFilled( pmin, pmax, IM_COL32( 6, 8, 14, ( int )( 96.f * ca * bg_t ) ), pr );

            ImFont* big = atlas->Fonts.Size > 8 ? atlas->Fonts[ 8 ] : bold;
            ImFont* lil = atlas->Fonts.Size > 9 ? atlas->Fonts[ 9 ] : small;
            const float inset = 14.f * s;
            const float wrap = w - inset * 2.f;

            draw->PushClipRect( pmin, pmax, true );
            if ( ly.state != spotify::lyrics_state_t::ready || ly.lines.empty( ) ) {
                const char* note = ly.state == spotify::lyrics_state_t::loading ? tr( "Loading lyrics" )
                                                                               : tr( "No lyrics for this track" );
                const ImVec2 nsz = lil->CalcTextSizeA( lil->FontSize, FLT_MAX, 0.f, note );
                draw->AddText( lil, lil->FontSize,
                    ImVec2( IM_ROUND( pmin.x + ( w - nsz.x ) * 0.5f ),
                            IM_ROUND( ( pmin.y + pmax.y ) * 0.5f - lil->FontSize * 0.5f ) ),
                    text_disabled.to_im_color( ), note );
            }
            else {
                const int active = ly.synced ? spotify::lyrics_index( st.position ) : -1;
                if ( active != sp_ui.lyrics_line ) {
                    sp_ui.lyrics_line = active;
                    sp_ui.lyrics_line_t = 0.f;
                }

                // One pass to lay the block out, a second to draw the part of it that is on screen.
                const int count = ( int )ly.lines.size( );
                float cursor = 0.f, active_y = 0.f, active_h = 0.f, total_lines_h = 0.f;
                for ( int i = 0; i < count; ++i ) {
                    ImFont* f = ( i == active ) ? big : lil;
                    const float lh = ly.lines[ i ].text.empty( )
                        ? f->FontSize * 0.55f
                        : f->CalcTextSizeA( f->FontSize, FLT_MAX, wrap, ly.lines[ i ].text.c_str( ) ).y;
                    if ( i == active ) { active_y = cursor; active_h = lh; }
                    cursor += lh + 10.f * s;
                }
                total_lines_h = cursor;

                float target = 0.f;
                if ( active >= 0 ) {
                    target = active_y + active_h * 0.5f - panel_h * 0.42f;
                }
                else if ( !ly.synced && st.duration > 0.0 ) {
                    // Nothing to sync to, so it drifts with the track instead of sitting still.
                    target = ( float )( st.position / st.duration ) * ImMax( 0.f, total_lines_h - panel_h * 0.6f );
                }
                target = ImClamp( target, -panel_h * 0.25f, ImMax( 0.f, total_lines_h - panel_h * 0.5f ) );
                sp_ui.lyrics_scroll = ImLerp( sp_ui.lyrics_scroll, target,
                                              1.f - expf( -ImClamp( io.DeltaTime, 0.f, 0.1f ) * 9.f ) );

                cursor = 0.f;
                for ( int i = 0; i < count; ++i ) {
                    ImFont* f = ( i == active ) ? big : lil;
                    const char* txt = ly.lines[ i ].text.c_str( );
                    const float lh = ly.lines[ i ].text.empty( )
                        ? f->FontSize * 0.55f
                        : f->CalcTextSizeA( f->FontSize, FLT_MAX, wrap, txt ).y;

                    const float ly_top = pmin.y + 10.f * s + cursor - sp_ui.lyrics_scroll;
                    cursor += lh + 10.f * s;
                    if ( ly_top > pmax.y || ly_top + lh < pmin.y || ly.lines[ i ].text.empty( ) )
                        continue;

                    // Read: bright on the line being sung, falling away above and below it, and
                    // fading again at the panel's own edges so nothing is cut mid-glyph.
                    float a;
                    if ( i == active ) {
                        a = 1.f;
                    }
                    else if ( active < 0 ) {
                        a = 0.62f;
                    }
                    else {
                        const int d = i - active;
                        a = d < 0 ? 0.26f : ImMax( 0.30f, 0.62f - 0.10f * ( d - 1 ) );
                    }
                    const float mid = ly_top + lh * 0.5f;
                    a *= ImSaturate( ( mid - pmin.y ) / ( 26.f * s ) ) * ImSaturate( ( pmax.y - mid ) / ( 26.f * s ) );
                    if ( a <= 0.01f )
                        continue;

                    const ImU32 col = ( i == active ) ? accent_color.to_im_color( a )
                                                      : text.to_im_color( a * 0.9f );

                    const float slide = ( i == active ) ? ( 1.f - sp_ui.lyrics_line_t ) * 5.f * s : 0.f;
                    draw->AddText( f, f->FontSize, ImVec2( IM_ROUND( pmin.x + inset ), IM_ROUND( ly_top + slide ) ),
                        col, txt, nullptr, wrap );
                }
            }
            draw->PopClipRect( );

            y += panel_h + 6.f * s;
        }
    }

    ImGui::SetCursorScreenPos( ImVec2( origin.x, origin.y + total_h ) );
    ImGui::End( );
    ImGui::PopStyleColor( );
    ImGui::PopStyleVar( 4 );
}

void c_gui::user_profile( ImVec2 min, ImVec2 max, void* avatar_tex ) {
    spotify_tick( ); // before the SkipItems bail: the player keeps running with the card closed

    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( window->SkipItems )
        return;
    ImDrawList* draw = window->DrawList;
    ImFont* font = ImGui::GetFont( );
    const float s = m_scale;
    const char* popup_name = "UserPopupAnim";

    ImGui::SetCursorScreenPos( min );
    const ImGuiID btn_id = ImGui::GetID( "##user_profile" );
    const bool clicked = ImGui::InvisibleButton( "##user_profile", max - min );
    const bool card_up = ImGui::IsPopupOpen( ImGui::GetID( popup_name ), ImGuiPopupFlags_None );
    const float t = cfg_anim( btn_id, ImGui::IsItemHovered( ) || card_up );

    draw->AddLine( min, max - ImVec2(0, 40), border.to_im_color(), 1.f * s); // same plate as a selected tab
    //draw->AddRectFilled( min, max, tab_active.to_im_color( 0.55f + 0.45f * t ), 8.f * s ); // same plate as a selected tab

    const float h = max.y - min.y;
    const float av = ImMin( 30.f * s, h - 10.f * s );
    const ImVec2 av_min( min.x + 8.f * s, min.y + IM_ROUND( ( h - av ) * 0.5f ) );
    if ( avatar_tex )
        draw->AddImageRounded( ( ImTextureID )avatar_tex, av_min, av_min + ImVec2( av, av ), ImVec2( 0.f, 0.f ), ImVec2( 1.f, 1.f ),
            ImGui::GetColorU32( ImVec4( 1.f, 1.f, 1.f, 1.f ) ), av * 0.5f );
    else
        draw->AddCircleFilled( av_min + ImVec2( av, av ) * 0.5f, av * 0.5f, frame_active.to_im_color( ), 32 );

    const char* status = tr( user_status.c_str( ) );
    // The sub-line and the chevron come from the caption face at its own baked size - drawn from
    // the body font they were a scaled bitmap, which read as blurry.
    ImFont* small = small_font( );
    const float name_fs = ImGui::GetFontSize( ), sub_fs = small->FontSize, chev_fs = small->FontSize;
    const ImVec2 name_sz = font->CalcTextSizeA( name_fs, FLT_MAX, 0.f, user_name.c_str( ) );
    const ImVec2 sub_sz  = small->CalcTextSizeA( sub_fs, FLT_MAX, 0.f, status );
    const ImVec2 chev_sz = small->CalcTextSizeA( chev_fs, FLT_MAX, 0.f, ICON_FA_CHEVRON_RIGHT );
    const float chev_x = max.x - 10.f * s - chev_sz.x;
    const float text_x = av_min.x + av + 9.f * s;
    const float ty = IM_ROUND( min.y + ( h - name_sz.y - sub_sz.y ) * 0.5f );

    draw->PushClipRect( ImVec2( text_x, min.y ), ImVec2( chev_x - 4.f * s, max.y ), true );
    draw->AddText( font, name_fs, ImVec2( text_x, ty ), text.to_im_color( ), user_name.c_str( ) );
    draw->AddText( small, sub_fs, ImVec2( text_x, ty + name_sz.y ), text_disabled.to_im_color( ), status );
    draw->PopClipRect( );
    draw->AddText( small, chev_fs, ImVec2( chev_x, IM_ROUND( min.y + ( h - chev_sz.y ) * 0.5f ) ),
        cfg_mix( text_disabled.to_vec4( 1.f, false ), text.to_vec4( 1.f, false ), t ), ICON_FA_CHEVRON_RIGHT );

    bool wants_open = false;
    const float anim = popup_animation( popup_name, clicked, wants_open );
    // To the right of the button, bottom edges aligned.
    user_profile_card( wants_open, anim, ImVec2( max.x + 10.f * s, max.y ), avatar_tex );
}

void c_gui::user_profile_card( bool& wants_open, float anim, ImVec2 anchor, void* avatar_tex ) {
    if ( !begin_popup_card_at( "UserPopupAnim", wants_open, anim, anchor, ImVec2( 0.f, 1.f ), 250.f ) )
        return;

    ImGuiIO& io = ImGui::GetIO( );
    ImDrawList* draw = ImGui::GetWindowDrawList( );
    ImFont* font = io.Fonts->Fonts[ 0 ];
    ImFont* bold = io.Fonts->Fonts.Size > 4 ? io.Fonts->Fonts[ 4 ] : font;
    const float s = m_scale;
    const float w = ImGui::GetContentRegionAvail( ).x;
    const float fs = ImGui::GetFontSize( );
    const float small = 15.f * s;
    const float row_h = 32.f * s;
    const ImVec4 c_text = text.to_vec4( 1.f, false ), c_dim = text_disabled.to_vec4( 1.f, false );

    // ---- header: avatar, name, subscription, renew ----
    {
        const char* till = tr( user_till.c_str( ) );
        const char* renew = tr( "Renew" );
        const ImVec2 hp = ImGui::GetCursorScreenPos( );
        const float av = 40.f * s;
        ImFont* caption = small_font( );
        const float caption_fs = caption->FontSize;
        const ImVec2 name_sz  = bold->CalcTextSizeA( fs, FLT_MAX, 0.f, user_name.c_str( ) );
        const ImVec2 till_sz  = caption->CalcTextSizeA( caption_fs, FLT_MAX, 0.f, till );
        const ImVec2 renew_sz = caption->CalcTextSizeA( caption_fs, FLT_MAX, 0.f, renew );
        const float block = name_sz.y + till_sz.y + renew_sz.y;
        const float head_h = ImMax( av, block );
        const ImVec2 av_min( hp.x + 4.f * s, hp.y + IM_ROUND( ( head_h - av ) * 0.5f ) );
        if ( avatar_tex )
            draw->AddImageRounded( ( ImTextureID )avatar_tex, av_min, av_min + ImVec2( av, av ), ImVec2( 0.f, 0.f ), ImVec2( 1.f, 1.f ),
                ImGui::GetColorU32( ImVec4( 1.f, 1.f, 1.f, 1.f ) ), av * 0.5f );
        else
            draw->AddCircleFilled( av_min + ImVec2( av, av ) * 0.5f, av * 0.5f, frame_active.to_im_color( ), 32 );

        const float tx = av_min.x + av + 12.f * s;
        float ty = hp.y + IM_ROUND( ( head_h - block ) * 0.5f );
        draw->AddText( bold, fs, ImVec2( tx, ty ), text.to_im_color( ), user_name.c_str( ) );
        ty += name_sz.y;
        draw->AddText( caption, caption_fs, ImVec2( tx, ty ), text_disabled.to_im_color( ), till );
        ty += till_sz.y;
        draw->AddText( caption, caption_fs, ImVec2( tx, ty ), accent_color.to_im_color( ), renew );
        ImGui::SetCursorScreenPos( ImVec2( hp.x, hp.y + head_h + 4.f * s ) );
    }

    static int menu_scale = 1, esp_scale = 1, win_scale = 1, units = 0, safe_mode = 0;
    static bool sync = true;
    const char* langs[]     = { "English", "Russian", "Turkish" };
    const char* scales[]    = { "75%", "100%", "125%", "150%", "175%", "200%" };
    const char* unit_opts[] = { "Auto", "Metric", "Imperial" };
    const char* safe_opts[] = { "Disabled", "Enabled" };
    const char* themes[]    = { "Dark", "Light" };
    const float scale_values[] = { 0.75f, 1.f, 1.25f, 1.5f, 1.75f, 2.f };

    popup_option( ICON_FA_GLOBE,          "Language",      langs,     IM_ARRAYSIZE( langs ),     &language );
    popup_option( ICON_FA_TEXT_HEIGHT,    "Menu Scale",    scales,    IM_ARRAYSIZE( scales ),    &menu_scale );
    m_scale = scale_values[ menu_scale ];
    popup_option( ICON_FA_TEXT_SIZE,      "ESP Scale",     scales,    IM_ARRAYSIZE( scales ),    &esp_scale );
    popup_option( ICON_FA_CLONE,          "Windows Scale", scales,    IM_ARRAYSIZE( scales ),    &win_scale );
    popup_option( ICON_FA_RULER_TRIANGLE, "Units",         unit_opts, IM_ARRAYSIZE( unit_opts ), &units );

    // Style: the cog opens the Dark / Light card, the swatch edits the accent color (no
    // gradient - the accent is a flat color everywhere). Both go in before the row so they
    // own their clicks; clicking the rest of the row opens the theme card too.
    {
        const ImVec2 rp = ImGui::GetCursorScreenPos( );
        const float sz = 14.f * s;
        const ImRect sw( ImVec2( rp.x + w - 4.f * s - sz, rp.y + IM_ROUND( ( row_h - sz ) * 0.5f ) ),
                         ImVec2( rp.x + w - 4.f * s, rp.y + IM_ROUND( ( row_h - sz ) * 0.5f ) + sz ) );
        m_accent_edit.a = accent_color.to_vec4( 1.f, false );
        m_accent_edit.gradient = false;
        color_slot accent_slot{ "Accent", &m_accent_edit };
        ImGui::PushID( "##accent" );
        const bool sw_clicked = color_swatch_item( "##swatch", m_accent_edit, sw );
        if ( color_card_begin( &accent_slot, 1, sw_clicked, sw, false ) )
            color_card_end( );
        ImGui::PopID( );
        accent_color = { m_accent_edit.a.x, m_accent_edit.a.y, m_accent_edit.a.z, m_accent_edit.a.w };

        const char* theme_popup = "##opt_Theme";
        const bool theme_up = ImGui::IsPopupOpen( ImGui::GetID( theme_popup ), ImGuiPopupFlags_None );
        // Centred on the glyph's ink (the swatch beside it is centred by geometry) - centring the
        // cog's em box instead left the two a pixel and a half apart.
        const ImVec2 cog_sz = font->CalcTextSizeA( small, FLT_MAX, 0.f, ICON_FA_COG );
        unsigned int cog_cp = 0;
        ImTextCharFromUtf8( &cog_cp, ICON_FA_COG, NULL );
        const ImFontGlyph* cog_glyph = font->FindGlyph( ( ImWchar )cog_cp );
        const float cog_scale = small / font->FontSize;
        const ImVec2 cog_ink = cog_glyph
            ? ImVec2( cog_glyph->X1 * cog_scale, ( cog_glyph->Y0 + cog_glyph->Y1 ) * 0.5f * cog_scale )
            : ImVec2( cog_sz.x, cog_sz.y * 0.5f );
        const ImVec2 cog_pos( IM_ROUND( sw.Min.x - 9.f * s - cog_ink.x ),
                              IM_ROUND( rp.y + row_h * 0.5f - cog_ink.y ) );
        const ImRect cog( cog_pos - ImVec2( 4.f, 4.f ) * s, cog_pos + cog_sz + ImVec2( 4.f, 4.f ) * s );
        const ImGuiID cog_id = ImGui::GetID( "##theme_cog" );
        bool cog_hovered = false, cog_held = false, cog_clicked = false;
        if ( ImGui::ItemAdd( cog, cog_id ) )
            cog_clicked = ImGui::ButtonBehavior( cog, cog_id, &cog_hovered, &cog_held );
        const float cog_t = cfg_anim( cog_id, cog_hovered || theme_up );

        ImGui::SetCursorScreenPos( rp );
        const popup_row_state r = popup_row_impl( ICON_FA_FILL_DRIP, "Style", theme_up );
        draw->AddText( font, small, cog_pos, cfg_mix( c_dim, c_text, ImMax( cog_t, r.t * 0.6f ) ), ICON_FA_COG );

        int theme_idx = theme;
        if ( popup_option_card( theme_popup, cog_clicked || r.clicked, r.pos.y, themes, IM_ARRAYSIZE( themes ), &theme_idx ) )
            apply_theme( theme_idx );
    }

    popup_separator( );
    popup_option( ICON_FA_SHIELD_ALT, "Safe Mode", safe_opts, IM_ARRAYSIZE( safe_opts ), &safe_mode );
    popup_toggle( ICON_FA_SYNC_ALT, "Synchronization", &sync );
    popup_separator( );
    popup_toggle( ICON_FA_MUSIC, "Spotify", &spotify_widget );
    popup_row( ICON_FA_COPYRIGHT, "About" );
    popup_row( ICON_FA_COMMENTS,  "Chat" );

    end_popup_card( );
}

// ---------------------------------------------------------------------------------------
//  Search
// ---------------------------------------------------------------------------------------

namespace {

// Folds one codepoint to its lowercase, across the Latin, Latin Extended-A and Cyrillic the menu
// can display - so "Sessiz", "SESSİZ" and "тихий" all match what was typed.
unsigned int search_fold_char( unsigned int c ) {
    // 1) lowercase
    if ( c >= 'A' && c <= 'Z' )                          c += 32;
    else if ( c >= 0xC0 && c <= 0xDE && c != 0xD7 )      c += 32;
    else if ( ( c >= 0x100 && c <= 0x137 ) || ( c >= 0x14A && c <= 0x177 ) ) c |= 1;
    else if ( ( c >= 0x139 && c <= 0x148 ) || ( c >= 0x179 && c <= 0x17E ) ) c = ( c & 1 ) ? c + 1 : c;
    else if ( c == 0x178 )                               c = 0xFF;
    else if ( c == 0x130 )                               c = 'i';      // Turkish dotted I
    else if ( c >= 0x410 && c <= 0x42F )                 c += 32;
    else if ( c >= 0x400 && c <= 0x40F )                 c += 80;

    // 2) strip the accent, so "nisan" finds "Nişan" and "uber" finds "Über"
    if ( c >= 0xE0 && c <= 0xE6 )                        return 'a';
    if ( c == 0xE7 )                                     return 'c';
    if ( c >= 0xE8 && c <= 0xEB )                        return 'e';
    if ( c >= 0xEC && c <= 0xEF )                        return 'i';
    if ( c == 0xF0 )                                     return 'd';
    if ( c == 0xF1 )                                     return 'n';
    if ( ( c >= 0xF2 && c <= 0xF6 ) || c == 0xF8 )       return 'o';
    if ( c >= 0xF9 && c <= 0xFC )                        return 'u';
    if ( c == 0xFD || c == 0xFF )                        return 'y';
    if ( c == 0xDF )                                     return 's'; // ß
    switch ( c ) { // Latin Extended-A, lowercase by now
    case 0x101: case 0x103: case 0x105:                                     return 'a';
    case 0x107: case 0x109: case 0x10B: case 0x10D:                         return 'c';
    case 0x10F: case 0x111:                                                 return 'd';
    case 0x113: case 0x115: case 0x117: case 0x119: case 0x11B:             return 'e';
    case 0x11D: case 0x11F: case 0x121: case 0x123:                         return 'g';
    case 0x125: case 0x127:                                                 return 'h';
    case 0x129: case 0x12B: case 0x12D: case 0x12F: case 0x131:             return 'i';
    case 0x135:                                                             return 'j';
    case 0x137:                                                             return 'k';
    case 0x13A: case 0x13C: case 0x13E: case 0x140: case 0x142:             return 'l';
    case 0x144: case 0x146: case 0x148: case 0x14B:                         return 'n';
    case 0x14D: case 0x14F: case 0x151:                                     return 'o';
    case 0x155: case 0x157: case 0x159:                                     return 'r';
    case 0x15B: case 0x15D: case 0x15F: case 0x161:                         return 's';
    case 0x163: case 0x165: case 0x167:                                     return 't';
    case 0x169: case 0x16B: case 0x16D: case 0x16F: case 0x171: case 0x173: return 'u';
    case 0x175:                                                             return 'w';
    case 0x177:                                                             return 'y';
    case 0x17A: case 0x17C: case 0x17E:                                     return 'z';
    case 0x451:                                                             return 0x435; // ё -> е
    default: break;
    }
    return c;
}

// `offsets`, when asked for, ends up one longer than `out`: the byte offset of every codepoint
// plus the end, which is what the result rows use to paint the matched part in the accent color.
void search_fold( const char* text, const char* end, std::vector<unsigned int>& out, std::vector<int>* offsets = nullptr ) {
    out.clear( );
    if ( offsets ) offsets->clear( );
    const char* start = text;
    while ( text < end && *text ) {
        unsigned int c = 0;
        const int bytes = ImTextCharFromUtf8( &c, text, end );
        if ( bytes <= 0 ) break;
        if ( offsets ) offsets->push_back( (int)( text - start ) );
        out.push_back( search_fold_char( c ) );
        text += bytes;
    }
    if ( offsets ) offsets->push_back( (int)( text - start ) );
}

int search_find( const std::vector<unsigned int>& hay, const std::vector<unsigned int>& needle ) {
    if ( needle.empty( ) || needle.size( ) > hay.size( ) ) return -1;
    for ( size_t i = 0; i + needle.size( ) <= hay.size( ); ++i ) {
        size_t k = 0;
        while ( k < needle.size( ) && hay[ i + k ] == needle[ k ] ) ++k;
        if ( k == needle.size( ) ) return (int)i;
    }
    return -1;
}

} // namespace

void c_gui::search_scope_begin( int tab, const char* tab_name ) {
    m_search_tab = tab;
    m_search_tab_name = tab_name ? tab_name : "";
    m_search_box.clear( );
}

void c_gui::search_scope_end( ) {
    m_search_tab = -1;
    m_search_box.clear( );
}

bool c_gui::search_index_pass( ) {
    // A couple of passes, not one: a group box only knows its true height (and so which of its
    // rows it clips away) from the frame after it was first laid out.
    if ( m_search_index_frames >= 4 )
        return false;
    ++m_search_index_frames;
    return true;
}

float c_gui::search_note( const char* label ) {
    if ( m_search_tab < 0 || !label )
        return 0.f;
    const char* end = ImGui::FindRenderedTextEnd( label );
    unsigned int cp = 0;
    const int bytes = ImTextCharFromUtf8( &cp, label, end );
    if ( bytes > 0 && cp >= ICON_MIN_FA && cp <= ICON_MAX_FA ) { // a leading icon isn't part of the name
        label += bytes;
        while ( label < end && *label == ' ' ) ++label;
    }
    if ( label >= end )
        return 0.f;
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if ( is_popup_window( window ) )
        return 0.f; // what a popup holds belongs to the row that opens it, and that row is indexed

    const ImGuiID key = ImHashStr( label, (size_t)( end - label ),
        ImHashStr( m_search_box.c_str( ), 0, ImHashData( &m_search_tab, sizeof( m_search_tab ) ) ) );
    if ( m_search_keys.find( key ) == m_search_keys.end( ) ) {
        m_search_keys[ key ] = (int)m_search_index.size( );
        m_search_index.push_back( { std::string( label, end ), m_search_box, m_search_tab_name, m_search_tab, key } );
    }

    // The row picked from the results says so in its own label: accent, easing back to normal.
    if ( key != m_search_flash_key || m_search_flash <= 0.f )
        return 0.f;
    return ImMin( 1.f, m_search_flash / 0.7f );
}

int c_gui::search_bar( ImVec2 top_right ) {
    ImGuiContext& g = *GImGui;
    ImGuiIO& io = g.IO;
    ImDrawList* draw = ImGui::GetWindowDrawList( );
    const float s = m_scale, h = 30.f * s, wide = 250.f * s;
    const float dt = ImMin( io.DeltaTime, 0.05f );
    const ImVec4 c_text = text.to_vec4( 1.f, false ), c_dim = text_disabled.to_vec4( 1.f, false );
    const ImVec4 c_accent = accent_color.to_vec4( 1.f, false );

    if ( m_search_flash > 0.f )
        m_search_flash = ImMax( 0.f, m_search_flash - dt );

    m_search_anim = ImLerp( m_search_anim, m_search_open ? 1.f : 0.f, 1.f - expf( -16.f * dt ) );
    if ( m_search_anim < 0.002f ) m_search_anim = 0.f;
    if ( m_search_anim > 0.998f ) m_search_anim = 1.f;
    const float ease = m_search_anim * m_search_anim * ( 3.f - 2.f * m_search_anim );
    const float w = ImLerp( h, wide, ease );
    const ImVec2 plate_min( top_right.x - w, top_right.y ), plate_max( top_right.x, top_right.y + h );

    // The magnifier is submitted first, so it owns the hover where it overlaps the plate.
    ImGui::SetCursorScreenPos( ImVec2( plate_max.x - h, plate_min.y ) );
    const ImGuiID icon_id = ImGui::GetID( "##search_icon" );
    const bool icon_clicked = ImGui::InvisibleButton( "##search_icon", ImVec2( h, h ) );
    bool hovered = ImGui::IsItemHovered( );
    const float icon_t = cfg_anim( icon_id, hovered );

    bool plate_clicked = false;
    if ( ease > 0.01f ) {
        ImGui::SetCursorScreenPos( plate_min );
        plate_clicked = ImGui::InvisibleButton( "##search_plate", ImVec2( ImMax( 1.f, w - h ), h ) );
        hovered |= ImGui::IsItemHovered( );
    }

    if ( icon_clicked ) {
        m_search_open = !m_search_open;
        m_search_focus = m_search_open ? 2 : 0;
        if ( !m_search_open ) m_search[ 0 ] = '\0';
    } else if ( plate_clicked ) {
        m_search_focus = 2;
    }

    draw->AddRectFilled( plate_min, plate_max,
        frame_inactive.to_im_color( 0.5f + 0.3f * ImMax( icon_t, ease ) ), 6.f * s );

    const bool was_active = m_search_active;
    bool enter = false;
    if ( ease > 0.01f ) {
        const float text_x = plate_min.x + 12.f * s;
        const float text_w = ImMax( 1.f, plate_max.x - h + 4.f * s - text_x );
        const float text_y = IM_ROUND( plate_min.y + ( h - ImGui::GetFontSize( ) ) * 0.5f - 2.f * s );
        draw->PushClipRect( plate_min, plate_max, true );
        ImGui::PushStyleVar( ImGuiStyleVar_Alpha, g.Style.Alpha * ease );
        const bool want_focus = m_search_focus > 0;
        if ( want_focus ) --m_search_focus;
        enter = bare_input( "##menu_search", m_search, IM_ARRAYSIZE( m_search ), ImVec2( text_x, text_y ), text_w,
                            ImGui::GetFontSize( ), ImGuiInputTextFlags_EnterReturnsTrue, want_focus );
        m_search_active = ImGui::IsItemActive( );
        if ( m_search[ 0 ] == '\0' )
            draw->AddText( ImVec2( text_x, text_y ), text_disabled.to_im_color( 0.75f * ease ), tr( "Search functions..." ) );
        ImGui::PopStyleVar( );
        draw->PopClipRect( );
    } else {
        m_search_active = false;
    }

    const ImVec2 mag = ImGui::CalcTextSize( ICON_FA_SEARCH );
    draw->AddText( ImVec2( IM_ROUND( plate_max.x - h + ( h - mag.x ) * 0.5f ),
                           IM_ROUND( plate_min.y + ( h - mag.y ) * 0.5f - 1.f * s ) ),
        cfg_mix( c_dim, c_text, ImMax( icon_t, ease ) ), ICON_FA_SEARCH );

    if ( was_active && ImGui::IsKeyPressed( ImGuiKey_Escape ) ) {
        m_search[ 0 ] = '\0';
        m_search_open = false;
        m_search_active = false;
    }
    // Focus lost with nothing typed: fold back into the magnifier.
    if ( m_search_open && !m_search_active && m_search_focus == 0 && m_search[ 0 ] == '\0' &&
         !hovered && !m_search_results_hot )
        m_search_open = false;

    const bool show_list = m_search_open && m_search[ 0 ] != '\0' &&
        ( m_search_active || m_search_focus > 0 || m_search_results_hot );

    std::vector<unsigned int> needle;
    if ( show_list ) {
        search_fold( m_search, m_search + strlen( m_search ), needle );
        m_search_hits.clear( );
        std::vector<unsigned int> folded;
        int prefix_hits = 0; // rows that start with what was typed are listed first
        for ( int i = 0; i < (int)m_search_index.size( ) && (int)m_search_hits.size( ) < 8; ++i ) {
            const search_entry& e = m_search_index[ i ];
            const char* shown = tr( e.label.c_str( ) );
            search_fold( shown, shown + strlen( shown ), folded );
            int at = search_find( folded, needle );
            if ( at < 0 && shown != e.label.c_str( ) ) { // typed in English while the menu is translated
                search_fold( e.label.c_str( ), e.label.c_str( ) + e.label.size( ), folded );
                at = search_find( folded, needle );
            }
            if ( at < 0 )
                continue;
            if ( at == 0 )
                m_search_hits.insert( m_search_hits.begin( ) + prefix_hits++, i );
            else
                m_search_hits.push_back( i );
        }
        m_search_sel = m_search_hits.empty( ) ? 0 : ImClamp( m_search_sel, 0, (int)m_search_hits.size( ) - 1 );
        if ( !m_search_hits.empty( ) ) {
            const int count = (int)m_search_hits.size( );
            if ( ImGui::IsKeyPressed( ImGuiKey_DownArrow, true ) ) m_search_sel = ( m_search_sel + 1 ) % count;
            if ( ImGui::IsKeyPressed( ImGuiKey_UpArrow, true ) )   m_search_sel = ( m_search_sel + count - 1 ) % count;
        }
    }

    m_search_list_anim = ImLerp( m_search_list_anim, show_list ? 1.f : 0.f, 1.f - expf( -20.f * dt ) );
    if ( m_search_list_anim < 0.004f ) m_search_list_anim = 0.f;

    int result = -1;
    if ( m_search_list_anim > 0.f ) {
        const float alpha = g.Style.Alpha * m_search_list_anim;
        const float rounding = 12.f * s, row_h = 32.f * s;
        ImGui::SetNextWindowPos( ImVec2( plate_max.x, plate_max.y + 8.f * s + ( 1.f - m_search_list_anim ) * 6.f * s ),
                                 ImGuiCond_Always, ImVec2( 1.f, 0.f ) );
        ImGui::SetNextWindowSize( ImVec2( wide, 0.f ) );
        ImGui::PushStyleVar( ImGuiStyleVar_Alpha, alpha );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 6.f * s, 6.f * s ) );
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.f, 0.f ) );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );
        ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.f, 0.f, 0.f, 0.f ) );
        // Not a popup: opening a popup would take the keyboard focus off the field it belongs to.
        // It is raised to the front by hand instead, which is all a popup would have done here.
        ImGui::Begin( "##SearchResults", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize );
        ImGuiWindow* list = ImGui::GetCurrentWindow( );
        ImGui::BringWindowToDisplayFront( list );
        popup_surface( list->DrawList, list->Pos, list->Pos + list->Size, rounding, alpha );

        ImFont* font = ImGui::GetFont( );
        const float fs = ImGui::GetFontSize( ), small = 15.f * s; // tab / box path, readable next to the label
        const float rw = ImGui::GetContentRegionAvail( ).x;
        if ( m_search_hits.empty( ) ) {
            const ImVec2 p = ImGui::GetCursorScreenPos( );
            const char* none = tr( "No matching functions" );
            list->DrawList->AddText( ImVec2( p.x + 10.f * s, IM_ROUND( p.y + row_h * 0.5f - TextLineCenterY( font, ImGui::GetFontSize( ) ) ) ),
                text_disabled.to_im_color( ), none );
            ImGui::Dummy( ImVec2( rw, row_h ) );
        }
        std::vector<unsigned int> folded;
        std::vector<int> offsets;
        for ( int i = 0; i < (int)m_search_hits.size( ); ++i ) {
            const search_entry& e = m_search_index[ m_search_hits[ i ] ];
            ImGui::PushID( i );
            const ImVec2 p = ImGui::GetCursorScreenPos( );
            const ImGuiID row_id = ImGui::GetID( "##hit" );
            const bool clicked = ImGui::InvisibleButton( "##hit", ImVec2( rw, row_h ) );
            if ( ImGui::IsItemHovered( ) )
                m_search_sel = i;
            const float t = cfg_anim( row_id, m_search_sel == i );
            if ( t > 0.002f )
                list->DrawList->AddRectFilled( p, p + ImVec2( rw, row_h ), frame_inactive.to_im_color( 0.8f * t ), 6.f * s );

            // where the row lives, right-aligned: "Rage   MAIN"
            std::string path = tr( e.tab_name.c_str( ) );
            if ( !e.box.empty( ) ) {
                path += "   ";
                path += tr( e.box.c_str( ) );
            }
            const ImVec2 psz = font->CalcTextSizeA( small, FLT_MAX, 0.f, path.c_str( ) );
            const float path_x = p.x + rw - 10.f * s - psz.x;
            list->DrawList->AddText( font, small, ImVec2( path_x, IM_ROUND( p.y + row_h * 0.5f - TextLineCenterY( font, small ) ) ),
                text_disabled.to_im_color( 0.9f ), path.c_str( ) );

            // the label, with the typed part picked out in the accent color
            const char* shown = tr( e.label.c_str( ) );
            const char* shown_end = ImGui::FindRenderedTextEnd( shown );
            search_fold( shown, shown_end, folded, &offsets );
            const int at = search_find( folded, needle );
            const int glyphs = (int)folded.size( );
            const int parts[ 4 ] = { 0, at < 0 ? glyphs : at,
                                     at < 0 ? glyphs : ImMin( glyphs, at + (int)needle.size( ) ), glyphs };
            float lx = p.x + 10.f * s;
            const float ly = IM_ROUND( p.y + row_h * 0.5f - TextLineCenterY( font, fs ) );
            list->DrawList->PushClipRect( p, ImVec2( path_x - 6.f * s, p.y + row_h ), true );
            for ( int k = 0; k < 3; ++k ) {
                const int a = parts[ k ], b = parts[ k + 1 ];
                if ( b <= a || b >= (int)offsets.size( ) )
                    continue;
                const char* from = shown + offsets[ a ];
                const char* to = shown + offsets[ b ];
                list->DrawList->AddText( font, fs, ImVec2( lx, ly ),
                    ImGui::GetColorU32( k == 1 ? c_accent : c_text ), from, to );
                lx += font->CalcTextSizeA( fs, FLT_MAX, 0.f, from, to ).x;
            }
            list->DrawList->PopClipRect( );

            if ( clicked || ( enter && m_search_sel == i ) ) {
                result = e.tab;
                m_search_flash_key = e.key;
                m_search_flash = 1.2f; // the row's label holds the accent, then eases back
            }
            ImGui::PopID( );
        }
        m_search_results_hot = ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows |
                                                       ImGuiHoveredFlags_AllowWhenBlockedByActiveItem );
        ImGui::End( );
        ImGui::PopStyleColor( );
        ImGui::PopStyleVar( 4 );
    } else {
        m_search_results_hot = false;
    }

    if ( result >= 0 ) { // picked one: the field has done its job
        m_search[ 0 ] = '\0';
        m_search_open = false;
        m_search_active = false;
        m_search_focus = 0;
        m_search_sel = 0;
        ImGui::ClearActiveID( );
    }
    return result;
}
