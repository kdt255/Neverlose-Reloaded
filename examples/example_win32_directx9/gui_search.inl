// Included by gui.cpp so search shares the menu's palette and popup renderer.
namespace {
std::string search_fold(const char* value) {
    std::string result;
    if (!value)
        return result;
    for (const char* cursor = value; *cursor;) {
        unsigned int code = 0;
        const int length = ImTextCharFromUtf8(&code, cursor, nullptr);
        if (length <= 0)
            break;
        cursor += length;
        if (code >= 'A' && code <= 'Z') code += 'a' - 'A';
        else if (code >= 0x0410 && code <= 0x042F) code += 0x20;
        else if (code == 0x0401 || code == 0x0451) code = 0x0435;
        else if (code == 0x0130 || code == 0x0131) code = 'i';
        else if (code == 0x00C7 || code == 0x00E7) code = 'c';
        else if (code == 0x011E || code == 0x011F) code = 'g';
        else if (code == 0x015E || code == 0x015F) code = 's';
        else if (code == 0x00D6 || code == 0x00F6) code = 'o';
        else if (code == 0x00DC || code == 0x00FC) code = 'u';
        char bytes[5];
        result += ImTextCharToUtf8(bytes, code);
    }
    return result;
}

bool search_matches(const std::string& text, const std::string& query) {
    size_t begin = 0;
    while (begin < query.size()) {
        begin = query.find_first_not_of(" \t\r\n", begin);
        if (begin == std::string::npos)
            break;
        const size_t end = query.find_first_of(" \t\r\n", begin);
        if (text.find(query.substr(begin, end - begin)) == std::string::npos)
            return false;
        begin = end;
    }
    return true;
}

float search_text_y(ImFont* font, float size, const char* label, float top, float height) {
    float low = FLT_MAX;
    float high = -FLT_MAX;
    for (const char* cursor = label; *cursor;) {
        unsigned int code = 0;
        const int length = ImTextCharFromUtf8(&code, cursor, nullptr);
        if (length <= 0)
            break;
        cursor += length;
        const ImFontGlyph* glyph = font->FindGlyph((ImWchar)code);
        if (glyph && glyph->Visible) {
            low = ImMin(low, glyph->Y0);
            high = ImMax(high, glyph->Y1);
        }
    }
    if (low == FLT_MAX)
        return IM_ROUND(top + (height - size) * 0.5f);
    const float scale = size / font->FontSize;
    return IM_ROUND(top + (height - (high - low) * scale) * 0.5f - low * scale);
}
}

void c_gui::search_scope_begin(int tab, const char* tab_name) {
    m_search_tab = tab;
    m_search_tab_name = tab_name ? tab_name : "";
    m_search_box.clear();
}

void c_gui::search_scope_end() {
    m_search_tab = -1;
    m_search_tab_name.clear();
    m_search_box.clear();
}

bool c_gui::search_index_pass() {
    if (m_search_index_frames >= 3)
        return false;
    ++m_search_index_frames;
    return true;
}

