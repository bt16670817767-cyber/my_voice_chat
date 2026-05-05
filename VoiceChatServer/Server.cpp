//
// Created by Amin on 10/15/23.
//
#include "Server.h"
#include "Auth/UserRepository.h"
#include "Auth/PasswordHasher.h"
#include "Auth/FriendRepository.h"
#include "RoomManager.h"
#include <cstring>

namespace {
UserRepository g_userRepository("data/users.sqlite");
}

Server* Server::Instance = nullptr;
SteamNetworkingMicroseconds Server::g_logTimeZero;
ISteamNetworkingSockets* Server::steamNetworking;
HSteamNetPollGroup Server::connectionPollGroup;
std::map<int64, std::set<HSteamNetConnection>> Server::channelToConnnectionsMap;
std::map<HSteamNetConnection, Server::SessionInfo> Server::connectionSessions;
std::mutex Server::sessionMutex;
std::mutex Server::channelMutex;
std::unordered_set<int32_t> Server::onlineUsers;
std::mutex Server::onlineUsersMutex;
FriendRepository* Server::friendRepo = nullptr;
RoomManager Server::roomManager;
std::vector<LogEntry> Server::eventLog;
std::mutex Server::eventLogMutex;


bool Server::StartServer(uint16 port) {
    Instance = this;

    // Create client and server sockets
    InitSteamDatagramConnectionSockets();

    steamNetworking = SteamNetworkingSockets();
    if (!g_userRepository.EnsureDefaultData()) {
        printf("Failed to initialize users database file.\n");
        return false;
    }

    friendRepo = new FriendRepository("data/users.sqlite");
    if (!friendRepo->EnsureTables()) {
        printf("Failed to initialize friendships database.\n");
        return false;
    }

    connectionPollGroup = steamNetworking->CreatePollGroup();

    if (steamNetworking == nullptr){
        printf("steam networking is null");
    }

    SteamNetworkingIPAddr addr;
    addr.Clear();
    addr.m_port = port;

    SteamNetworkingConfigValue_t options;
    options.SetPtr( k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*) OnSteamNetConnectionStatusChanged );
    socket = steamNetworking->CreateListenSocketIP(addr, 1, &options);

    if ( socket == k_HSteamListenSocket_Invalid ) {
        printf("Failed to listen on port %d", port);
        return false;
    }
    if ( socket == k_HSteamNetPollGroup_Invalid ) {
        printf("Failed to listen on port %d", port);
        return false;
    }
    printf( "Server listening on port %d\n", port );


    return true;
}

Server::~Server() {
    steamNetworking->DestroyPollGroup( connectionPollGroup );
    steamNetworking = nullptr;
    printf("server cleaned \n");
}

void Server::DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg )
{
    if (eType >= k_ESteamNetworkingSocketsDebugOutputType_Warning) {
        fprintf(stderr, "SteamNetworking warning: %s\n", pszMsg);
        fflush(stderr);
    }
}

void Server::InitSteamDatagramConnectionSockets()
{
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
    SteamDatagramErrMsg errMsg;
    if ( !GameNetworkingSockets_Init( nullptr, errMsg ) )
        printf( "GameNetworkingSockets_Init failed.  %s", errMsg );
#else
    SteamDatagram_SetAppID( 570 ); // Just set something, doesn't matter what
		SteamDatagram_SetUniverse( false, k_EUniverseDev );

		SteamDatagramErrMsg errMsg;
		if ( !SteamDatagramClient_Init( errMsg ) )
			FatalError( "SteamDatagramClient_Init failed.  %s", errMsg );

		// Disable authentication when running with Steam, for this
		// example, since we're not a real app.
		//
		// Authentication is disabled automatically in the open-source
		// version since we don't have a trusted third party to issue
		// certs.
		SteamNetworkingUtils()->SetGlobalConfigValueInt32( k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1 );
#endif

    g_logTimeZero = SteamNetworkingUtils()->GetLocalTimestamp();

    SteamNetworkingUtils()->SetDebugOutputFunction( k_ESteamNetworkingSocketsDebugOutputType_Warning, DebugOutput );
}

void Server::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo) {
    // What's the state of the connection?
    switch ( pInfo->m_info.m_eState )
    {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            const int64 channel = pInfo->m_info.m_nUserData;
            if (channel != 0) {
                RemoveConnectionFromChannel(pInfo->m_hConn, channel);
            }

            {
                int32_t userId = -1;
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(sessionMutex);
                    auto it = connectionSessions.find(pInfo->m_hConn);
                    if (it != connectionSessions.end() && it->second.authenticated) {
                        userId = it->second.userId;
                        username = it->second.username;
                    }
                    connectionSessions.erase(pInfo->m_hConn);
                }
                if (userId >= 0) {
                    AddLogEntry("User disconnected: " + username + " (ID:" + std::to_string(userId) + ")");
                    HandleUserDisconnect(userId);
                }
            }

            steamNetworking->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
        {
            // This must be a new connection


            // A client is attempting to connect
            // Try to accept the connection.
            if ( steamNetworking->AcceptConnection( pInfo->m_hConn ) != k_EResultOK )
            {
                // This could fail.  If the remote host tried to connect, but then
                // disconnected, the connection may already be half closed.  Just
                // destroy whatever we have on our side.
                steamNetworking->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
                break;
            }

            // Assign the poll group
            if ( !steamNetworking->SetConnectionPollGroup( pInfo->m_hConn, connectionPollGroup ) )
            {
                steamNetworking->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
                break;
            }

            {
                std::lock_guard<std::mutex> lock(sessionMutex);
                connectionSessions[pInfo->m_hConn] = SessionInfo{};
            }
            break;
        }

        case k_ESteamNetworkingConnectionState_Connected:
            AddLogEntry("New client connected");
            break;

        default:
            // Silences -Wswitch
            break;
    }
}

