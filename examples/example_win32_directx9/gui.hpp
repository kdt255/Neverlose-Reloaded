#pragma once

#define  IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "color_t.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

void AddSquircleFilled(ImDrawList* draw, ImVec2 p_min, ImVec2 p_max, ImColor col, float radius, ImDrawFlags corner_flags = ImDrawFlags_RoundCornersAll);
void AddSquircle(ImDrawList* draw, ImVec2 p_min, ImVec2 p_max, ImColor col, float radius, float thickness = 1.0f);
void RenderRowHover(ImGuiID id, const ImRect& total_bb); // defined in imgui_widgets.cpp
// Label text clipped to `rect`, centred on its ink; when it overflows it fades at the edge and
// scrolls while hovered, then eases back (defined in imgui_widgets.cpp).
void RenderScrollingText(ImDrawList* draw, ImGuiID id, const ImRect& rect, const char* text,
                        ImU32 color, bool hovered, ImFont* font = nullptr, float font_size = 0.f);
// Half-height of a string's actual ink, for centring text on what is drawn rather than on the
// em box (defined in imgui_widgets.cpp).
float TextInkCenterY(const ImFont* font, float font_size, const char* text, const char* text_end = nullptr);
// Optical centre of a line of text (cap band): the same for every string in a given font and size,
// which is what keeps labels with and without descenders on one line. Use it for text, and
// TextInkCenterY for a lone icon glyph.
float TextLineCenterY(const ImFont* font, float font_size);

using namespace std;

// A color that can also be a two-stop gradient. `a` is the color; when `gradient` is on, the
// preview runs from `a` to `b` (right-click the picker's color field to add/remove stop `b`).
struct color_value {
    ImVec4 a = ImVec4( 1.f, 1.f, 1.f, 1.f );
    ImVec4 b = ImVec4( 1.f, 1.f, 1.f, 1.f );
    bool   gradient = false;
};

// One color of a multi color picker (several colors behind a single swatch).
struct color_slot {
    const char*  name;
    color_value* value;
};

// One entry in the config browser (see c_gui::config_popup).
struct config_entry {
    std::string name;
    bool        cloud = false; // cloud-owned preset: gem badge + download action
    int         created = 0;   // bigger = newer          (sort: Newest First)
    int         modified = 0;  // bigger = touched later   (sort: Recently Modified)
};

class c_gui {

public:

    float m_anim = 0.f;
    int m_tab = 0;
    float m_scale = 1.0f;

    color_t accent_color = { 0.318f, 0.486f, 0.992f, 1.f }; // old one
    //color_t accent_color = { 0.604f, 0.596f, 0.788f, 1.f }; // neverlose v4

    color_t text = { 0.922f, 0.922f, 0.941f, 1.f };
    color_t text_disabled = { 0.51f, 0.52f, 0.56f, 1.f };

    color_t border = { 0.118f, 0.129f, 0.173f, 1.f };
    color_t popup_border = { 0.165f, 0.180f, 0.235f, 1.f };

    color_t frame_inactive = { 0.098f, 0.110f, 0.149f, 1.000f };
    color_t frame_active = { 0.043f, 0.07f, 0.137f, 1.f };

    color_t tab_active = { 0.153f, 0.169f, 0.212f, 1.f };

    color_t button = { 0.102f, 0.102f, 0.133f, 1.f };
    color_t button_hovered = { 0.050f, 0.054f, 0.078f, 1.f };
    color_t button_active = { 0.07f, 0.074f, 0.098f, 1.f };

    color_t group_box_bg = { 0.071f, 0.078f, 0.114f, 1.f };

