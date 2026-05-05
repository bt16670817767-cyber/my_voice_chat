#pragma once
#include <imgui.h>
#include <atomic>
#include <algorithm>
#include <string>
#include "SocketClient.h"
#include "AudioTools.h"
#include "Utils.h"
#include "../Common/Messages/SetChannelMessage.h"
#include "UITheme.h"
#include "UINotifications.h"

inline void RenderVoiceTab(SocketClient* client, AudioTools* audio,
                           NetworkBuffer* buffer,
                           std::atomic<bool>& recording,
                           int& channel) {
    ImGui::Spacing();
    ImGui::TextColored(COLOR_HEADER, "Voice Chat");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::InputInt("Channel", &channel);

    // Audio VU meter
    float peakLevel = AudioTools::GetPeakLevel();
    ImGui::TextUnformatted("Mic Level:");
    ImGui::SameLine();
    ImGui::ProgressBar(peakLevel, ImVec2(-1, 14), "");

    ImGui::Spacing();

    if (!recording.load()) {
        if (ImGui::Button("Start Voice", ImVec2(160, 32))) {
            SetChannel message;
            message.channel = channel;
            client->Send(&message, sizeof(message));
            if (audio->StartRecording(client, buffer)) {
                recording.store(true);
                PushNotification("Audio streaming started.", COLOR_ONLINE);
            } else {
                PushNotification("Failed to start audio stream.", COLOR_ERROR);
            }
        }
    } else {
        ImGui::TextColored(COLOR_SPEAKING, "[Voice Active]");
        ImGui::SameLine();
        if (ImGui::Button("Stop Voice", ImVec2(160, 32))) {
            audio->StopRecording();
            recording.store(false);
            PushNotification("Audio streaming stopped.", COLOR_OFFLINE);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Channel %d", channel);
}