void Server::RemoveConnectionFromChannel(HSteamNetConnection connection, int64 channel) {
    std::lock_guard<std::mutex> lock(channelMutex);
    auto channelIt = channelToConnnectionsMap.find(channel);
    if (channelIt == channelToConnnectionsMap.end()) {
        return;
    }
    channelIt->second.erase(connection);
    if (channelIt->second.empty()) {
        channelToConnnectionsMap.erase(channelIt);
    }
}

bool Server::IsAuthenticated(HSteamNetConnection connection) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto sessionIt = connectionSessions.find(connection);
    return sessionIt != connectionSessions.end() && sessionIt->second.authenticated;
}

void Server::SendServerError(HSteamNetConnection connection, int32_t errorCode, const char* message) {
    ServerErrorMessage errorMessage;
    errorMessage.errorCode = errorCode;
    std::snprintf(errorMessage.message, sizeof(errorMessage.message), "%s", message);
    steamNetworking->SendMessageToConnection(
        connection,
        &errorMessage,
        sizeof(errorMessage),
        k_nSteamNetworkingSend_ReliableNoNagle,
        nullptr
    );
}

void Server::HandleRegisterRequest(HSteamNetConnection connection, const RegisterRequest* request, uint32 messageSize) {
    if (messageSize < sizeof(RegisterRequest) || request == nullptr) {
        SendServerError(connection, ERR_INVALID_REQUEST, "invalid register request");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        auto sessionIt = connectionSessions.find(connection);
        if (sessionIt != connectionSessions.end() && sessionIt->second.authenticated) {
            SendServerError(connection, ERR_ALREADY_AUTHENTICATED, "already logged in; logout first to register");
            return;
        }
    }

    const std::string username(request->username);
    const std::string password(request->password);
    const std::string displayName(request->displayName);

    if (username.empty() || password.empty()) {
        RegisterResponse resp;
        resp.success = 0;
        resp.errorCode = ERR_INVALID_REQUEST;
        std::snprintf(resp.message, sizeof(resp.message), "%s", "username and password are required");
        steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
        return;
    }

    if (g_userRepository.UsernameExists(username)) {
        RegisterResponse resp;
        resp.success = 0;
        resp.errorCode = ERR_USERNAME_TAKEN;
        std::snprintf(resp.message, sizeof(resp.message), "%s", "username already taken");
        steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
        return;
    }

    const std::string passwordHash = PasswordHasher::HashPassword(password);
    if (passwordHash.empty()) {
        RegisterResponse resp;
        resp.success = 0;
        resp.errorCode = ERR_INVALID_REQUEST;
        std::snprintf(resp.message, sizeof(resp.message), "%s", "failed to hash password");
        steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
        return;
    }

    const auto user = g_userRepository.CreateUser(username, passwordHash, displayName.empty() ? username : displayName);
    if (!user.has_value() || user->id < 0) {
        RegisterResponse resp;
        resp.success = 0;
        resp.errorCode = ERR_INVALID_REQUEST;
        std::snprintf(resp.message, sizeof(resp.message), "%s", "failed to create user");
        steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
        return;
    }

    RegisterResponse resp;
    resp.success = 1;
    resp.errorCode = ERR_NONE;
    resp.userId = user->id;
    std::snprintf(resp.message, sizeof(resp.message), "%s", "registration successful; please login");

    AddLogEntry("New user registered: " + username + " (ID:" + std::to_string(user->id) + ")");

    steamNetworking->SendMessageToConnection(
        connection,
        &resp,
        sizeof(resp),
        k_nSteamNetworkingSend_ReliableNoNagle,
        nullptr
    );
}

