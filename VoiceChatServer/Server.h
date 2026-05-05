//
// Created by Amin on 10/15/23.
//

#ifndef VOICECHATSERVER_SERVER_H
#define VOICECHATSERVER_SERVER_H

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


#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <stdio.h>
#include <cassert>
#include <thread>
#include "queue"
#include <csignal>

#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

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
#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include <mutex>
#include <unordered_set>
#include <vector>

class FriendRepository;
class RoomManager;

struct UserInfo {
    int32_t userId = -1;
    std::string username;
    std::string displayName;
    int32_t activeRoomCount = 0;
};

struct ChannelInfo {
    int64 channelId = 0;
    size_t connectionCount = 0;
};

struct RoomSummary {
    int32_t roomId = -1;
    std::string roomName;
    size_t memberCount = 0;
    int32_t ownerUserId = -1;
};

struct LogEntry {
    std::string timestamp;
    std::string message;
};

class Server {
private:
    struct SessionInfo {
        bool authenticated = false;
        int32_t userId = -1;
        std::string username;
        std::string displayName;
        std::set<int32_t> activeRoomIds;
    };

    HSteamListenSocket socket;
    uint64 sentBytesCount;
    uint64 receivedBytesCount;
    static ISteamNetworkingSockets* steamNetworking;
    static SteamNetworkingMicroseconds g_logTimeZero;
    static HSteamNetPollGroup connectionPollGroup;
    static std::map<int64, std::set<HSteamNetConnection>> channelToConnnectionsMap;
    static std::map<HSteamNetConnection, SessionInfo> connectionSessions;
    static std::mutex sessionMutex;
    static std::mutex channelMutex;
    static std::unordered_set<int32_t> onlineUsers;
    static std::mutex onlineUsersMutex;
    static FriendRepository* friendRepo;
    static RoomManager roomManager;
    static std::vector<LogEntry> eventLog;
    static std::mutex eventLogMutex;
    static constexpr size_t MAX_LOG_ENTRIES = 200;

    static void InitSteamDatagramConnectionSockets();
    static void DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg );
    static void OnSteamNetConnectionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pInfo );
    static void RemoveConnectionFromChannel(HSteamNetConnection connection, int64 channel);
    static bool IsAuthenticated(HSteamNetConnection connection);
    static void SendServerError(HSteamNetConnection connection, int32_t errorCode, const char* message);

    // Auth handlers
    static void HandleLoginRequest(HSteamNetConnection connection, const LoginRequest* request, uint32 messageSize);
    static void HandleRegisterRequest(HSteamNetConnection connection, const RegisterRequest* request, uint32 messageSize);

    // Friend handlers
    static void HandleFriendSearchRequest(HSteamNetConnection connection, const FriendSearchRequest* request);
    static void HandleFriendAddRequest(HSteamNetConnection connection, const FriendAddRequest* request);
    static void HandleFriendAcceptRequest(HSteamNetConnection connection, const FriendAcceptRequest* request);
    static void HandleFriendRejectRequest(HSteamNetConnection connection, const FriendRejectRequest* request);
    static void HandleFriendListRequest(HSteamNetConnection connection);
    static void HandleFriendRemoveRequest(HSteamNetConnection connection, const FriendRemoveRequest* request);
    static void HandleFriendSentRequests(HSteamNetConnection connection);

    // Room handlers
    static void HandleRoomCreateRequest(HSteamNetConnection connection, const RoomCreateRequest* request);
    static void HandleRoomInviteRequest(HSteamNetConnection connection, const RoomInviteRequest* request);
    static void HandleRoomJoinRequest(HSteamNetConnection connection, const RoomJoinRequest* request);
    static void HandleRoomLeaveRequest(HSteamNetConnection connection, const RoomLeaveRequest* request);
    static void HandleRoomListRequest(HSteamNetConnection connection);

    // Helpers
    static HSteamNetConnection FindConnectionByUserId(int32_t userId);
    static void SendMessageToUser(int32_t userId, const void* data, uint32 size);
    static void NotifyFriendsStatusChange(int32_t userId, bool isOnline);
    static void HandleUserDisconnect(int32_t userId);

    // Session helpers
    static int32_t GetUserIdFromConnection(HSteamNetConnection connection);

public:
    static Server* Instance;
    bool  StartServer(uint16 port);
    void PollIncomingMessages();
    void PollConnectionStateChanges();
    uint64 GetSentBytes();
    uint64 GetRecievedBytes();
    bool ResetCounters();

    size_t GetConnectedUserCount() const;
    std::vector<UserInfo> GetConnectedUsers() const;
    std::vector<ChannelInfo> GetActiveChannels() const;
    std::vector<RoomSummary> GetAllRooms() const;
    std::vector<LogEntry> GetRecentLogEntries() const;
    static void AddLogEntry(const std::string& message);

    ~Server();
};


#endif //VOICECHATSERVER_SERVER_H
