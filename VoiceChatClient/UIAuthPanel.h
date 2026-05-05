#pragma once
#include <imgui.h>
#include <string>
#include "SocketClient.h"
#include "UITheme.h"

inline void RenderAuthPanel(SocketClient* client,
                            char* username, size_t usernameSize,
                            char* password, size_t passwordSize,
                            char* reg_username, size_t regUsernameSize,
                            char* reg_password, size_t regPasswordSize,
                            char* reg_display_name, size_t regDisplayNameSize,
                            std::string& outAuthMessage) {
    ImGui::TextColored(COLOR_HEADER, "Voice Chat - Welcome");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTabBar("AuthTabs")) {

        // --- Login Tab ---
        if (ImGui::BeginTabItem("Login")) {
            ImGui::Spacing();
            ImGui::InputText("Username", username, usernameSize);
            ImGui::InputText("Password", password, passwordSize, ImGuiInputTextFlags_Password);
            ImGui::Spacing();
            if (ImGui::Button("Login", ImVec2(140, 30))) {
                if (username[0] != '\0' && password[0] != '\0') {
                    client->SendLoginRequest(username, password);
                } else {
                    client->SetAuthMessage("Please enter username and password.");
                }
            }
            ImGui::EndTabItem();
        }

        // --- Register Tab ---
        if (ImGui::BeginTabItem("Register")) {
            ImGui::Spacing();
            ImGui::InputText("Username", reg_username, regUsernameSize);
            ImGui::InputText("Password", reg_password, regPasswordSize, ImGuiInputTextFlags_Password);
            ImGui::InputText("Display Name", reg_display_name, regDisplayNameSize);
            ImGui::Spacing();
            if (ImGui::Button("Register", ImVec2(140, 30))) {
                if (reg_username[0] == '\0' || reg_password[0] == '\0') {
                    client->SetAuthMessage("Username and password are required.");
                } else {
                    client->SendRegisterRequest(reg_username, reg_password, reg_display_name);
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (!outAuthMessage.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(COLOR_INVITED, "%s", outAuthMessage.c_str());
    }
}