void Server::HandleLoginRequest(HSteamNetConnection connection, const LoginRequest* request, uint32 messageSize) {
    if (messageSize < sizeof(LoginRequest) || request == nullptr) {
        SendServerError(connection, ERR_INVALID_REQUEST, "invalid login request");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        auto sessionIt = connectionSessions.find(connection);
        if (sessionIt != connectionSessions.end() && sessionIt->second.authenticated) {
            SendServerError(connection, ERR_ALREADY_AUTHENTICATED, "already authenticated");
            return;
        }
    }

    LoginResponse response;
    const auto user = g_userRepository.FindByUsername(request->username);
    if (user.has_value() && user->status == 1 && PasswordHasher::VerifyPassword(request->password, user->passwordHash)) {
        response.success = 1;
        response.errorCode = ERR_NONE;
        response.userId = user->id;
        std::snprintf(response.displayName, sizeof(response.displayName), "%s", user->displayName.c_str());
        std::snprintf(response.message, sizeof(response.message), "%s", "login success");

        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            SessionInfo& session = connectionSessions[connection];
            session.authenticated = true;
            session.userId = response.userId;
            session.username = request->username;
            session.displayName = response.displayName;
        }

        {
            std::lock_guard<std::mutex> ol(onlineUsersMutex);
            onlineUsers.insert(response.userId);
        }
        NotifyFriendsStatusChange(response.userId, true);
        AddLogEntry("User logged in: " + std::string(request->username) + " (ID:" + std::to_string(response.userId) + ")");
    } else {
        response.success = 0;
        response.errorCode = ERR_UNAUTHENTICATED;
        std::snprintf(response.message, sizeof(response.message), "%s", "invalid username or password");
        AddLogEntry("Failed login attempt for user: " + std::string(request->username));
    }

    steamNetworking->SendMessageToConnection(
        connection,
        &response,
        sizeof(response),
        k_nSteamNetworkingSend_ReliableNoNagle,
        nullptr
    );
}

void Server::PollIncomingMessages() {
    SetChannel* setChannel;
    int64 channel;
    ResetCounters();
    while ( 1)
    {
        ISteamNetworkingMessage *pIncomingMsg = nullptr;
        int numMsgs = steamNetworking->ReceiveMessagesOnPollGroup( connectionPollGroup, &pIncomingMsg, 1 );
        if ( numMsgs == 0 )
            break;
        if ( numMsgs < 0 )
            printf( "Error checking for messages" );
        assert( numMsgs == 1 && pIncomingMsg );

        if (pIncomingMsg->m_pData == NULL){
            printf("received null data!\n");
            pIncomingMsg->Release();
            continue;
        }

        uint8_t messageType = ((uint8_t*)pIncomingMsg->m_pData)[0];
        receivedBytesCount += pIncomingMsg->GetSize();

        //printf("message type is %d \n", messageType);


        switch (messageType)
        {
            case LOGIN_REQ:
                HandleLoginRequest(pIncomingMsg->m_conn, static_cast<LoginRequest*>(pIncomingMsg->m_pData), pIncomingMsg->GetSize());
                break;
            case REGISTER_REQ:
                HandleRegisterRequest(pIncomingMsg->m_conn, static_cast<RegisterRequest*>(pIncomingMsg->m_pData), pIncomingMsg->GetSize());
                break;
            case FRIEND_SEARCH_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleFriendSearchRequest(pIncomingMsg->m_conn, static_cast<FriendSearchRequest*>(pIncomingMsg->m_pData));
                break;
            case FRIEND_ADD_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleFriendAddRequest(pIncomingMsg->m_conn, static_cast<FriendAddRequest*>(pIncomingMsg->m_pData));
                break;
            case FRIEND_ACCEPT_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleFriendAcceptRequest(pIncomingMsg->m_conn, static_cast<FriendAcceptRequest*>(pIncomingMsg->m_pData));
                break;
            case FRIEND_REJECT_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleFriendRejectRequest(pIncomingMsg->m_conn, static_cast<FriendRejectRequest*>(pIncomingMsg->m_pData));
                break;
            case FRIEND_LIST_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleFriendListRequest(pIncomingMsg->m_conn);
                break;
            case FRIEND_REMOVE_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleFriendRemoveRequest(pIncomingMsg->m_conn, static_cast<FriendRemoveRequest*>(pIncomingMsg->m_pData));
                break;
            case FRIEND_SENT_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleFriendSentRequests(pIncomingMsg->m_conn);
                break;
            case ROOM_CREATE_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleRoomCreateRequest(pIncomingMsg->m_conn, static_cast<RoomCreateRequest*>(pIncomingMsg->m_pData));
                break;
            case ROOM_INVITE_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleRoomInviteRequest(pIncomingMsg->m_conn, static_cast<RoomInviteRequest*>(pIncomingMsg->m_pData));
                break;
            case ROOM_JOIN_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleRoomJoinRequest(pIncomingMsg->m_conn, static_cast<RoomJoinRequest*>(pIncomingMsg->m_pData));
                break;
            case ROOM_LEAVE_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleRoomLeaveRequest(pIncomingMsg->m_conn, static_cast<RoomLeaveRequest*>(pIncomingMsg->m_pData));
                break;
            case ROOM_LIST_REQ:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) { SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required"); break; }
                HandleRoomListRequest(pIncomingMsg->m_conn);
                break;
            case SET_CHANNEL:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) {
                    SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required before setting channel");
                    break;
                }
                setChannel = (SetChannel*) pIncomingMsg->m_pData;
                {
                    const int64 oldChannel = pIncomingMsg->GetConnectionUserData();
                    if (oldChannel != 0 && oldChannel != setChannel->channel) {
                        RemoveConnectionFromChannel(pIncomingMsg->m_conn, oldChannel);
                    }
                }
                steamNetworking->SetConnectionUserData(pIncomingMsg->m_conn, setChannel->channel);
                {
                    std::lock_guard<std::mutex> lock(channelMutex);
                    if (channelToConnnectionsMap.find(setChannel->channel) == channelToConnnectionsMap.end()) {
                        channelToConnnectionsMap[setChannel->channel] = { pIncomingMsg->m_conn };
                    }
                    else {
                        channelToConnnectionsMap[setChannel->channel].insert(pIncomingMsg->m_conn);
                    }
                }
                break;
            case AUDIO:
                if (!IsAuthenticated(pIncomingMsg->m_conn)) {
                    SendServerError(pIncomingMsg->m_conn, ERR_UNAUTHENTICATED, "login required before audio stream");
                    break;
                }
                {
                    int32_t senderUserId = GetUserIdFromConnection(pIncomingMsg->m_conn);
                    if (senderUserId >= 0 && pIncomingMsg->GetSize() >= sizeof(AudioData)) {
                        auto* audioData = static_cast<AudioData*>(pIncomingMsg->m_pData);
                        audioData->senderUserId = senderUserId;
                    }
                }
                channel = pIncomingMsg->GetConnectionUserData();
                {
                    std::lock_guard<std::mutex> lock(channelMutex);
                    if (channelToConnnectionsMap.find(channel) != channelToConnnectionsMap.end()) {
                        for (auto it = channelToConnnectionsMap[channel].begin(); it != channelToConnnectionsMap[channel].end(); ++it) {
                        if (*it == pIncomingMsg->m_conn)
                        {
                            continue;
                        }
                        steamNetworking->SendMessageToConnection(*it, pIncomingMsg->m_pData, pIncomingMsg->GetSize(),
                            k_nSteamNetworkingSend_ReliableNoNagle,
                            nullptr);
                        sentBytesCount += pIncomingMsg->GetSize();
                        }
                    }
                }
                break;
            default:
                break;
        }

        pIncomingMsg->Release();
    }
}

