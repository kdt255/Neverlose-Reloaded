#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include <string>
#include <vector>
#include <cmath>
#include <ctime>

#ifndef ICON_FA_COG
#define ICON_FA_COG "\xef\x80\x93"
#endif

// -------------------------------------------------------------------
//   Notification / 通知系统
//   屏幕右上角飞入动画弹窗
// -------------------------------------------------------------------

struct NotificationItem {
    std::string title;
    std::string message;
    float       spawn_time;    // ImGui::GetTime() when created
    float       duration;      // seconds before fade-out starts
    float       anim_offset;   // current slide offset (x), 0 = fully visible
    int         slot;          // stack slot (0 = top)
};

class NotificationManager {
public:
    static NotificationManager& Get() {
        static NotificationManager instance;
        return instance;
    }

    void Push(const std::string& title, const std::string& message, float duration = 3.0f, bool show_time = true) {
        NotificationItem item;
        item.title = title;
        item.message = show_time ? (message) : message;
        item.spawn_time = ImGui::GetTime();
        item.duration = duration;
        item.anim_offset = 1.0f;   // start from right side
        item.slot = 0;
        m_notifications.push_back(item);
    }

    void Draw() {
        if (m_notifications.empty())
            return;

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw = ImGui::GetForegroundDrawList();

        const float now = ImGui::GetTime();
        const float screen_w = io.DisplaySize.x;
        const float screen_h = io.DisplaySize.y;

        const float notif_w = 280.0f;
        const float notif_h = 72.0f;
        const float padding = 12.0f;
        const float spacing = 8.0f;

        const float slide_in_speed = 6.0f;

        // --- Logo 正方形配置 ---
        const float logo_size = 44.0f;    // 正方形边长
        const float logo_rounding = 10.0f;   // 圆角半径
        const float logo_pad_x = 12.0f;    // 左边距
        const float logo_pad_y = (notif_h - logo_size) * 0.5f; // 垂直居中
        const ImU32 logo_bgColor = IM_COL32(77, 125, 255, 255);
        const ImU32 logo_borderColor = IM_COL32(100, 150, 255, 255);

        for (int i = (int)m_notifications.size() - 1; i >= 0; i--) {
            auto& n = m_notifications[i];
            float age = now - n.spawn_time;

            if (age > n.duration + 1.0f) {
                m_notifications.erase(m_notifications.begin() + i);
                continue;
            }

            // slide-in animation
            float target_offset = 0.0f;
            n.anim_offset = ImLerp(n.anim_offset, target_offset, io.DeltaTime * slide_in_speed);

            // fade in/out
            float alpha = 1.0f;
            if (age < 0.15f) {
                alpha = age / 0.15f;
            }
            if (age > n.duration) {
                float fade = (age - n.duration) / 1.0f;
                alpha = 1.0f - fade;
            }
            alpha = ImClamp(alpha, 0.0f, 1.0f);

            // position: top-right corner
            float x = screen_w - notif_w - padding + (n.anim_offset * (notif_w + 20.0f));
            float y = padding + (notif_h + spacing) * i;

            if (x > screen_w) continue;

            // --- Draw background ---
            ImVec2 bb_min(x, y);
            ImVec2 bb_max(x + notif_w, y + notif_h);

            // shadow
            draw->AddRectFilled(bb_min + ImVec2(2, 2), bb_max + ImVec2(2, 2),
                IM_COL32(0, 0, 0, (int)(80 * alpha)), 6.0f);

            // main bg
            draw->AddRectFilled(bb_min, bb_max,
                IM_COL32(8, 9, 16, (int)(255 * alpha)), 6.0f);

            // border
            draw->AddRect(bb_min, bb_max,
                IM_COL32(255, 255, 255, (int)(8 * alpha)), 6.0f);

            // ========== 1. 绘制圆角正方形 Logo (左边) ==========
            ImVec2 logoMin(bb_min.x + logo_pad_x, bb_min.y + logo_pad_y);
            ImVec2 logoMax(logoMin.x + logo_size, logoMin.y + logo_size);

            // 填充背景
            draw->AddRectFilled(logoMin, logoMax,
                IM_COL32(0, 0, 0, (int)(255 * alpha)), logo_rounding);
            // 描边
            draw->AddRect(logoMin, logoMax,
                IM_COL32(0, 0, 0, (int)(255 * alpha)), logo_rounding, 0, 1.5f);

            // Logo 文字（正方形中心）
            const char* logoText = reinterpret_cast<const char*>(ICON_FA_COG);
            ImVec2 textSize = ImGui::CalcTextSize(logoText);
            ImVec2 logoTextPos(
                logoMin.x + (logo_size - textSize.x) * 0.5f,
                logoMin.y + (logo_size - textSize.y) * 0.5f
            );
            draw->AddText(logoTextPos,
                IM_COL32(255, 255, 255, (int)(255 * alpha)), logoText);

            // --- Draw text (向右偏移，避开 Logo) ---
            float text_offset_x = logo_pad_x + logo_size + 12.0f; // Logo右边留12px间距

            ImVec2 text_pos = bb_min + ImVec2(text_offset_x, 10);
            draw->AddText(text_pos,
                IM_COL32(255, 255, 255, (int)(255 * alpha)), n.title.c_str());

            ImVec2 msg_pos = bb_min + ImVec2(text_offset_x, 32);
            draw->AddText(msg_pos,
                IM_COL32(130, 132, 143, (int)(255 * alpha)), n.message.c_str());

            // --- 倒计时进度条 ---
            float progress = 1.0f;
            float bar_h = 3.0f;
            float bar_pad = 4.0f;
            ImVec2 bar_min(bb_min.x + bar_pad, bb_max.y - bar_h - 2);
            ImVec2 bar_max(bb_max.x - bar_pad, bb_max.y - 2);

            if (age < n.duration) {
                progress = 1.0f - (age / n.duration);
            }
            else {
                progress = 0.0f;
            }

            // 背景条
            draw->AddRectFilled(bar_min, bar_max,
                IM_COL32(30, 35, 50, (int)(120 * alpha)), 1.5f);

            // 前景条 (从右到左收缩)
            float filled_w = (bar_max.x - bar_min.x) * progress;
            if (filled_w > 0.0f) {
                ImVec2 fill_max(bar_min.x + filled_w, bar_max.y);
                draw->AddRectFilled(bar_min, fill_max,
                    gui.accent_color.to_im_color((int)(255 * alpha)), 1.5f);
            }
        }
    }

private:
    std::vector<NotificationItem> m_notifications;

    NotificationManager() {}
    ~NotificationManager() {}
    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;
};

inline void PushNotification(const std::string& title, const std::string& message, float duration = 3.0f, bool show_time = true) {
    NotificationManager::Get().Push(title, message, duration, show_time);
}

inline void DrawNotifications() {
    NotificationManager::Get().Draw();
}
