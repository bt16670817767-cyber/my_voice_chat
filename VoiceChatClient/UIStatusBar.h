#pragma once
#include <imgui.h>
#include <string>
#include "UITheme.h"

inline void RenderStatusBar(bool connected, bool connecting, bool authenticated,
                            bool recording, const std::string& displayName) {
    ImGui::Separator();
    if (!connected && !connecting) {
        ImGui::TextColored(COLOR_ERROR, "Disconnected");
    } else if (connecting) {
        ImGui::TextColored(COLOR_INVITED, "Connecting...");
    } else if (!authenticated) {
        ImGui::TextColored(COLOR_ONLINE, "Connected");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "|  Please login or register");
    } else {
        ImGui::TextColored(COLOR_ONLINE, "Online");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "|  %s", displayName.c_str());
        if (recording) {
            ImGui::SameLine();
            ImGui::TextColored(COLOR_SPEAKING, "|  Mic ON");
        }
    }
}