void Server::PollConnectionStateChanges() {
    steamNetworking->RunCallbacks();
}

uint64 Server::GetSentBytes()
{
    return sentBytesCount;
}

uint64 Server::GetRecievedBytes()
{
    return receivedBytesCount;
}

bool Server::ResetCounters()
{
    sentBytesCount = 0;
    receivedBytesCount = 0;
    return true;
}

size_t Server::GetConnectedUserCount() const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return connectionSessions.size();
}

std::vector<UserInfo> Server::GetConnectedUsers() const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    std::vector<UserInfo> result;
    for (const auto& pair : connectionSessions) {
        UserInfo info;
        info.userId = pair.second.userId;
        info.username = pair.second.username;
        info.displayName = pair.second.displayName;
        info.activeRoomCount = static_cast<int32_t>(pair.second.activeRoomIds.size());
        result.push_back(info);
    }
    return result;
}

std::vector<ChannelInfo> Server::GetActiveChannels() const {
    std::lock_guard<std::mutex> lock(channelMutex);
    std::vector<ChannelInfo> result;
    for (const auto& pair : channelToConnnectionsMap) {
        ChannelInfo info;
        info.channelId = pair.first;
        info.connectionCount = pair.second.size();
        result.push_back(info);
    }
    return result;
}

std::vector<RoomSummary> Server::GetAllRooms() const {
    auto rooms = roomManager.GetAllRooms();
    std::vector<RoomSummary> result;
    for (const auto& room : rooms) {
        RoomSummary summary;
        summary.roomId = room.roomId;
        summary.roomName = room.roomName;
        summary.memberCount = room.memberUserIds.size();
        summary.ownerUserId = room.ownerUserId;
        result.push_back(summary);
    }
    return result;
}

std::vector<LogEntry> Server::GetRecentLogEntries() const {
    std::lock_guard<std::mutex> lock(eventLogMutex);
    return eventLog;
}

void Server::AddLogEntry(const std::string& message) {
    std::lock_guard<std::mutex> lock(eventLogMutex);
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", std::localtime(&time_t));
    eventLog.push_back({timeBuf, message});
    if (eventLog.size() > MAX_LOG_ENTRIES) {
        eventLog.erase(eventLog.begin(), eventLog.begin() + (eventLog.size() - MAX_LOG_ENTRIES));
    }
}

// --- Friend handlers ---

