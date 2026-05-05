#pragma once
#include <imgui.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <cstdint>
#include "SocketClient.h"
#include "AudioTools.h"
#include "Utils.h"
#include "../Common/Messages/SetChannelMessage.h"
#include "UITheme.h"
#include "UINotifications.h"
#include "UIConfirmDialogs.h"

inline bool IsUserSpeaking(SocketClient* client, int32_t userId) {
    auto speakers = client->GetSpeakingUsers();
    auto now = std::chrono::steady_clock::now();
    float nowFloat = std::chrono::duration<float>(now.time_since_epoch()).count();
    for (const auto& s : speakers) {
        if (s.userId == userId && (nowFloat - s.lastAudioTime) < 1.5f) {
            return true;
        }
    }
    return false;
}

inline void RenderRoomsTab(SocketClient* client, AudioTools* audio,
                           NetworkBuffer* buffer,
                           std::atomic<bool>& recording,
                           char* newRoomName, size_t nameSize,
                           char* newRoomPassword, size_t passSize,
                           int& joinRoomId, char* joinRoomPassword, size_t joinPassSize) {
    ImGui::Spacing();

    // --- Create Room ---
    if (ImGui::CollapsingHeader("Create Room", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(8);
        ImGui::SetNextItemWidth(180);
        ImGui::InputTextWithHint("##RoomName", "Room Name", newRoomName, nameSize);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        ImGui::InputTextWithHint("##RoomPass", "Password (optional)", newRoomPassword, passSize);
        ImGui::SameLine();
        if (ImGui::Button("Create##RoomBtn", ImVec2(110, 0))) {
            if (newRoomName[0] != '\0') {
                client->SendRoomCreateRequest(newRoomName, newRoomPassword);
                client->SendRoomListRequest();
            }
        }
        ImGui::Unindent(8);
    }

    // --- Join Room by ID ---
    if (ImGui::CollapsingHeader("Join Room by ID")) {
        ImGui::Indent(8);
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("##JoinRoomId", &joinRoomId);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        ImGui::InputTextWithHint("##JoinRoomPass", "Password", joinRoomPassword, joinPassSize);
        ImGui::SameLine();
        if (ImGui::Button("Join Room", ImVec2(100, 0))) {
            if (joinRoomId > 0) {
                client->SendRoomJoinRequest(joinRoomId, joinRoomPassword);
                client->SendRoomListRequest();
            }
        }
        ImGui::Unindent(8);
    }

    // --- My Rooms ---
    ImGui::Separator();
    ImGui::TextColored(COLOR_HEADER, "My Rooms");
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        client->SendRoomListRequest();
    }

    ImGui::BeginChild("RoomListScroll", ImVec2(0, 0), true);
    auto roomList = client->GetRoomList();
    for (const auto& room : roomList) {
        ImGui::PushID(room.roomId);

        ImVec4 roomColor = room.isJoined ? COLOR_JOINED : (room.isInvited ? COLOR_INVITED : ImVec4(1, 1, 1, 1));
        std::string title = room.roomName + "  [ID:" + std::to_string(room.roomId) + "]  (" + std::to_string(room.memberCount) + " members)";
        bool nodeOpen = ImGui::TreeNodeEx(title.c_str(),
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen);

        if (nodeOpen) {
            ImGui::Indent(16.0f);

            // Status
            if (room.isJoined) {
                ImGui::TextColored(COLOR_JOINED, "[Joined]");
            } else if (room.isInvited) {
                ImGui::TextColored(COLOR_INVITED, "[Invited - you have been invited to this room]");
            }

            if (room.isJoined) {
                // Leave Room
                std::string leaveConfirmId = "ConfirmLeave##" + std::to_string(room.roomId);
                if (ImGui::Button("Leave Room")) {
                    ImGui::OpenPopup(leaveConfirmId.c_str());
                }
                if (ConfirmDialog(leaveConfirmId.c_str(),
                    ("Leave room " + room.roomName + "?").c_str())) {
                    if (recording.load()) {
                        audio->StopRecording();
                        recording.store(false);
                    }
                    client->SendRoomLeaveRequest(room.roomId);
                    client->SendRoomListRequest();
                    PushNotification("Left room: " + room.roomName, COLOR_OFFLINE);
                }

                // Invite Friend
                ImGui::SameLine();
                std::string invitePopupId = "InvitePopup##" + std::to_string(room.roomId);
                if (ImGui::Button("Invite Friend")) {
                    ImGui::OpenPopup(invitePopupId.c_str());
                }
                if (ImGui::BeginPopup(invitePopupId.c_str())) {
                    auto friendsList = client->GetFriendList();
                    ImGui::TextUnformatted("Select a friend to invite:");
                    ImGui::Separator();
                    if (friendsList.empty()) {
                        ImGui::TextColored(COLOR_OFFLINE, "No friends to invite.");
                    }
                    for (const auto& f : friendsList) {
                        if (f.isOnline) {
                            std::string btnLabel = f.displayName + " (ID:" + std::to_string(f.userId) + ")##inv" + std::to_string(room.roomId);
                            if (ImGui::Button(btnLabel.c_str(), ImVec2(250, 0))) {
                                client->SendRoomInviteRequest(room.roomId, f.userId);
                                PushNotification("Invited " + f.displayName, COLOR_ACCENT);
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                    ImGui::EndPopup();
                }

                // Voice toggle
                ImGui::SameLine();
                if (!recording.load()) {
                    std::string voiceBtn = "Start Voice##" + std::to_string(room.roomId);
                    if (ImGui::Button(voiceBtn.c_str())) {
                        SetChannel chMsg;
                        chMsg.channel = static_cast<int64>(room.channel);
                        client->Send(&chMsg, sizeof(chMsg));
                        if (audio->StartRecording(client, buffer)) {
                            recording.store(true);
                            PushNotification("Voice started in room: " + room.roomName, COLOR_ONLINE);
                        } else {
                            PushNotification("Failed to start voice.", COLOR_ERROR);
                        }
                    }
                } else {
                    ImGui::TextColored(COLOR_SPEAKING, "[Mic Active]");
                    ImGui::SameLine();
                    std::string stopBtn = "Stop Voice##" + std::to_string(room.roomId);
                    if (ImGui::Button(stopBtn.c_str())) {
                        audio->StopRecording();
                        recording.store(false);
                        PushNotification("Voice stopped.", COLOR_OFFLINE);
                    }
                }

                // Room ID hint for sharing
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                    "Share this Room ID with friends: %d", room.roomId);
                ImGui::SameLine();
                if (ImGui::Button("Copy Room ID")) {
                    ImGui::SetClipboardText(std::to_string(room.roomId).c_str());
                    PushNotification("Room ID copied.", COLOR_ACCENT);
                }

            } else if (room.isInvited) {
                std::string joinBtn = "Accept Invite##" + std::to_string(room.roomId);
                if (ImGui::Button(joinBtn.c_str(), ImVec2(130, 0))) {
                    client->SendRoomJoinRequest(room.roomId, "");
                    client->SendRoomListRequest();
                    PushNotification("Joined room: " + room.roomName, COLOR_ONLINE);
                }
            }

            ImGui::Unindent(16.0f);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (roomList.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "No rooms. Create a room or join one by ID.");
    }
    ImGui::EndChild();

    // --- Room Members ---
    auto roomMembers = client->GetRoomMembers();
    int32_t selectedRoomId = client->GetSelectedRoomId();
    if (selectedRoomId > 0 && !roomMembers.empty()) {
        ImGui::Separator();
        ImGui::TextColored(COLOR_HEADER, "Room Members:");
        ImGui::BeginChild("RoomMembersScroll", ImVec2(0, 90), true);
        for (const auto& member : roomMembers) {
            bool speaking = IsUserSpeaking(client, member.userId);
            if (speaking) {
                ImGui::TextColored(COLOR_SPEAKING, "[Speaking] ");
                ImGui::SameLine();
            }
            ImGui::BulletText("%s", member.displayName.c_str());
            if (member.userId == client->GetUserId()) {
                ImGui::SameLine();
                ImGui::TextColored(COLOR_OFFLINE, "(you)");
            }
        }
        ImGui::EndChild();
    }
}
