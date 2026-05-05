//
// Created by Amin on 10/17/23.
//
#pragma once

#include "../Common/Messages/MessageTypes.h"
#include "../Common/Messages/AudioMessage.h"
#include "../Common/Messages/SetChannelMessage.h"
#include "../Common/Messages/LoginRequestMessage.h"
#include "../Common/Messages/LoginResponseMessage.h"
#include "../Common/Messages/ServerErrorMessage.h"
#include "../Common/Messages/RegisterRequestMessage.h"
#include "../Common/Messages/RegisterResponseMessage.h"
#include "../Common/Messages/FriendSearchRequestMessage.h"
#include "../Common/Messages/FriendSearchResultMessage.h"
#include "../Common/Messages/FriendAddRequestMessage.h"
#include "../Common/Messages/FriendAddResponseMessage.h"
#include "../Common/Messages/FriendAcceptRequestMessage.h"
#include "../Common/Messages/FriendAcceptResponseMessage.h"
#include "../Common/Messages/FriendRejectRequestMessage.h"
#include "../Common/Messages/FriendRejectResponseMessage.h"
#include "../Common/Messages/FriendListRequestMessage.h"
#include "../Common/Messages/FriendListResponseMessage.h"
#include "../Common/Messages/FriendRemoveRequestMessage.h"
#include "../Common/Messages/FriendRemoveResponseMessage.h"
#include "../Common/Messages/FriendRequestNotifyMessage.h"
#include "../Common/Messages/FriendOnlineNotifyMessage.h"
#include "../Common/Messages/FriendSentRequestMessage.h"
#include "../Common/Messages/FriendSentResponseMessage.h"
#include "../Common/Messages/RoomCreateRequestMessage.h"
#include "../Common/Messages/RoomCreateResponseMessage.h"
#include "../Common/Messages/RoomInviteRequestMessage.h"
#include "../Common/Messages/RoomInviteResponseMessage.h"
#include "../Common/Messages/RoomInviteNotifyMessage.h"
#include "../Common/Messages/RoomJoinRequestMessage.h"
#include "../Common/Messages/RoomJoinResponseMessage.h"
#include "../Common/Messages/RoomLeaveRequestMessage.h"
#include "../Common/Messages/RoomLeaveResponseMessage.h"
#include "../Common/Messages/RoomListRequestMessage.h"
#include "../Common/Messages/RoomListResponseMessage.h"
#include "../Common/Messages/RoomMemberUpdateMessage.h"
#include "Utils.h"
#include <steam/isteamnetworkingutils.h>
#include <cassert>
#include <atomic>
#include <mutex>
#include <string>
#include <cstdint>
#include <vector>



#define PLATFORM_WINDOWS  1
#define PLATFORM_MAC      2
#define PLATFORM_UNIX     3

#if defined(_WIN32)
#define PLATFORM PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define PLATFORM PLATFORM_MAC
#else
#define PLATFORM PLATFORM_UNIX
#endif


#if PLATFORM == PLATFORM_WINDOWS
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <string>
#else
#include <arpa/inet.h>
#endif


#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

struct FriendEntryUi {
    int32_t userId = -1;
    std::string username;
    std::string displayName;
    bool isOnline = false;
};

struct SearchResultUi {
    int32_t userId = -1;
    std::string username;
    std::string displayName;
    int relationshipStatus = 0; // 0=none, 1=friend, 2=outgoing pending, 3=incoming pending
    bool isOnline = false;
};

struct PendingRequestUi {
    int32_t userId = -1;
    std::string username;
    std::string displayName;
};

struct SentRequestUi {
    int32_t userId = -1;
    std::string username;
    std::string displayName;
};

struct SpeakingUser {
    int32_t userId = -1;
    float lastAudioTime = 0.0f;
};

struct RoomEntryUi {
    int32_t roomId = -1;
    int64 channel = 0;
    std::string roomName;
    int32_t ownerUserId = -1;
    int memberCount = 0;
    bool isJoined = false;
    bool isInvited = false;
};