void c_gui::search_note(const char* label, const ImRect& row) {
    if (m_search_tab < 0 || !label || !*label)
        return;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    for (ImGuiWindow* ancestor = window; ancestor; ancestor = ancestor->ParentWindow)
        if (is_popup_window(ancestor) || (ancestor->Flags & ImGuiWindowFlags_Tooltip))
            return;

    // A merged icon font prefixes some labels with a private-use glyph.
    const char* end = ImGui::FindRenderedTextEnd(label);
    while (label < end) {
        unsigned int code = 0;
        const int length = ImTextCharFromUtf8(&code, label, end);
        if (length <= 0 || (code > ' ' && !(code >= 0xE000 && code <= 0xF8FF)))
            break;
        label += length;
    }
    while (end > label && end[-1] == ' ')
        --end;
    if (end == label)
        return;
    const std::string visible(label, end);
    if (visible.find_first_not_of("0123456789 ") == std::string::npos)
        return;

    const ImGuiID tab_key = ImHashData(&m_search_tab, sizeof(m_search_tab));
    const ImGuiID box_key = ImHashStr(m_search_box.c_str(), 0, tab_key);
    const ImGuiID key = ImHashStr(visible.c_str(), 0, box_key);
    if (m_search_keys.find(key) == m_search_keys.end()) {
        m_search_keys.emplace(key, static_cast<int>(m_search_index.size()));
        m_search_index.push_back({visible, m_search_box, m_search_tab_name, m_search_tab, key});
    }

    if (key != m_search_flash_key || m_search_flash <= 0.f || window->SkipItems ||
        row.GetWidth() <= 0.f || row.GetHeight() <= 0.f)
        return;
    const float intensity = ImMin(m_search_flash / 0.5f, 1.f);
    window->DrawList->AddRectFilled(row.Min, row.Max, accent_color.to_im_color(0.12f * intensity), 5.f * m_scale);
    window->DrawList->AddRect(row.Min + ImVec2(0.5f, 0.5f), row.Max - ImVec2(0.5f, 0.5f),
                             accent_color.to_im_color(0.6f * intensity), 5.f * m_scale, 0, m_scale);
    if (row.Min.y < window->InnerRect.Min.y || row.Max.y > window->InnerRect.Max.y)
        ImGui::SetScrollFromPosY(window, row.GetCenter().y - window->Pos.y, 0.5f);
}