void Server::HandleFriendSearchRequest(HSteamNetConnection connection, const FriendSearchRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    const auto results = friendRepo->SearchUsers(request->query, userId, MAX_SEARCH_RESULTS);

    FriendSearchResult resp;
    resp.resultCount = static_cast<uint8_t>(results.size());
    for (size_t i = 0; i < results.size() && i < MAX_SEARCH_RESULTS; ++i) {
        resp.userIds[i] = results[i].userId;
        std::snprintf(resp.usernames[i], sizeof(resp.usernames[i]), "%s", results[i].username.c_str());
        std::snprintf(resp.displayNames[i], sizeof(resp.displayNames[i]), "%s", results[i].displayName.c_str());
        resp.relationshipStatus[i] = static_cast<uint8_t>(results[i].relationshipStatus);
        {
            std::lock_guard<std::mutex> lock(onlineUsersMutex);
            resp.isOnline[i] = onlineUsers.count(results[i].userId) > 0 ? 1 : 0;
        }
    }

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
}

void Server::HandleFriendAddRequest(HSteamNetConnection connection, const FriendAddRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    int32_t targetId = request->targetUserId;
    if (targetId < 0) {
        SendServerError(connection, ERR_USER_NOT_FOUND, "user not found");
        return;
    }
    if (targetId == userId) {
        SendServerError(connection, ERR_CANNOT_FRIEND_SELF, "cannot add yourself as friend");
        return;
    }

    int rel = friendRepo->GetRelationshipStatus(userId, targetId);
    if (rel == 1) {
        SendServerError(connection, ERR_ALREADY_FRIENDS, "already friends");
        return;
    }
    if (rel == 2) {
        SendServerError(connection, ERR_ALREADY_REQUESTED, "friend request already sent");
        return;
    }

    // If there's an incoming request from the target, auto-accept instead
    if (rel == 3) {
        friendRepo->AcceptFriendRequest(targetId, userId);
        FriendAcceptResponse resp;
        resp.success = 1;
        resp.friendUserId = targetId;
        steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);

        // Notify original requester
        HSteamNetConnection targetConn = FindConnectionByUserId(targetId);
        if (targetConn != k_HSteamNetConnection_Invalid) {
            FriendAcceptResponse notify;
            notify.success = 1;
            notify.friendUserId = userId;
            steamNetworking->SendMessageToConnection(targetConn, &notify, sizeof(notify), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
        }
        return;
    }

    // Verify target user exists
    auto targetUser = g_userRepository.FindByUsername(""); // need to check existence - use relationship check which already verifies user exists
    // Send friend request
    if (!friendRepo->SendFriendRequest(userId, targetId)) {
        SendServerError(connection, ERR_USER_NOT_FOUND, "failed to send friend request");
        return;
    }

    FriendAddResponse resp;
    resp.success = 1;
    resp.errorCode = ERR_NONE;
    resp.targetUserId = targetId;
    std::snprintf(resp.message, sizeof(resp.message), "%s", "friend request sent");

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);

    // Notify target user if online
    HSteamNetConnection targetConn = FindConnectionByUserId(targetId);
    if (targetConn != k_HSteamNetConnection_Invalid) {
        FriendRequestNotify notify;
        notify.fromUserId = userId;
        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            auto it = connectionSessions.find(connection);
            if (it != connectionSessions.end()) {
                std::snprintf(notify.fromUsername, sizeof(notify.fromUsername), "%s", it->second.username.c_str());
                std::snprintf(notify.fromDisplayName, sizeof(notify.fromDisplayName), "%s", it->second.displayName.c_str());
            }
        }
        steamNetworking->SendMessageToConnection(targetConn, &notify, sizeof(notify), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
    }
}

void Server::HandleFriendAcceptRequest(HSteamNetConnection connection, const FriendAcceptRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    int32_t fromUserId = request->fromUserId;
    if (fromUserId < 0) {
        SendServerError(connection, ERR_USER_NOT_FOUND, "user not found");
        return;
    }

    if (!friendRepo->AcceptFriendRequest(fromUserId, userId)) {
        SendServerError(connection, ERR_INVALID_REQUEST, "no pending friend request");
        return;
    }

    // Get the new friend's display name
    auto friendUser = g_userRepository.FindById(fromUserId);
    std::string friendDisplayName = friendUser.has_value() ? friendUser->displayName : "";

    FriendAcceptResponse resp;
    resp.success = 1;
    resp.friendUserId = fromUserId;
    std::snprintf(resp.friendDisplayName, sizeof(resp.friendDisplayName), "%s", friendDisplayName.c_str());
    std::snprintf(resp.message, sizeof(resp.message), "%s", "friend request accepted");

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);

    // Notify the original requester if online
    HSteamNetConnection requesterConn = FindConnectionByUserId(fromUserId);
    if (requesterConn != k_HSteamNetConnection_Invalid) {
        FriendAcceptResponse notify;
        notify.success = 1;
        notify.friendUserId = userId;
        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            auto it = connectionSessions.find(connection);
            if (it != connectionSessions.end()) {
                std::snprintf(notify.friendDisplayName, sizeof(notify.friendDisplayName), "%s", it->second.displayName.c_str());
            }
        }
        std::snprintf(notify.message, sizeof(notify.message), "%s", "friend request accepted");
        steamNetworking->SendMessageToConnection(requesterConn, &notify, sizeof(notify), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
    }
}