struct RoomMemberUi {
    int32_t userId = -1;
    std::string displayName;
};

class SocketClient {
private:
    static HSteamNetConnection connection;
    static ISteamNetworkingSockets* steamNetworking;
    static SteamNetworkingMicroseconds g_logTimeZero;
    static std::atomic<bool> isConnected;
    static std::atomic<bool> isConnecting;
    static std::atomic<bool> isAuthenticated;
    static std::string statusMessage;
    static std::string authMessage;
    static std::string displayName;
    static int32_t userId;
    static std::mutex statusMessageMutex;
    static std::mutex authMutex;

    // Friend state
    static std::vector<FriendEntryUi> friendList;
    static std::vector<SearchResultUi> searchResults;
    static std::vector<PendingRequestUi> pendingRequests;
    static std::vector<SentRequestUi> sentRequests;
    static std::vector<std::string> notificationQueue;
    static std::mutex friendMutex;
    static std::mutex notificationMutex;

    // Speaking detection
    static std::vector<SpeakingUser> speakingUsers;
    static std::mutex speakingMutex;

    // Auto-refresh flags
    static std::atomic<bool> friendListNeedsRefresh;
    static std::atomic<bool> roomListNeedsRefresh;

    // Room state
    static std::vector<RoomEntryUi> roomList;
    static std::vector<RoomMemberUi> roomMembers;
    static int32_t selectedRoomId;
    static std::mutex roomMutex;

    static void InitSteamDatagramConnectionSockets();
    static void DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg );
    static void OnSteamNetConnectionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pInfo );

public:
    static void SetStatusMessage(const std::string& message);
    static void SetAuthMessage(const std::string& message);
    bool IsConnected();
    bool IsConnecting();
    bool Connect(SteamNetworkingIPAddr add);
    void Disconnect();
    void PollIncomingMessages(NetworkBuffer* _voiceOutputBuffer);
    void PollConnectionStateChanges();
    void Send(const void* data, uint32 size);
    void SendLoginRequest(const char* username, const char* password);
    void SendRegisterRequest(const char* username, const char* password, const char* displayName);

    // Friend send methods
    void SendFriendSearchRequest(const char* query);
    void SendFriendAddRequest(int32_t targetUserId);
    void SendFriendAcceptRequest(int32_t fromUserId);
    void SendFriendRejectRequest(int32_t fromUserId);
    void SendFriendListRequest();
    void SendFriendRemoveRequest(int32_t friendUserId);
    void SendFriendSentRequest();

    // Room send methods
    void SendRoomCreateRequest(const char* roomName, const char* password);
    void SendRoomInviteRequest(int32_t roomId, int32_t targetUserId);
    void SendRoomJoinRequest(int32_t roomId, const char* password);
    void SendRoomLeaveRequest(int32_t roomId);
    void SendRoomListRequest();

    // State accessors
    bool IsAuthenticated();
    std::string GetAuthMessage();
    std::string GetDisplayName();
    int32_t GetUserId();
    std::string GetStatusMessage();

    // Friend accessors
    std::vector<FriendEntryUi> GetFriendList();
    std::vector<SearchResultUi> GetSearchResults();
    std::vector<PendingRequestUi> GetPendingRequests();
    std::vector<SentRequestUi> GetSentRequests();
    std::vector<std::string> DrainNotifications();

    // Speaking detection
    void RecordAudioFromUser(int32_t userId);
    std::vector<SpeakingUser> GetSpeakingUsers();
    void PruneSilentUsers(float thresholdSeconds);

    // Auto-refresh
    bool IsFriendListRefreshNeeded();
    void ClearFriendListRefreshFlag();
    bool IsRoomListRefreshNeeded();
    void ClearRoomListRefreshFlag();

    // Room accessors
    std::vector<RoomEntryUi> GetRoomList();
    std::vector<RoomMemberUi> GetRoomMembers();
    int32_t GetSelectedRoomId();
    void SetSelectedRoomId(int32_t roomId);

    SocketClient();
    ~SocketClient();
};
