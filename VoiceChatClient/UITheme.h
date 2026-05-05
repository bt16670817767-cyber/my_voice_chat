#pragma once
#include <imgui.h>

constexpr ImVec4 COLOR_ONLINE   = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
constexpr ImVec4 COLOR_OFFLINE  = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
constexpr ImVec4 COLOR_SPEAKING = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
constexpr ImVec4 COLOR_INVITED  = ImVec4(1.0f, 0.7f, 0.0f, 1.0f);
constexpr ImVec4 COLOR_JOINED   = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
constexpr ImVec4 COLOR_ERROR    = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
constexpr ImVec4 COLOR_ACCENT   = ImVec4(0.3f, 0.5f, 0.9f, 1.0f);
constexpr ImVec4 COLOR_HEADER   = ImVec4(0.5f, 0.7f, 1.0f, 1.0f);

inline void ApplyCustomTheme() {
    ImGui::GetStyle().WindowRounding = 6.0f;
    ImGui::GetStyle().FrameRounding = 4.0f;
    ImGui::GetStyle().GrabRounding = 4.0f;
    ImGui::GetStyle().TabRounding = 4.0f;
    ImGui::GetStyle().ChildRounding = 4.0f;
    ImGui::GetStyle().ScrollbarRounding = 4.0f;
    ImGui::GetStyle().FramePadding = ImVec2(8, 4);
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 4);

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg]         = ImVec4(0.06f, 0.07f, 0.09f, 1.0f);
    colors[ImGuiCol_ChildBg]          = ImVec4(0.08f, 0.09f, 0.12f, 1.0f);
    colors[ImGuiCol_FrameBg]          = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.18f, 0.20f, 0.26f, 1.0f);
    colors[ImGuiCol_FrameBgActive]    = ImVec4(0.22f, 0.25f, 0.32f, 1.0f);
    colors[ImGuiCol_Tab]              = ImVec4(0.10f, 0.12f, 0.16f, 1.0f);
    colors[ImGuiCol_TabHovered]       = ImVec4(0.18f, 0.22f, 0.30f, 1.0f);
    colors[ImGuiCol_TabActive]        = ImVec4(0.20f, 0.35f, 0.60f, 1.0f);
    colors[ImGuiCol_Button]           = ImVec4(0.18f, 0.28f, 0.48f, 1.0f);
    colors[ImGuiCol_ButtonHovered]    = ImVec4(0.25f, 0.38f, 0.60f, 1.0f);
    colors[ImGuiCol_ButtonActive]     = ImVec4(0.14f, 0.22f, 0.40f, 1.0f);
    colors[ImGuiCol_Header]           = ImVec4(0.18f, 0.28f, 0.45f, 1.0f);
    colors[ImGuiCol_HeaderHovered]    = ImVec4(0.22f, 0.34f, 0.55f, 1.0f);
    colors[ImGuiCol_HeaderActive]     = ImVec4(0.14f, 0.24f, 0.38f, 1.0f);
    colors[ImGuiCol_Separator]        = ImVec4(0.20f, 0.22f, 0.28f, 1.0f);
    colors[ImGuiCol_Border]           = ImVec4(0.16f, 0.18f, 0.24f, 1.0f);
    colors[ImGuiCol_Text]             = ImVec4(0.88f, 0.90f, 0.92f, 1.0f);
    colors[ImGuiCol_TextDisabled]     = ImVec4(0.45f, 0.48f, 0.55f, 1.0f);
    colors[ImGuiCol_PopupBg]          = ImVec4(0.10f, 0.11f, 0.15f, 0.95f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);
}
