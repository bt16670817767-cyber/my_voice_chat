cmake_minimum_required(VERSION 3.20)
project(VoiceChatServer)

find_package( GameNetworkingSockets CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

# vcpkg 的 SQLiteCpp 在不同平台/版本下导出的 target 名称可能不同。
if (TARGET SQLiteCpp::SQLiteCpp)
        set(SQLITECPP_TARGET SQLiteCpp::SQLiteCpp)
elseif (TARGET SQLiteCpp)
        set(SQLITECPP_TARGET SQLiteCpp)
else()
        message(FATAL_ERROR "SQLiteCpp target not found after find_package(SQLiteCpp CONFIG REQUIRED)")
endif()

set(CMAKE_CXX_STANDARD 17)


add_executable(VoiceChatServer main.cpp
        Server.cpp
        Server.h
        Auth/UserRepository.cpp
        Auth/UserRepository.h
        Auth/PasswordHasher.cpp
        Auth/PasswordHasher.h
        Auth/FriendRepository.cpp
        Auth/FriendRepository.h
        RoomManager.cpp
        RoomManager.h
         "../Common/Messages/AudioMessage.h" "../Common/Messages/MessageTypes.h" "../Common/Messages/SetChannelMessage.h"
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
         "../Common/Messages/RoomMemberUpdateMessage.h")

target_link_libraries(VoiceChatServer
        PRIVATE
        GameNetworkingSockets::shared
        ftxui::screen
        ftxui::dom
        ftxui::component
        ${SQLITECPP_TARGET}
        unofficial-sodium::sodium)
