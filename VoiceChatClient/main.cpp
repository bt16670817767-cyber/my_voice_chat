#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "AudioTools.h"

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
    char server_ip[64] = "127.0.0.1";
    int port = 27020;
    int channel = 0;
    char username[32] = "demo";
    char password[64] = "123456";

    if (argc == 4) {
        std::snprintf(server_ip, sizeof(server_ip), "%s", argv[1]);
        port = std::stoi(argv[2]);
        channel = std::stoi(argv[3]);
    }

    if (!glfwInit()) {
        return 1;
    }

    const char* glsl_version = "#version 130";
    GLFWwindow* window = glfwCreateWindow(900, 600, "Voice Chat Client", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
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
    std::string auth_status_text = "Not logged in.";

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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Voice Chat Controls");
        ImGui::InputText("Server IP", server_ip, sizeof(server_ip));
        ImGui::InputInt("Port", &port);
        ImGui::InputInt("Channel", &channel);

        const std::string socket_status = client_socket->GetStatusMessage();
        if (socket_status != status_text) {
            status_text = socket_status;
        }
        auth_status_text = client_socket->GetAuthMessage();
        const bool authenticated = client_socket->IsAuthenticated();

        if (!connected.load() && !connecting.load()) {
            if (ImGui::Button("Connect")) {
                if (port <= 0 || port > 65535) {
                    status_text = "Port must be between 1 and 65535.";
                } else {
                SteamNetworkingIPAddr server_address;
                    if (!ParseAddress(server_ip, static_cast<uint16>(port), &server_address)) {
                        status_text = "Invalid server IP.";
                    } else if (!client_socket->Connect(server_address)) {
                        status_text = "Failed to create connection.";
                    } else {
                        status_text = "Connecting...";
                    }
                }
            }
        } else if (connecting.load() && !connected.load()) {
            ImGui::TextUnformatted("Connecting...");
        } else {
            ImGui::TextUnformatted("Connected");
            ImGui::InputText("Username", username, sizeof(username));
            ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
            if (!authenticated) {
                if (ImGui::Button("Login")) {
                    client_socket->SendLoginRequest(username, password);
                }
                ImGui::TextUnformatted("Please login before selecting channel or audio.");
            } else {
                ImGui::Text("Logged in as: %s (#%d)", client_socket->GetDisplayName().c_str(), client_socket->GetUserId());
            }

            ImGui::BeginDisabled(!authenticated);
            if (!recording_started.load() && ImGui::Button("Start Audio")) {
                    SetChannel message;
                    message.channel = channel;
                    client_socket->Send(&message, sizeof(message));
                    if (audio_tools->StartRecording(client_socket, &network_buffer)) {
                        recording_started.store(true);
                        status_text = "Audio streaming started.";
                    } else {
                        status_text = "Failed to start audio stream.";
                    }
            }
            if (recording_started.load() && ImGui::Button("Stop Audio")) {
                audio_tools->StopRecording();
                recording_started.store(false);
                status_text = "Audio streaming stopped.";
            }
            ImGui::EndDisabled();
        }

        ImGui::Separator();
        const char* connection_text = connected.load()
                                          ? "Connected"
                                          : (connecting.load() ? "Connecting" : "Disconnected");
        ImGui::Text("Connection: %s", connection_text);
        ImGui::Text("Auth: %s", authenticated ? "Authenticated" : "Unauthenticated");
        ImGui::Text("Audio: %s", recording_started.load() ? "Running" : "Stopped");
        ImGui::TextWrapped("Login: %s", auth_status_text.c_str());
        ImGui::TextWrapped("Status: %s", status_text.c_str());
        ImGui::End();

        ImGui::Render();
        int display_w;
        int display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    running.store(false);
    if (network_loop.joinable()) {
        network_loop.join();
    }
    audio_tools->StopRecording();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    delete audio_tools;
    delete client_socket;
    return 0;
}