void Server::HandleFriendRejectRequest(HSteamNetConnection connection, const FriendRejectRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    friendRepo->RejectFriendRequest(request->fromUserId, userId);

    FriendRejectResponse resp;
    resp.success = 1;
    std::snprintf(resp.message, sizeof(resp.message), "%s", "friend request rejected");

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
}

void Server::HandleFriendListRequest(HSteamNetConnection connection) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    const auto friends = friendRepo->GetFriends(userId);

    FriendListResponse resp;
    resp.friendCount = static_cast<uint8_t>(friends.size() < MAX_FRIENDS_PER_MSG ? friends.size() : MAX_FRIENDS_PER_MSG);
    for (size_t i = 0; i < resp.friendCount; ++i) {
        resp.userIds[i] = friends[i].userId;
        std::snprintf(resp.usernames[i], sizeof(resp.usernames[i]), "%s", friends[i].username.c_str());
        std::snprintf(resp.displayNames[i], sizeof(resp.displayNames[i]), "%s", friends[i].displayName.c_str());
        {
            std::lock_guard<std::mutex> lock(onlineUsersMutex);
            resp.isOnline[i] = onlineUsers.count(friends[i].userId) > 0 ? 1 : 0;
        }
    }

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
}

void Server::HandleFriendRemoveRequest(HSteamNetConnection connection, const FriendRemoveRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    friendRepo->RemoveFriend(userId, request->friendUserId);

    FriendRemoveResponse resp;
    resp.success = 1;
    std::snprintf(resp.message, sizeof(resp.message), "%s", "friend removed");

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
}

void Server::HandleFriendSentRequests(HSteamNetConnection connection) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    const auto sent = friendRepo->GetSentRequests(userId);

    FriendSentResponse resp;
    resp.count = static_cast<uint8_t>(sent.size() < MAX_SENT_REQUESTS ? sent.size() : MAX_SENT_REQUESTS);
    for (size_t i = 0; i < resp.count; ++i) {
        resp.userIds[i] = sent[i].userId;
        std::snprintf(resp.usernames[i], sizeof(resp.usernames[i]), "%s", sent[i].username.c_str());
        std::snprintf(resp.displayNames[i], sizeof(resp.displayNames[i]), "%s", sent[i].displayName.c_str());
    }

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
}

// --- Helpers ---

int32_t Server::GetUserIdFromConnection(HSteamNetConnection connection) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = connectionSessions.find(connection);
    if (it == connectionSessions.end() || !it->second.authenticated) return -1;
    return it->second.userId;
}

HSteamNetConnection Server::FindConnectionByUserId(int32_t userId) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    for (const auto& pair : connectionSessions) {
        if (pair.second.authenticated && pair.second.userId == userId) {
            return pair.first;
        }
    }
    return k_HSteamNetConnection_Invalid;
}

