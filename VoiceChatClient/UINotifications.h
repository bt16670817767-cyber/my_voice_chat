#pragma once
#include <imgui.h>
#include <string>
#include <vector>

struct Notification {
    std::string text;
    float lifetime;
    ImVec4 color;
};

inline std::vector<Notification> g_Notifications;

inline void PushNotification(const std::string& text, ImVec4 color = ImVec4(1, 1, 1, 1), float lifetime = 5.0f) {
    g_Notifications.push_back({text, lifetime, color});
}

inline void RenderNotifications(float deltaTime) {
    if (g_Notifications.empty()) return;

    for (auto it = g_Notifications.begin(); it != g_Notifications.end(); ) {
        it->lifetime -= deltaTime;
        if (it->lifetime <= 0.0f) {
            it = g_Notifications.erase(it);
        } else {
            ++it;
        }
    }
    while (g_Notifications.size() > 5) {
        g_Notifications.erase(g_Notifications.begin());
    }
    if (g_Notifications.empty()) return;

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 310, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::Begin("##Notifications", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs);
    for (const auto& n : g_Notifications) {
        float alpha = n.lifetime > 1.0f ? 1.0f : n.lifetime;
        ImGui::TextColored(ImVec4(n.color.x, n.color.y, n.color.z, alpha), "%s", n.text.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
