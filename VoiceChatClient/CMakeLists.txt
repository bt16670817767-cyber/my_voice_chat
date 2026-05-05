cmake_minimum_required(VERSION 3.20)
project(VoiceChatClient)

find_package( GameNetworkingSockets CONFIG REQUIRED)
find_package(rtaudio CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)


set(CMAKE_CXX_STANDARD 17)

add_executable(VoiceChatClient main.cpp
        SocketClient.cpp
        SocketClient.h
        AudioTools.cpp
        AudioTools.h
        UITheme.h
        UINotifications.h
        UIConfirmDialogs.h
        UIStatusBar.h
        UIAuthPanel.h
        UIVoiceTab.h
        UIFriendsTab.h
        UIRoomsTab.h
        "../Common/Messages/AudioMessage.h" "../Common/Messages/SetChannelMessage.h" "../Common/Messages/MessageTypes.h"
        "../Common/Messages/LoginRequestMessage.h" "../Common/Messages/LoginResponseMessage.h" "../Common/Messages/ServerErrorMessage.h"
        "../Common/Messages/RegisterRequestMessage.h" "../Common/Messages/RegisterResponseMessage.h"
        "../Common/Messages/FriendSearchRequestMessage.h" "../Common/Messages/FriendSearchResultMessage.h"
        "../Common/Messages/FriendAddRequestMessage.h" "../Common/Messages/FriendAddResponseMessage.h"
        "../Common/Messages/FriendAcceptRequestMessage.h" "../Common/Messages/FriendAcceptResponseMessage.h"
        "../Common/Messages/FriendRejectRequestMessage.h" "../Common/Messages/FriendRejectResponseMessage.h"
        "../Common/Messages/FriendListRequestMessage.h" "../Common/Messages/FriendListResponseMessage.h"
        "../Common/Messages/FriendRemoveRequestMessage.h" "../Common/Messages/FriendRemoveResponseMessage.h"
        "../Common/Messages/FriendRequestNotifyMessage.h" "../Common/Messages/FriendOnlineNotifyMessage.h"
        "../Common/Messages/FriendSentRequestMessage.h" "../Common/Messages/FriendSentResponseMessage.h"
        "../Common/Messages/RoomCreateRequestMessage.h" "../Common/Messages/RoomCreateResponseMessage.h"
        "../Common/Messages/RoomInviteRequestMessage.h" "../Common/Messages/RoomInviteResponseMessage.h"
        "../Common/Messages/RoomInviteNotifyMessage.h"
        "../Common/Messages/RoomJoinRequestMessage.h" "../Common/Messages/RoomJoinResponseMessage.h"
        "../Common/Messages/RoomLeaveRequestMessage.h" "../Common/Messages/RoomLeaveResponseMessage.h"
        "../Common/Messages/RoomListRequestMessage.h" "../Common/Messages/RoomListResponseMessage.h"
        "../Common/Messages/RoomMemberUpdateMessage.h" "Utils.h")

target_link_libraries(VoiceChatClient
        PRIVATE
        GameNetworkingSockets::shared
        RtAudio::rtaudio
        imgui::imgui
        glfw
        OpenGL::GL)