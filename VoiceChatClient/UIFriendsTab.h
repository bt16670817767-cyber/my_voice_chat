#pragma once
#include <imgui.h>
#include <algorithm>
#include <string>
#include "SocketClient.h"
#include "UITheme.h"
#include "UINotifications.h"
#include "UIConfirmDialogs.h"

inline void RenderFriendsTab(SocketClient* client, char* searchQuery, size_t querySize) {
    // --- Search bar ---
    ImGui::Spacing();
    ImGui::TextColored(COLOR_HEADER, "Add Friend");
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##SearchQuery", "User ID or Name", searchQuery, querySize);
    ImGui::SameLine();
    if (ImGui::Button("Search", ImVec2(80, 0))) {
        if (searchQuery[0] != '\0') {
            client->SendFriendSearchRequest(searchQuery);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(80, 0))) {
        client->SendFriendListRequest();
        client->SendFriendSentRequest();
    }

    // --- Search Results ---
    auto searchResults = client->GetSearchResults();
    if (!searchResults.empty()) {
        ImGui::Separator();
        ImGui::TextColored(COLOR_HEADER, "Search Results:");
        int srCount = static_cast<int>(searchResults.size());
        float srHeight = static_cast<float>((srCount < 4 ? srCount : 4) * 28 + 10);
        ImGui::BeginChild("SearchResultsScroll", ImVec2(0, srHeight), true);
        for (const auto& r : searchResults) {
            ImGui::Text("ID:%d  %s  (@%s)", r.userId, r.displayName.c_str(), r.username.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 220);
            if (r.relationshipStatus == 1) {
                ImGui::TextColored(COLOR_OFFLINE, "[Already friends]");
            } else if (r.relationshipStatus == 2) {
                ImGui::TextColored(COLOR_INVITED, "[Request sent]");
            } else if (r.relationshipStatus == 3) {
                ImGui::PushID(r.userId);
                if (ImGui::Button("Accept")) client->SendFriendAcceptRequest(r.userId);
                ImGui::SameLine();
                if (ImGui::Button("Reject")) client->SendFriendRejectRequest(r.userId);
                ImGui::PopID();
            } else {
                std::string addBtn = "Add Friend##" + std::to_string(r.userId);
                if (ImGui::Button(addBtn.c_str())) {
                    client->SendFriendAddRequest(r.userId);
                    PushNotification("Friend request sent.", COLOR_ACCENT);
                }
            }
            ImGui::SameLine();
            ImGui::TextColored(r.isOnline ? COLOR_ONLINE : COLOR_OFFLINE,
                r.isOnline ? "Online" : "Offline");
        }
        ImGui::EndChild();
    }

    // --- Friend List ---
    ImGui::Separator();
    ImGui::TextColored(COLOR_HEADER, "My Friends:");
    float remainingHeight = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("FriendListScroll", ImVec2(0, remainingHeight * 0.40f), true);
    auto friendList = client->GetFriendList();
    if (friendList.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No friends yet. Search by ID or name to add.");
    }
    for (const auto& f : friendList) {
        ImGui::Text("ID:%d  %s", f.userId, f.displayName.c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::TextColored(f.isOnline ? COLOR_ONLINE : COLOR_OFFLINE,
            f.isOnline ? "Online" : "Offline");
        ImGui::SameLine(ImGui::GetWindowWidth() - 120);
        std::string removeBtn = "Remove##" + std::to_string(f.userId);
        if (ImGui::Button(removeBtn.c_str())) {
            ImGui::OpenPopup(("ConfirmRemove##" + std::to_string(f.userId)).c_str());
        }
        if (ConfirmDialog(("ConfirmRemove##" + std::to_string(f.userId)).c_str(),
            ("Remove " + f.displayName + " from friends?").c_str())) {
            client->SendFriendRemoveRequest(f.userId);
            PushNotification("Friend removed.", COLOR_OFFLINE);
        }
    }
    ImGui::EndChild();

    // --- Sent Requests ---
    auto sentRequests = client->GetSentRequests();
    if (!sentRequests.empty()) {
        ImGui::TextColored(COLOR_INVITED, "Sent Requests (%zu):", sentRequests.size());
        int sCount = static_cast<int>(sentRequests.size());
        float sHeight = static_cast<float>((sCount < 3 ? sCount : 3) * 24 + 8);
        ImGui::BeginChild("SentScroll", ImVec2(0, sHeight), true);
        for (const auto& s : sentRequests) {
            ImGui::Text("ID:%d  %s  (@%s)", s.userId, s.displayName.c_str(), s.username.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 120);
            ImGui::TextColored(COLOR_OFFLINE, "[Waiting...]");
        }
        ImGui::EndChild();
    }

    // --- Pending Requests ---
    auto pendingRequests = client->GetPendingRequests();
    if (!pendingRequests.empty()) {
        ImGui::TextColored(COLOR_ACCENT, "Incoming Requests (%zu):", pendingRequests.size());
        int pCount = static_cast<int>(pendingRequests.size());
        float pHeight = static_cast<float>((pCount < 3 ? pCount : 3) * 28 + 8);
        ImGui::BeginChild("PendingScroll", ImVec2(0, pHeight), true);
        for (const auto& p : pendingRequests) {
            ImGui::Text("ID:%d  %s  (@%s)", p.userId, p.displayName.c_str(), p.username.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 200);
            std::string acceptBtn = "Accept##p" + std::to_string(p.userId);
            if (ImGui::Button(acceptBtn.c_str())) {
                client->SendFriendAcceptRequest(p.userId);
            }
            ImGui::SameLine();
            std::string rejectBtn = "Reject##p" + std::to_string(p.userId);
            if (ImGui::Button(rejectBtn.c_str())) {
                client->SendFriendRejectRequest(p.userId);
            }
        }
        ImGui::EndChild();
    }

    // Drain notifications
    auto notifications = client->DrainNotifications();
    for (const auto& n : notifications) {
        PushNotification(n, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
    }
}
