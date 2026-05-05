#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "AudioTools.h"
#include "SocketClient.h"
#include "UITheme.h"
#include "UINotifications.h"
#include "UIConfirmDialogs.h"
#include "UIStatusBar.h"
#include "UIAuthPanel.h"
#include "UIFriendsTab.h"
#include "UIRoomsTab.h"

namespace {
bool ParseAddress(const char* address, uint16 port, SteamNetworkingIPAddr* out) {
    in_addr buf;
    if (!inet_pton(AF_INET, address, &buf)) {
        return false;
    }
    out->SetIPv4(htonl(buf.s_addr), port);
    return true;
}
}  // namespace

int main(int argc, const char* argv[]) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    char server_ip[64] = "127.0.0.1";
    int port = 27020;
    char username[32] = {};
    char password[64] = {};
    char reg_username[32] = {};
    char reg_password[64] = {};
    char reg_display_name[32] = {};
    char friend_search_query[32] = {};
    char new_room_name[32] = {};
    char new_room_password[32] = {};
    int join_room_id = 0;
    char join_room_password[32] = {};

    if (argc == 3) {
        std::snprintf(server_ip, sizeof(server_ip), "%s", argv[1]);
        port = std::stoi(argv[2]);
    }

    if (!glfwInit()) {
        return 1;
    }

    const char* glsl_version = "#version 130";
    GLFWwindow* window = glfwCreateWindow(960, 720, "Voice Chat", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetWindowSizeLimits(window, 800, 550, GLFW_DONT_CARE, GLFW_DONT_CARE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyCustomTheme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    auto* client_socket = new SocketClient();
    auto* audio_tools = new AudioTools();
    NetworkBuffer network_buffer;

    std::atomic<bool> running{true};
    std::atomic<bool> connected{false};
    std::atomic<bool> connecting{false};
    std::atomic<bool> recording_started{false};
    std::string status_text = "Idle";
    std::string auth_status_text;
    bool friends_refreshed = false;
    float lastFrameTime = static_cast<float>(glfwGetTime());
    float speakingPruneTimer = 0.0f;

    std::thread network_loop([&] {
        while (running.load()) {
            client_socket->PollIncomingMessages(&network_buffer);
            client_socket->PollConnectionStateChanges();
            connected.store(client_socket->IsConnected());
            connecting.store(client_socket->IsConnecting());
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    });

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(display_w), static_cast<float>(display_h)));
        ImGui::Begin("Voice Chat", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        const std::string socket_status = client_socket->GetStatusMessage();
        if (socket_status != status_text) {
            status_text = socket_status;
        }
        auth_status_text = client_socket->GetAuthMessage();
        const bool authenticated = client_socket->IsAuthenticated();

        // =============================================
        // NOT CONNECTED
        // =============================================
        if (!connected.load() && !connecting.load()) {
            ImGui::TextColored(COLOR_HEADER, "Connect to Server");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::InputText("Server IP", server_ip, sizeof(server_ip));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Port", &port);
            ImGui::SameLine();
            if (ImGui::Button("Connect", ImVec2(100, 0))) {
                if (port <= 0 || port > 65535) {
                    status_text = "Port must be between 1 and 65535.";
                    PushNotification("Invalid port.", COLOR_ERROR);
                } else {
                    SteamNetworkingIPAddr server_address;
                    if (!ParseAddress(server_ip, static_cast<uint16>(port), &server_address)) {
                        status_text = "Invalid server IP.";
                        PushNotification("Invalid server IP.", COLOR_ERROR);
                    } else if (!client_socket->Connect(server_address)) {
                        status_text = "Failed to create connection.";
                        PushNotification("Connection failed.", COLOR_ERROR);
                    } else {
                        status_text = "Connecting...";
                    }
                }
            }
        }
        // =============================================
        // CONNECTING
        // =============================================
        else if (connecting.load() && !connected.load()) {
            ImGui::TextColored(COLOR_INVITED, "Connecting to %s:%d...", server_ip, port);
        }
        // =============================================
        // CONNECTED - NOT AUTHENTICATED
        // =============================================
        else if (!authenticated) {
            ImGui::TextColored(COLOR_ONLINE, "Connected to %s:%d", server_ip, port);
            ImGui::SameLine();
            ImGui::TextColored(COLOR_OFFLINE, "|  Not logged in");
            ImGui::Spacing();
            RenderAuthPanel(client_socket,
                username, sizeof(username),
                password, sizeof(password),
                reg_username, sizeof(reg_username),
                reg_password, sizeof(reg_password),
                reg_display_name, sizeof(reg_display_name),
                auth_status_text);
        }
        // =============================================
        // CONNECTED + AUTHENTICATED
        // =============================================
        else {
            if (!friends_refreshed) {
                client_socket->SendFriendListRequest();
                client_socket->SendRoomListRequest();
                client_socket->SendFriendSentRequest();
                friends_refreshed = true;
            }

            // Auto-refresh
            if (client_socket->IsFriendListRefreshNeeded()) {
                client_socket->SendFriendListRequest();
                client_socket->SendFriendSentRequest();
                client_socket->ClearFriendListRefreshFlag();
            }
            if (client_socket->IsRoomListRefreshNeeded()) {
                client_socket->SendRoomListRequest();
                client_socket->ClearRoomListRefreshFlag();
            }

            // Prune silent speakers every 0.5s
            speakingPruneTimer += deltaTime;
            if (speakingPruneTimer > 0.5f) {
                client_socket->PruneSilentUsers(1.5f);
                speakingPruneTimer = 0.0f;
            }

            // --- User info bar ---
            ImGui::TextColored(COLOR_ONLINE, "%s", client_socket->GetDisplayName().c_str());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "|  ID: #%d", client_socket->GetUserId());
            ImGui::SameLine();
            if (ImGui::Button("Copy ID")) {
                std::string idStr = std::to_string(client_socket->GetUserId());
                ImGui::SetClipboardText(idStr.c_str());
                PushNotification("ID copied to clipboard.", COLOR_ACCENT);
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 110);
            if (ImGui::Button("Disconnect", ImVec2(100, 0))) {
                if (recording_started.load()) {
                    audio_tools->StopRecording();
                    recording_started.store(false);
                }
                client_socket->Disconnect();
                friends_refreshed = false;
                auth_status_text.clear();
            }

            ImGui::Separator();

            // --- Main Tabs ---
            if (ImGui::BeginTabBar("MainTabs")) {

                if (ImGui::BeginTabItem("Friends")) {
                    RenderFriendsTab(client_socket, friend_search_query, sizeof(friend_search_query));
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Rooms")) {
                    RenderRoomsTab(client_socket, audio_tools, &network_buffer,
                        recording_started,
                        new_room_name, sizeof(new_room_name),
                        new_room_password, sizeof(new_room_password),
                        join_room_id, join_room_password, sizeof(join_room_password));
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        // --- Status bar ---
        RenderStatusBar(connected.load(), connecting.load(), authenticated,
            recording_started.load(), client_socket->GetDisplayName());

        ImGui::End();

        // --- Notification overlay ---
        RenderNotifications(deltaTime);

        // --- Render ---
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    running.store(false);
    if (network_loop.joinable()) {
        network_loop.join();
    }
    audio_tools->StopRecording();
    client_socket->Disconnect();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    delete audio_tools;
    delete client_socket;
    return 0;
}