void Server::SendMessageToUser(int32_t userId, const void* data, uint32 size) {
    HSteamNetConnection conn = FindConnectionByUserId(userId);
    if (conn != k_HSteamNetConnection_Invalid) {
        steamNetworking->SendMessageToConnection(conn, data, size, k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
    }
}

void Server::NotifyFriendsStatusChange(int32_t userId, bool isOnline) {
    const auto friendIds = friendRepo->GetFriendIds(userId);

    FriendOnlineNotify notify;
    notify.userId = userId;
    notify.isOnline = isOnline ? 1 : 0;

    for (int32_t friendId : friendIds) {
        SendMessageToUser(friendId, &notify, sizeof(notify));
    }
}

void Server::HandleUserDisconnect(int32_t userId) {
    // Remove from all rooms
    auto activeRoomIds = roomManager.GetUserActiveRoomIds(userId);
    for (int32_t roomId : activeRoomIds) {
        roomManager.RemoveMember(roomId, userId);

        // Notify other members
        RoomMemberUpdate update;
        update.roomId = roomId;
        const auto* room = roomManager.GetRoom(roomId);
        if (room) {
            update.memberCount = static_cast<uint8_t>(room->memberUserIds.size() < MAX_ROOM_MEMBERS ? room->memberUserIds.size() : MAX_ROOM_MEMBERS);
            int idx = 0;
            for (int32_t memberId : room->memberUserIds) {
                if (idx >= MAX_ROOM_MEMBERS) break;
                update.userIds[idx] = memberId;
                // Get display name from session
                HSteamNetConnection memberConn = FindConnectionByUserId(memberId);
                if (memberConn != k_HSteamNetConnection_Invalid) {
                    std::lock_guard<std::mutex> lock(sessionMutex);
                    auto it = connectionSessions.find(memberConn);
                    if (it != connectionSessions.end()) {
                        std::snprintf(update.displayNames[idx], sizeof(update.displayNames[idx]), "%s", it->second.displayName.c_str());
                    }
                }
                ++idx;
            }
            // Broadcast update to remaining members
            for (int32_t memberId : room->memberUserIds) {
                SendMessageToUser(memberId, &update, sizeof(update));
            }

            // Delete room if empty
            if (room->memberUserIds.empty()) {
                roomManager.DeleteRoom(roomId);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(onlineUsersMutex);
        onlineUsers.erase(userId);
    }
    NotifyFriendsStatusChange(userId, false);
}

// --- Room handlers ---

void Server::HandleRoomCreateRequest(HSteamNetConnection connection, const RoomCreateRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    std::string roomName(request->roomName);
    std::string password(request->password);
    int32_t roomId = roomManager.CreateRoom(userId, roomName, password);
    const auto* room = roomManager.GetRoom(roomId);
    if (!room) {
        SendServerError(connection, ERR_INVALID_REQUEST, "failed to create room");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        auto it = connectionSessions.find(connection);
        if (it != connectionSessions.end()) {
            it->second.activeRoomIds.insert(roomId);
        }
    }

    // Auto-join the channel for this room
    steamNetworking->SetConnectionUserData(connection, room->channel);
    {
        std::lock_guard<std::mutex> lock(channelMutex);
        channelToConnnectionsMap[room->channel] = { connection };
    }

    RoomCreateResponse resp;
    resp.success = 1;
    resp.roomId = roomId;
    resp.channel = room->channel;
    std::snprintf(resp.roomName, sizeof(resp.roomName), "%s", room->roomName.c_str());
    std::snprintf(resp.message, sizeof(resp.message), "%s", "room created");

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
}

void Server::HandleRoomInviteRequest(HSteamNetConnection connection, const RoomInviteRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    const auto* room = roomManager.GetRoom(request->roomId);
    if (!room) {
        SendServerError(connection, ERR_ROOM_NOT_FOUND, "room not found");
        return;
    }
    if (!roomManager.IsMember(request->roomId, userId)) {
        SendServerError(connection, ERR_NOT_ROOM_MEMBER, "you are not a member of this room");
        return;
    }

    roomManager.AddInvite(request->roomId, request->targetUserId);

    RoomInviteResponse resp;
    resp.success = 1;
    std::snprintf(resp.message, sizeof(resp.message), "%s", "invitation sent");
    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);

    // Notify target user
    HSteamNetConnection targetConn = FindConnectionByUserId(request->targetUserId);
    if (targetConn != k_HSteamNetConnection_Invalid) {
        RoomInviteNotify notify;
        notify.roomId = request->roomId;
        notify.channel = room->channel;
        notify.fromUserId = userId;
        std::snprintf(notify.roomName, sizeof(notify.roomName), "%s", room->roomName.c_str());
        {
            std::lock_guard<std::mutex> lock(sessionMutex);
            auto it = connectionSessions.find(connection);
            if (it != connectionSessions.end()) {
                std::snprintf(notify.fromDisplayName, sizeof(notify.fromDisplayName), "%s", it->second.displayName.c_str());
            }
        }
        steamNetworking->SendMessageToConnection(targetConn, &notify, sizeof(notify), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
    }
}

void Server::HandleRoomJoinRequest(HSteamNetConnection connection, const RoomJoinRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    const auto* room = roomManager.GetRoom(request->roomId);
    if (!room) {
        SendServerError(connection, ERR_ROOM_NOT_FOUND, "room not found");
        return;
    }

    if (roomManager.IsMember(request->roomId, userId)) {
        SendServerError(connection, ERR_ALREADY_AUTHENTICATED, "already a member of this room");
        return;
    }

    // Allow joining by invitation OR by correct password
    const bool invited = roomManager.IsInvited(request->roomId, userId);
    const bool hasPassword = !room->password.empty();
    const bool passwordMatch = hasPassword && (std::string(request->password) == room->password);

    if (!invited && !passwordMatch) {
        if (hasPassword) {
            SendServerError(connection, ERR_NOT_INVITED, "invalid room password");
        } else {
            SendServerError(connection, ERR_NOT_INVITED, "you have not been invited to this room");
        }
        return;
    }

    // Add as member
    roomManager.AddMember(request->roomId, userId);
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        auto it = connectionSessions.find(connection);
        if (it != connectionSessions.end()) {
            it->second.activeRoomIds.insert(request->roomId);
        }
    }

    // Switch to room's audio channel
    const int64 oldChannel = steamNetworking->GetConnectionUserData(connection);
    if (oldChannel != 0 && oldChannel != room->channel) {
        RemoveConnectionFromChannel(connection, oldChannel);
    }
    steamNetworking->SetConnectionUserData(connection, room->channel);
    {
        std::lock_guard<std::mutex> lock(channelMutex);
        channelToConnnectionsMap[room->channel].insert(connection);
    }

    RoomJoinResponse resp;
    resp.success = 1;
    resp.roomId = request->roomId;
    resp.channel = room->channel;
    std::snprintf(resp.roomName, sizeof(resp.roomName), "%s", room->roomName.c_str());
    std::snprintf(resp.message, sizeof(resp.message), "%s", "joined room");

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);

    // Notify other members
    const auto* updatedRoom = roomManager.GetRoom(request->roomId);
    if (updatedRoom) {
        RoomMemberUpdate update;
        update.roomId = request->roomId;
        update.roomChannel = room->channel;
        update.memberCount = static_cast<uint8_t>(updatedRoom->memberUserIds.size() < MAX_ROOM_MEMBERS ? updatedRoom->memberUserIds.size() : MAX_ROOM_MEMBERS);
        int idx = 0;
        for (int32_t memberId : updatedRoom->memberUserIds) {
            if (idx >= MAX_ROOM_MEMBERS) break;
            update.userIds[idx] = memberId;
            HSteamNetConnection memberConn = FindConnectionByUserId(memberId);
            if (memberConn != k_HSteamNetConnection_Invalid) {
                std::lock_guard<std::mutex> lock(sessionMutex);
                auto it = connectionSessions.find(memberConn);
                if (it != connectionSessions.end()) {
                    std::snprintf(update.displayNames[idx], sizeof(update.displayNames[idx]), "%s", it->second.displayName.c_str());
                }
            }
            ++idx;
        }
        for (int32_t memberId : updatedRoom->memberUserIds) {
            SendMessageToUser(memberId, &update, sizeof(update));
        }
    }
}

void Server::HandleRoomLeaveRequest(HSteamNetConnection connection, const RoomLeaveRequest* request) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    const auto* room = roomManager.GetRoom(request->roomId);
    if (!room || !roomManager.IsMember(request->roomId, userId)) {
        SendServerError(connection, ERR_NOT_ROOM_MEMBER, "not a member of this room");
        return;
    }

    // Clear channel
    const int64 oldChannel = steamNetworking->GetConnectionUserData(connection);
    if (oldChannel != 0) {
        RemoveConnectionFromChannel(connection, oldChannel);
    }
    steamNetworking->SetConnectionUserData(connection, 0);

    roomManager.RemoveMember(request->roomId, userId);
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        auto it = connectionSessions.find(connection);
        if (it != connectionSessions.end()) {
            it->second.activeRoomIds.erase(request->roomId);
        }
    }

    RoomLeaveResponse resp;
    resp.success = 1;
    std::snprintf(resp.message, sizeof(resp.message), "%s", "left room");
    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);

    // Notify remaining members
    const auto* updatedRoom = roomManager.GetRoom(request->roomId);
    if (updatedRoom && !updatedRoom->memberUserIds.empty()) {
        RoomMemberUpdate update;
        update.roomId = request->roomId;
        update.memberCount = static_cast<uint8_t>(updatedRoom->memberUserIds.size() < MAX_ROOM_MEMBERS ? updatedRoom->memberUserIds.size() : MAX_ROOM_MEMBERS);
        int idx = 0;
        for (int32_t memberId : updatedRoom->memberUserIds) {
            if (idx >= MAX_ROOM_MEMBERS) break;
            update.userIds[idx] = memberId;
            HSteamNetConnection memberConn = FindConnectionByUserId(memberId);
            if (memberConn != k_HSteamNetConnection_Invalid) {
                std::lock_guard<std::mutex> lock(sessionMutex);
                auto it = connectionSessions.find(memberConn);
                if (it != connectionSessions.end()) {
                    std::snprintf(update.displayNames[idx], sizeof(update.displayNames[idx]), "%s", it->second.displayName.c_str());
                }
            }
            ++idx;
        }
        for (int32_t memberId : updatedRoom->memberUserIds) {
            SendMessageToUser(memberId, &update, sizeof(update));
        }
    } else {
        // Delete empty room
        roomManager.DeleteRoom(request->roomId);
    }
}

void Server::HandleRoomListRequest(HSteamNetConnection connection) {
    int32_t userId = GetUserIdFromConnection(connection);
    if (userId < 0) return;

    const auto rooms = roomManager.GetRoomsForUser(userId);

    RoomListResponse resp;
    resp.roomCount = static_cast<uint8_t>(rooms.size() < MAX_ROOMS_PER_MSG ? rooms.size() : MAX_ROOMS_PER_MSG);
    for (size_t i = 0; i < resp.roomCount; ++i) {
        resp.roomIds[i] = rooms[i].roomId;
        resp.channels[i] = rooms[i].channel;
        std::snprintf(resp.roomNames[i], sizeof(resp.roomNames[i]), "%s", rooms[i].roomName.c_str());
        resp.ownerUserIds[i] = rooms[i].ownerUserId;
        resp.memberCounts[i] = static_cast<uint8_t>(rooms[i].memberUserIds.size());
        resp.isJoined[i] = rooms[i].memberUserIds.count(userId) > 0 ? 1 : 0;
        resp.isInvited[i] = rooms[i].invitedUserIds.count(userId) > 0 ? 1 : 0;
    }

    steamNetworking->SendMessageToConnection(connection, &resp, sizeof(resp), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
}