int c_gui::search_bar(ImVec2 top_right) {
    ImGuiContext& g = *GImGui;
    ImGuiWindow* parent = ImGui::GetCurrentWindow();
    const ImVec2 cursor_backup = ImGui::GetCursorScreenPos();
    const ImGuiLastItemData item_backup = g.LastItemData;
    const float s = m_scale;
    const float dt = ImMin(g.IO.DeltaTime, 0.05f);
    const float blend = 1.f - expf(-18.f * dt);
    const float height = 30.f * s;
    const float closed_width = 30.f * s;
    const float full_width = ImMax(closed_width, ImMin(255.f * s, parent->Size.x - 545.f * s));
    const bool was_active = m_search_active;
    int picked_tab = -1;

    m_search_flash = ImMax(0.f, m_search_flash - dt);
    ImGui::PushID("##MenuSearch");
    ImGui::SetCursorScreenPos(top_right - ImVec2(closed_width, 0.f));
    if (ImGui::InvisibleButton("##magnifier", ImVec2(closed_width, height))) {
        m_search_open = !m_search_open;
        m_search_focus = m_search_open ? 1 : 0;
    }
    const bool icon_hovered = ImGui::IsItemHovered();
    const bool escape_pressed = m_search_open && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (escape_pressed) {
        m_search_open = false;
        m_search_focus = 0;
        if (was_active)
            ImGui::ClearActiveID();
    }

    const float list_height = (ImMin(static_cast<int>(m_search_hits.size()), 6) * 43.f + 16.f) * s;
    const ImRect list_rect(ImVec2(top_right.x - full_width, top_right.y + height + 8.f * s),
                           ImVec2(top_right.x, top_right.y + height + 8.f * s + ImMax(list_height, 50.f * s)));
    const ImRect expanded_rect(top_right - ImVec2(full_width, 0.f), top_right + ImVec2(0.f, height));
    if (m_search_open && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !expanded_rect.Contains(g.IO.MousePos) &&
        !(m_search_list_anim > 0.01f && list_rect.Contains(g.IO.MousePos))) {
        m_search_open = false;
        m_search_focus = 0;
    }
    if (m_search_open && ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        m_search_open = false;
        m_search_focus = 0;
    }

    m_search_anim += ((m_search_open ? 1.f : 0.f) - m_search_anim) * blend;
    if (m_search_anim < 0.001f) m_search_anim = 0.f;
    if (m_search_anim > 0.999f) m_search_anim = 1.f;
    const float width = ImLerp(closed_width, full_width, m_search_anim);
    const ImVec2 field_pos(top_right.x - width, top_right.y);
    ImDrawList* draw = parent->DrawList;
    if (m_search_anim > 0.f || icon_hovered)
        draw->AddRectFilled(field_pos, field_pos + ImVec2(width, height),
                            field_bg.to_im_color(m_search_anim * 0.9f + (1.f - m_search_anim) * 0.6f), 7.f * s);
    if (m_search_anim > 0.f)
        draw->AddRect(field_pos + ImVec2(0.5f, 0.5f), field_pos + ImVec2(width - 0.5f, height - 0.5f),
                       popup_border.to_im_color(m_search_anim * 0.65f), 7.f * s, 0, s);

    ImFont* font = ImGui::GetFont();
    const float icon_size = 13.f * s;
    const ImVec2 icon_extent = font->CalcTextSizeA(icon_size, FLT_MAX, 0.f, ICON_FA_SEARCH);
    draw->AddText(font, icon_size,
                  ImVec2(IM_ROUND(top_right.x - (closed_width + icon_extent.x) * 0.5f),
                         search_text_y(font, icon_size, ICON_FA_SEARCH, top_right.y, height)),
                  (icon_hovered || m_search_open ? accent_color : text_disabled).to_im_color(), ICON_FA_SEARCH);

    m_search_active = false;
    if (width > 70.f * s) {
        const float query_width = width - closed_width - 15.f * s;
        ImGui::SetCursorScreenPos(field_pos + ImVec2(10.f * s, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            ImVec2(0.f, search_text_y(font, font->FontSize, tr("Search"), 0.f, height)));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g.Style.Alpha * m_search_anim);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_Text, text.to_vec4(1.f, false));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, text_disabled.to_vec4(1.f, false));
        ImGuiWindowFlags field_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!m_search_open)
            field_flags |= ImGuiWindowFlags_NoInputs;
        ImGui::BeginChild("##search_field", ImVec2(query_width, height), false, field_flags);
        if (m_search_open && m_search_focus > 0 && m_search_anim > 0.7f) {
            ImGui::SetKeyboardFocusHere();
            --m_search_focus;
        }
        ImGui::BeginDisabled(!m_search_open);
        if (ImGui::InputTextEx("##query", tr("Search functions..."), m_search, IM_ARRAYSIZE(m_search),
                              ImVec2(query_width, height), ImGuiInputTextFlags_None, nullptr, nullptr))
            m_search_sel = 0;
        m_search_active = ImGui::IsItemActive();
        ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);
    }

    if (m_search_open || m_search_list_anim <= 0.001f) {
        m_search_hits.clear();
        const std::string query = search_fold(m_search);
        for (int i = 0; i < static_cast<int>(m_search_index.size()); ++i) {
            const search_entry& entry = m_search_index[i];
            const std::string caption = std::string(tr(entry.label.c_str())) + " " +
                                        tr(entry.box.c_str()) + " " + tr(entry.tab_name.c_str());
            if (search_matches(search_fold(caption.c_str()), query))
                m_search_hits.push_back(i);
        }
    }
    m_search_sel = ImClamp(m_search_sel, 0, ImMax(0, static_cast<int>(m_search_hits.size()) - 1));
    bool keyboard_moved = false;
    if (m_search_open && (was_active || m_search_active) && !m_search_hits.empty()) {
        const int count = static_cast<int>(m_search_hits.size());
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            m_search_sel = (m_search_sel + 1) % count;
            keyboard_moved = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            m_search_sel = (m_search_sel + count - 1) % count;
            keyboard_moved = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            const search_entry& entry = m_search_index[m_search_hits[m_search_sel]];
            picked_tab = entry.tab;
            m_search_flash_key = entry.key;
            m_search_flash = 2.f;
            m_search_open = false;
            ImGui::ClearActiveID();
        }
    }

    const bool show_results = m_search_open && m_search_anim > 0.7f;
    if (show_results)
        m_search_list_anim += (1.f - m_search_list_anim) * blend;
    else
        m_search_list_anim = ImMax(0.f, m_search_list_anim - dt / 0.14f);
    m_search_results_hot = false;
    if (m_search_list_anim > 0.001f) {
        const float content_height = ImMax(34.f, ImMin(static_cast<int>(m_search_hits.size()), 6) * 43.f) * s;
        const ImVec2 list_size(full_width, content_height + 16.f * s);
        const ImGuiNextWindowData next_backup = g.NextWindowData;
        ImGui::SetNextWindowPos(list_rect.Min);
        ImGui::SetNextWindowSize(list_size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f * s, 8.f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        const float alpha = g.Style.Alpha * m_search_list_anim;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        // A separate overlay lets the field keep keyboard focus while results are hovered.
        ImGuiWindowFlags flags = ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoBackground;
        if (!show_results)
            flags |= ImGuiWindowFlags_NoInputs;
        const bool visible = ImGui::Begin("##MenuSearchResults", nullptr, flags);
        if (visible) {
            ImGuiWindow* list_window = ImGui::GetCurrentWindow();
            popup_surface(list_window->DrawList, list_window->Pos, list_window->Pos + list_window->Size,
                           12.f * s, alpha, false);
            m_search_results_hot = show_results && ImGui::IsWindowHovered();
            if (m_search_hits.empty()) {
                const char* empty = tr("No matching functions");
                list_window->DrawList->AddText(ImVec2(list_window->DC.CursorPos.x + 4.f * s,
                    search_text_y(font, font->FontSize, empty, list_window->DC.CursorPos.y, 34.f * s)),
                    text_disabled.to_im_color(), empty);
                ImGui::Dummy(ImVec2(0.f, 34.f * s));
            } else {
                const float row_width = ImGui::GetContentRegionAvail().x;
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(m_search_hits.size()), 43.f * s);
                if (keyboard_moved)
                    clipper.IncludeRangeByIndices(m_search_sel, m_search_sel + 1);
                while (clipper.Step()) {
                    for (int hit = clipper.DisplayStart; hit < clipper.DisplayEnd; ++hit) {
                        const search_entry& entry = m_search_index[m_search_hits[hit]];
                        const ImVec2 pos = ImGui::GetCursorScreenPos();
                        ImGui::PushID(entry.key);
                        const bool chosen = ImGui::InvisibleButton("##result", ImVec2(row_width, 43.f * s));
                        const bool hovered = show_results && ImGui::IsItemHovered();
                        if (hovered)
                            m_search_sel = hit;
                        if (hit == m_search_sel)
                            list_window->DrawList->AddRectFilled(pos, pos + ImVec2(row_width, 41.f * s),
                                                                 tab_active.to_im_color(hovered ? 0.9f : 0.65f), 6.f * s);
                        const char* caption = tr(entry.label.c_str());
                        const std::string location = std::string(tr(entry.tab_name.c_str())) + " / " + tr(entry.box.c_str());
                        list_window->DrawList->PushClipRect(pos + ImVec2(7.f * s, 0.f), pos + ImVec2(row_width - 7.f * s, 43.f * s), true);
                        list_window->DrawList->AddText(font, font->FontSize,
                            ImVec2(pos.x + 8.f * s, search_text_y(font, font->FontSize, caption, pos.y + 2.f * s, 23.f * s)),
                            (hit == m_search_sel ? text : text_soft).to_im_color(), caption);
                        list_window->DrawList->AddText(font, 10.f * s,
                            ImVec2(pos.x + 8.f * s, search_text_y(font, 10.f * s, location.c_str(), pos.y + 25.f * s, 13.f * s)),
                            text_disabled.to_im_color(), location.c_str());
                        list_window->DrawList->PopClipRect();
                        if (keyboard_moved && hit == m_search_sel)
                            ImGui::SetScrollHereY(0.5f);
                        if (chosen && show_results) {
                            picked_tab = entry.tab;
                            m_search_flash_key = entry.key;
                            m_search_flash = 2.f;
                            m_search_open = false;
                        }
                        ImGui::PopID();
                    }
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(4);
        g.NextWindowData = next_backup;
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(cursor_backup);
    g.LastItemData = item_backup;
    if (picked_tab >= 0) {
        m_search[0] = '\0';
        m_search_focus = 0;
    }
    return picked_tab;
}