    // Surfaces the menu draws itself - kept in the palette so the Dark / Light theme swaps them
    // together with everything above.
    color_t panel_side = { 0.071f, 0.078f, 0.114f, 0.80f };   // menu chrome (imgui.cpp)
    color_t panel_top  = { 0.043f, 0.059f, 0.090f, 0.90f };
    color_t panel_body = { 0.043f, 0.055f, 0.086f, 0.90f };
    color_t popup_bg   = { 0.071f, 0.078f, 0.114f, 0.70f };   // tint over every popup's blur
    color_t field_bg   = { 25.f / 255.f, 27.f / 255.f, 36.f / 255.f, 1.f }; // combo / slider / input boxes
    color_t switch_off = { 0.055f, 0.071f, 0.098f, 1.f };     // toggle track and knob when off
    color_t switch_rim = { 1.f, 1.f, 1.f, 0.07f };            // hairline round the off track
    color_t knob_off   = { 0.498f, 0.529f, 0.557f, 1.f };
    color_t text_soft  = { 0.76f, 0.77f, 0.81f, 1.f };        // secondary copy (tooltip body)
    color_t divider    = { 1.f, 1.f, 1.f, 30.f / 255.f };     // hairline splitters
    color_t label_off  = { 150.f / 255.f, 150.f / 255.f, 150.f / 255.f, 1.f }; // checkbox label, off / on
    color_t label_on   = { 1.f, 1.f, 1.f, 1.f };

    // --- Language ---
    // 0 English, 1 Russian, 2 Turkish. tr() gives the text to *show* for an English UI string
    // (the English itself when there's no translation). Widgets keep their IDs on the English
    // label and only draw tr(label), so call sites keep passing English and switching the
    // language never changes an ID. trf() is the same for strings with %s arguments.
    int language = 0;
    const char* tr( const char* text ) const;
    std::string trf( const char* fmt, const char* a, const char* b = nullptr ) const;

    // 0 = Dark, 1 = Light. apply_theme() rewrites the palette (the accent is left alone - it's
    // the user's pick); apply_style_colors() pushes it into ImGuiStyle and must run every frame
    // after the style is reset.
    int  theme = 0;
    void apply_theme( int new_theme );
    void apply_style_colors( );

    void render_circle_for_horizontal_bar( ImVec2 pos, ImColor color, float alpha );

    inline void group_title( const char* name ) {

        SetCursorPosX( GetCursorPosX( ) + 10 );
        ImVec4 title_col = GetStyle( ).Colors[ ImGuiCol_Text ]; // white in Dark, ink in Light
        title_col.w = 0.5f;
        PushStyleColor( ImGuiCol_Text, title_col );
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[6]);
        TextUnformatted( tr( name ) );
        ImGui::PopFont();
        PopStyleColor( );
    }

    // Vertical gap between the rows of a group box. Tabs with long lists (Visuals -> World) tighten
    // it so every row still fits the column; set it back to 8 afterwards.
    float row_spacing = 8.f;
    // Top/bottom padding inside a group box - the same tabs that tighten row_spacing trim this.
    float box_pad_y = 8.f;

    // scrollable: wheel scrolling with edge fades instead of a visible scrollbar.
    void group_box( const char* name, ImVec2 size_arg, bool scrollable = false );
    void popup_group_box( const char* name, ImVec2 size_arg );
    void end_group_box( );
    void shadow_outline(ImDrawList* draw, ImVec2 pos, ImVec2 size, float rounding = 20.0f);

    bool tab( const char* icon, const char* label, bool selected, float indent = 0.f );
    bool subtab( const char* label, bool selected, int size, ImDrawFlags flags );
    bool sub_menu( const char* label );

    // Row helpers: auto-insert a Separator() before every row after the first one in the current
    // group_box() - no more `if (i != last) Separator();` bookkeeping at every call site.
    void row_begin();                             // call before any non-wrapped row widget (e.g. SliderInt)
    bool row_checkbox( const char* label, bool* v, const char* description = "Click to enable or disable this option." );
    bool row_sub_menu( const char* label, const char* description = "Open this section to adjust its settings." );
    void info_tooltip(const char* title, const char* description);
    void popup_shadow(ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float alpha = 1.f);
    void popup_surface(ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding, float alpha, bool border = true);

    float popup_animation(const char* name, bool clicked, bool& wants_open);
    float popup_fade_alpha(ImGuiID id, bool open);
    bool popup_fade_visible(ImGuiID id) const;
    bool begin_animated_popup(ImGuiID id, const char* window_name, ImGuiWindowFlags flags, float alpha);
    void end_animated_popup();
    bool is_popup_window(const ImGuiWindow* window) const;

    // exact_pos: treat click_pos as an anchor the caller placed (only clamped into the menu)
    // instead of the cursor position the card nudges itself around.
    bool begin_popup_card(const char* name, bool& wants_open, float anim, ImVec2 click_pos, float width, bool allow_outside = false, bool exact_pos = false);
    // Card placed by anchor + pivot (pivot (0,1) = the anchor is the card's bottom-left corner,
    // (1,0) = its top-right, ...), kept on screen. For cards that belong to a specific widget.
    bool begin_popup_card_at(const char* name, bool& wants_open, float anim, ImVec2 anchor, ImVec2 pivot, float width, float padding = 10.f);
    void end_popup_card();

    // --- Account block ---
    // The profile button in the menu's corner plus the account card it opens (to its right,
    // bottom-aligned with it). Text comes from the three fields below.
    void user_profile( ImVec2 min, ImVec2 max, void* avatar_tex );
    // The sidebar's nameplate (top-left): the monogram, and the full name unrolling out of it
    // while the pointer is over it. `min` is the menu's top-left corner, `width` the side panel's.
    void name_plate( ImVec2 min, float width );

    void spotify_tick( );
    // The player is a widget, not a popup: toggled from the profile card, dragged wherever the
    // user wants it, and it stays there until the switch goes off.
    bool spotify_widget = false;
    bool spotify_lyrics = false;   // opt-in: nothing is requested from the network until this is on
    bool spotify_gif = false;      // animated backdrop from spotify_gif\ next to the exe
    int  spotify_lyrics_bg = 0;    // 0 with background, 1 without
    void spotify_widget_draw( );
    std::string user_name   = "kdt255";
    std::string user_status = "Till: Lifetime";        // under the name on the button
    std::string user_till   = "Till: Lifetime";  // under the name in the card

    // --- Color pickers ---
    // Swatch + picker card: color field, hue and alpha bars, HEX/RGB + alpha inputs. Right-click
    // the color field to add a second knob - the two knobs are the stops of a gradient (drag
    // either, a line joins them, every preview shows the gradient); right-click a knob to remove
    // it. The card's corner button toggles the gradient too. The card itself can be dragged.
    //
    // Plain calls are self-contained and return true when the color changed this frame:
    //
    //   gui.color_picker( "##kill", &kill_col );      // swatch left of the Checkbox that follows
    //   ImGui::Checkbox( "Kill Effect", &kill );
    //   gui.row_color( "Additional Colors", &col );   // a row of its own: label left, swatch right
    //   gui.row_checkbox( "Glow", &glow, &glow_col ); // checkbox row with its swatch, one call
    //
    // The *_begin() forms keep the card open for extra elements (popup sections, bordered or
    // not) and return true while it is on screen - then color_picker_end() is required:
    //
    //   if ( gui.color_picker_begin( "##glow", &glow_col ) ) {
    //       gui.popup_section_begin( "fx", false ); { ...widgets... } gui.popup_section_end( );
    //       gui.color_picker_end( );
    //   }
    //
    // The color_slot overloads edit several colors behind one swatch: a centered chip row on
    // top of the card switches between them.
    bool color_picker( const char* id, color_value* col );
    bool color_picker( const char* id, color_slot* slots, int count );
    bool color_picker_begin( const char* id, color_value* col );
    bool color_picker_begin( const char* id, color_slot* slots, int count );
    bool row_color( const char* label, color_value* col, const char* description = "Click the swatch to edit the color." );
    bool row_color( const char* label, color_slot* slots, int count, const char* description = "Click the swatch to edit the colors." );
    bool row_color_begin( const char* label, color_value* col, const char* description = "Click the swatch to edit the color." );
    bool row_color_begin( const char* label, color_slot* slots, int count, const char* description = "Click the swatch to edit the colors." );
    bool row_checkbox( const char* label, bool* v, color_value* col, const char* description = "Click to enable or disable this option." );
    void color_picker_end();   // also draws the separator between the picker and any extras

    // Combo row with its own colour swatch (the swatch sits left of the box, like a checkbox's):
    //   gui.row_combo( "Behind Walls", &walls_col, &walls_mode, modes, IM_ARRAYSIZE( modes ) );
    bool row_combo( const char* label, color_value* col, int* index, const char* const items[], int count );

    // --- Floating window ---
    // A panel over the menu with its own surface, a "..." and a close button, dragged by its empty
    // space; it stays up until closed (clicks elsewhere in the menu don't dismiss it):
    //
    //   if ( gui.window_begin( "Ambience", &open, ImVec2( 620, 470 ) ) ) { ...content... gui.window_end( ); }
    bool window_begin( const char* id, bool* open, ImVec2 size, bool* dots_clicked = nullptr );
    void window_end( );

    // A screen of its own *inside* the menu: everything right of the sidebar is covered by a
    // blurred scrim and the panel's content is laid out on top (the Ambience screen). The sidebar
    // stays live, so the tabs keep working behind it.
    bool overlay_begin( const char* id, bool* open, bool* dots_clicked = nullptr );
    void overlay_end( );

    // One line of a framed list (the Ambience map list): accent-bordered while selected, with an
    // optional trailing button ("..." or a label like "Add"). Returns true when the line is picked.
    bool framed_row( const char* label, bool selected, const char* action_icon = nullptr,
                     const char* action_label = nullptr, bool* action_clicked = nullptr, float width = 0.f );

    // --- Segmented header + chips ---
    // icon_tabs(): a bar of icon buttons right-aligned on `top_right`; the active one opens up to
    // show its label. `trailing_icon` is a separate button on the end (true when clicked).
    // chips(): wrapped pill toggles, e.g. the ESP Items panel.
    // Returns the index clicked this frame (-1 for none, `count` for the trailing button). It draws
    // out of the layout flow, so the rows underneath keep their own position.
    // `held` keeps one segment lit while something it opened is still up (the ESP Items card),
    // without making it "the active side".
    int icon_tabs( const char* id, const char* const* icons, const char* const* labels, int count,
                   int* active, ImVec2 top_right, const char* trailing_icon = nullptr, int held = -1 );
    void chips( const char* id, const char* const* labels, bool* values, int count );

    // The little "pull up" handle a card can grow out of: a plate with a chevron that turns over
    // while the card it belongs to is open. Submit the button yourself, then draw it with this.
    void chevron_handle( ImGuiID id, ImVec2 pos, ImVec2 size, bool open );

    // --- Rows for popup cards (account card, color picker extras, any card) ---
    // Full-width rows in this menu's popup style - icon, label, and a value or switch on the
    // right:
    //
    //   gui.popup_option( ICON_FA_FONT, "Font", fonts, 3, &font );   // "Regular >" - opens an option card beside this one
    //   gui.popup_separator( );
    //   gui.popup_toggle( ICON_FA_USER_CIRCLE, "Draw Avatar", &avatar );
    //   if ( gui.popup_row( ICON_FA_COMMENTS, "Chat" ) ) { ... }
    void popup_separator( );
    bool popup_row( const char* icon, const char* label );
    bool popup_option( const char* icon, const char* label, const char* const* options, int count, int* index );
    bool popup_toggle( const char* icon, const char* label, bool* v );

    // Small text (captions, chevrons, values): a face baked at its own size, so it stays crisp
    // instead of being the body font scaled down.
    ImFont* small_font( ) const {
        ImFontAtlas* atlas = ImGui::GetIO( ).Fonts;
        return atlas->Fonts.Size > 7 ? atlas->Fonts[ 7 ] : ImGui::GetFont( );
    }

    // Height of a group box's title strip (the title is centred in it).
    float box_header_h() const { return 18.f * m_scale; }

    // Checkbox toggle size - shared with the swatches that line up against it.
    float switch_w() const { return 30.f * m_scale; }
    float switch_h() const { return 19.f * m_scale; }
    // Toggle switch body (track, rim, knob) - Checkbox and popup_toggle share it. t: 0 off .. 1 on.
    void draw_switch_body( ImDrawList* draw, const ImRect& bb, float t );

    // --- Search ---
    // Magnifier in the menu's top-right corner that widens into a search field (placeholder and
    // results follow the menu language). Every row a tab draws - Checkbox, sliders, combos,
    // sub-menu and color rows - is indexed under its tab and group box; picking a result returns
    // its tab (-1 otherwise) and flashes the row once that tab is on screen.
    //
    //   const int hit = gui.search_bar( top_right_corner );
    //   gui.search_scope_begin( tab, "Rage" ); ...tab content... gui.search_scope_end( );
    //
    // search_index_pass() is true for the first few frames: draw every tab once more inside a
    // hidden child then, so tabs that were never opened are searchable too.
    int  search_bar( ImVec2 top_right );
    void search_scope_begin( int tab, const char* tab_name );
    void search_scope_end( );
    bool search_index_pass( );
    // Called by the row widgets: registers the row and returns how much of the "jumped here"
    // highlight is still on it (1 = full accent, 0 = its own color).
    float search_note( const char* label );

    // --- One-call popups ---
    // Everything a call site used to hand-roll for a popup - a static click-position, a
    // popup_animation() call, threading wants_open/anim into begin_popup_card(), then the
    // title block - lives in here now. popup_begin() returns true while the card is on
    // screen; fill it with popup_section_begin()/end() (or plain widgets) and popup_end():
    //
    //   if ( gui.popup_begin( "Pitch", clicked ) ) { ...content...  gui.popup_end(); }
    //
    // `beside` opens the card next to the click (outside the menu) instead of over it.
    bool popup_begin( const char* name, bool clicked, float width = 216.f, bool titled = true, bool beside = true );
    void popup_end();

    // A "..." button in the slot left of the next Checkbox, opening a card beside the row - for
    // the extra settings of a toggle. Fill it with popup rows / sections, exactly like row_popup:
    //
    //   gui.row_begin( );
    //   if ( gui.row_options_begin( "##traj" ) ) { ...rows... gui.row_options_end( ); }
    //   ImGui::Checkbox( "Grenade Trajectory", &traj );
    bool row_options_begin( const char* id, float width = 216.f );
    void row_options_end( );

    // Sub-menu row + its popup in one call - the row draws itself, clicking it opens the
    // card beside it: `if ( gui.row_popup( "Pitch" ) ) { ...content... gui.popup_end(); }`
    bool row_popup( const char* label, const char* description = "Open this section to adjust its settings.", float width = 216.f );

    // Remembers where a popup was opened from and hands that point back every frame.
    // Pass `anchor` to remember a fixed point (a button corner) instead of the cursor.
    ImVec2 popup_click_pos( const char* name, bool clicked, ImVec2 anchor = ImVec2( -FLT_MAX, -FLT_MAX ) );

    // Config browser popup: "Presets" header with upload / recycle-bin / new actions, a
    // search field with a sort menu, and the preset list with a per-row ... menu
    // (Share / Duplicate / Delete). Double-click a row to rename it.
    // `configs` is edited in place; `selected` is the loaded preset.
    // `anchor` (screen space, e.g. the bottom-left of the button that opens it) keeps the card
    // off its own button; omit it to open at the cursor.
    void config_popup( const char* name, bool clicked, std::vector<config_entry>& configs, int& selected,
                       ImVec2 anchor = ImVec2( -FLT_MAX, -FLT_MAX ) );

    // Places a popup window that draws its own surface (i.e. one not using begin_popup_card)
    // next to `click_pos`, clamped inside the menu, with this menu's slide+lerp entrance.
    // Call it right before begin_animated_popup().
    void popup_position( const char* name, ImVec2 click_pos, ImVec2 default_size, float anim, bool beside = false );

    // --- Popup sections ---
    // Auto-stacking content blocks inside a popup card - no more manual SetCursorScreenPos math
    // or guessed heights. Call popup_section_begin(), then plain widgets (Checkbox/Combo/SliderInt/
    // etc. - anything that advances the cursor normally), then popup_section_end(). Each section
    // measures its own true height via BeginGroup/EndGroup and stacks below the previous one.
    // Pass border=true for a section boxed in its own outline, false for one that's just spaced
    // (e.g. a lone slider sitting below a bordered group, matching this menu's reference design).
    void popup_section_begin( const char* id, bool border = true, float pad = 14.f );
    void popup_section_end();

    // --- Auto-layout grid ---
    // Splits the current content region into `columns` equal-width columns (ItemSpacing.x gap
    // between them) and lets group_box() calls stack vertically inside each column without any
    // manual GetWindowWidth()/GetStyle() math. All sizes are DPI-safe: begin_grid() captures the
    // real (already-scaled) content region, and grid_box() scales literal heights by m_scale itself.
    // `weights` (optional) splits the row unevenly, e.g. { 0.4f, 0.6f }.
    void  begin_grid(int columns, const float* weights = nullptr);
    void  grid_col(int index);                    // switch active column, cursor keeps stacking across calls
    float grid_remaining() const;                 // screen-space height left in the active column
    float grid_width() const;                     // width of the active column (for boxless content)
    // A group_box() header for a column that has no box: same strip height, same title style, so
    // boxless content (the Ambience map list) starts on the exact line a neighbouring box does.
    void  column_title(const char* name);
    void  grid_box(const char* name, float height = 0.f, bool scrollable = false); // >0: design px (auto *m_scale) | 0: fill remaining | <0: fraction of remaining (e.g. -0.5f = half)
    void  grid_skip(float height);                // leave a gap at the top of the active column
    // Height (design px) a k_auto_height box settled on last frame, so a neighbouring box can be
    // given the same one: grid_box( "MAP SELECTION", gui.measured_box_height( "EFFECTS" ) ).
    float measured_box_height(const char* name) const;
    void  end_grid();

private:
    struct popup_animation_state {
        float alpha = 0.f;
        ImVec2 pos{}, size{};
        int last_update = -1;
        int last_render = -1;
        bool was_open = false;
        bool rendered_open = false; // drawn as an open popup last time it was drawn
        ImVec2 drag{}, drag_target{}; // user drag of the card, eased like the menu's own drag
        bool dragging = false;
    };
    struct popup_scope {
        ImGuiID id;
        bool open;
    };
    ImGuiContext* m_popup_context = nullptr;
    int m_popup_context_frame = -1;
    std::unordered_map<ImGuiID, popup_animation_state> m_popup_animations;
    std::vector<popup_scope> m_popup_scopes;
    std::unordered_map<ImGuiID, ImVec2> m_popup_click_pos; // where each popup was opened from
    std::unordered_map<ImGuiID, ImVec2> m_popup_slide_pos; // popup_position(): smoothed window pos
    std::unordered_map<ImGuiID, int> m_window_open_frame;  // window_begin(): frame each panel opened on
    std::vector<ImGuiID> m_window_scopes;                  // window_begin()/end() nesting
    void sync_popup_context();

    ImVec2 m_grid_pos{};
    ImVec2 m_grid_size{};
    int    m_grid_active = 0;
    std::vector<float> m_grid_col_w;
    std::vector<ImVec2> m_grid_cursor;

    // group_box() auto-height: remembers each box's last measured content height
    // (keyed by its ImGuiID) so it can be sized to fit content instead of a fixed value.
    std::unordered_map<ImGuiID, float> m_auto_box_heights;
    ImGuiID m_cur_box_id = 0;
    bool    m_cur_box_auto = false;
    bool    m_force_measure = false; // grid_box(): re-measure content even though a concrete (clamped) height was passed
    bool    m_row_started = false;   // row_checkbox()/row_sub_menu(): false until the first row in this box is drawn

    // popup_section_begin()/end() state for the currently open section
    ImVec2 m_popup_section_pos{};
    float  m_popup_section_pad = 0.f;
    bool   m_popup_section_border = true;

    // config_popup() state
    int   m_config_sort = 0;        // 0 newest, 1 recently modified, 2 alphabetical
    int   m_config_menu_row = -1;   // row whose "..." menu is open
    int   m_config_rename_row = -1; // row being renamed in place
    bool  m_config_rename_focus = false;
    int   m_config_seq = 1;         // hands out created/modified stamps
    char  m_config_search[64] = "";
    char  m_config_rename[64] = "";
    std::vector<config_entry> m_config_trash; // deleted presets, restorable from the bin

    bool begin_popup_card_body(const char* name, ImGuiID id, float anim, float width, float padding = 10.f);

    // Color picker editing state, one per (picker, color). HSV is kept here rather than derived
    // from the RGB every frame: grays and black carry no hue/saturation, so re-deriving would
    // snap the knobs around the moment the color gets desaturated.
    struct color_edit_state {
        float  h[ 2 ] = { 0.f, 0.f }, s[ 2 ] = { 0.f, 0.f }, v[ 2 ] = { 1.f, 1.f };
        ImVec4 synced[ 2 ] = { ImVec4( -1.f, -1.f, -1.f, -1.f ), ImVec4( -1.f, -1.f, -1.f, -1.f ) };
        int    stop = 0;           // stop being edited: 0 = `a`, 1 = `b`
        int    grab = -1;          // color-field knob held by the mouse
        bool   text_active = false;  // HEX/RGB field being typed in (don't overwrite it)
        bool   pct_editing = false;  // alpha box shows its text field
        int    pct_focus = 0;        // frames left to wait for that field to take focus
        char   text[ 40 ] = "";
        char   pct[ 8 ] = "";
    };
    std::unordered_map<ImGuiID, color_edit_state> m_color_states;
    std::unordered_map<ImGuiID, int>  m_color_slot;   // multi picker: color being edited
    std::unordered_map<ImGuiID, int>  m_color_mode;   // input row format: 0 HEX, 1 RGB

    // What a picker card currently *shows* - eased towards the real values, so switching colors
    // (multi picker) or typing a HEX glides the knobs instead of teleporting them. Snaps to the
    // real values while the user is dragging something.
    struct color_view_state {
        bool  init = false;
        float h = 0.f, a = 1.f, grad = 0.f;
        float s[ 2 ] = { 0.f, 0.f }, v[ 2 ] = { 1.f, 1.f };
        float ring_x = 0.f;     // chip row: selection ring, relative to the row's first chip
        int   shown_slot = -1;
        float label_t = 1.f;    // chip row: name fade-in after a switch
    };
    std::unordered_map<ImGuiID, color_view_state> m_color_views;

    bool   color_swatch_item( const char* id, const color_value& shown, const ImRect& bb );
    bool   color_begin_at( const char* id, color_slot* slots, int count, const ImRect& swatch, bool allow_gradient );
    bool   color_card_begin( color_slot* slots, int count, bool clicked, const ImRect& swatch, bool allow_gradient );
    void   color_card_body( ImGuiID state_id, ImGuiID mode_id, ImGuiID view_id, color_value* col, bool allow_gradient );
    ImRect next_checkbox_swatch_rect( ) const;
    ImRect next_combo_swatch_rect( ) const;
    void   color_card_end( );
    // color_card_begin() notes where the picker's own controls end; color_card_end() draws the
    // separator there if extra elements were added below (nothing is drawn for a bare picker).
    struct picker_mark { float bottom, line_y, x, w; };
    std::vector<picker_mark> m_picker_marks;

    void   user_profile_card( bool& wants_open, float anim, ImVec2 anchor, void* avatar_tex );
    color_value m_accent_edit;                       // accent_color seen through the color picker

    // label_end: where the row's own label stops, so a value drawn on the right knows how much
    // room it actually has before it would run into it.
    struct popup_row_state { bool clicked; ImVec2 pos; float t; float w; float label_end; bool hovered; };
    popup_row_state popup_row_impl( const char* icon, const char* label, bool lit );
    void popup_row_value( const popup_row_state& r, const char* value );
    bool popup_option_card( const char* popup_name, bool clicked, float row_y, const char* const* options, int count, int* index );

    // Spotify player (user profile card -> Spotify). spotify_tick() advances playback and has to be
    // called once a frame from somewhere that always runs; spotify_card() only draws it.
    float popup_row_width( ) const;

    // search_bar() state
    struct search_entry {
        std::string label, box, tab_name; // English - shown through tr()
        int         tab;
        ImGuiID     key;
    };
    std::vector<search_entry>        m_search_index;
    std::unordered_map<ImGuiID, int> m_search_keys;       // key -> index entry
    std::vector<int>                 m_search_hits;       // entries listed under the field
    std::string m_search_box;           // group box being drawn
    std::string m_search_tab_name;
    int     m_search_tab = -1;          // tab being drawn (-1: outside search_scope_begin/end)
    int     m_search_index_frames = 0;
    char    m_search[ 64 ] = "";
    bool    m_search_open = false;
    bool    m_search_active = false;    // field had keyboard focus last frame
    bool    m_search_results_hot = false; // pointer on the result list last frame
    int     m_search_focus = 0;         // frames left to hand the field keyboard focus
    int     m_search_sel = 0;
    float   m_search_anim = 0.f;
    float   m_search_list_anim = 0.f;
    ImGuiID m_search_flash_key = 0;
    float   m_search_flash = 0.f;       // seconds left on the picked row's highlight

public:

};

// Pass as the height to group_box()/grid_box() to size the box to fit its content
// (measured from the previous frame) instead of a fixed pixel height.
static constexpr float k_auto_height = -1000.f;

inline c_gui gui;
