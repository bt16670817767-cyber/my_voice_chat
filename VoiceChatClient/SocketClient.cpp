//
// Created by Amin on 10/17/23.
//

#include "SocketClient.h"
#include <algorithm>
#include <chrono>
#include <cstdio>

SteamNetworkingMicroseconds SocketClient::g_logTimeZero;
HSteamNetConnection SocketClient::connection = k_HSteamNetConnection_Invalid;
ISteamNetworkingSockets* SocketClient::steamNetworking;
std::atomic<bool> SocketClient::isConnected{false};
std::atomic<bool> SocketClient::isConnecting{false};
std::atomic<bool> SocketClient::isAuthenticated{false};
std::string SocketClient::statusMessage = "Idle";
std::string SocketClient::authMessage = "Not logged in.";
std::string SocketClient::displayName;
int32_t SocketClient::userId = -1;
std::mutex SocketClient::statusMessageMutex;
std::mutex SocketClient::authMutex;

std::vector<FriendEntryUi> SocketClient::friendList;
std::vector<SearchResultUi> SocketClient::searchResults;
std::vector<PendingRequestUi> SocketClient::pendingRequests;
std::vector<SentRequestUi> SocketClient::sentRequests;
std::vector<std::string> SocketClient::notificationQueue;
std::mutex SocketClient::friendMutex;
std::mutex SocketClient::notificationMutex;

std::vector<SpeakingUser> SocketClient::speakingUsers;
std::mutex SocketClient::speakingMutex;

std::atomic<bool> SocketClient::friendListNeedsRefresh{false};
std::atomic<bool> SocketClient::roomListNeedsRefresh{false};

std::vector<RoomEntryUi> SocketClient::roomList;
std::vector<RoomMemberUi> SocketClient::roomMembers;
int32_t SocketClient::selectedRoomId = -1;
std::mutex SocketClient::roomMutex;


void SocketClient::InitSteamDatagramConnectionSockets() {
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

    SteamNetworkingUtils()->SetDebugOutputFunction( k_ESteamNetworkingSocketsDebugOutputType_Msg, DebugOutput );
}

void SocketClient::DebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg) {
    SteamNetworkingMicroseconds time = SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    printf( "%10.6f %s\n", time*1e-6, pszMsg );
    fflush(stdout);
    if ( eType == k_ESteamNetworkingSocketsDebugOutputType_Bug )
    {
        fflush(stdout);
        fflush(stderr);
    }
}

void SocketClient::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo) {
    // Ignore callbacks for stale connections after disconnect+reconnect.
    // When connection is invalid (initial connect), accept any callback.
    if (pInfo->m_hConn != connection && connection != k_HSteamNetConnection_Invalid) {
        if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer ||
            pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
            steamNetworking->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
        }
        return;
    }

    // What's the state of the connection?
    switch ( pInfo->m_info.m_eState )
    {
        case k_ESteamNetworkingConnectionState_None:
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            printf("connection closed: %s\n", pInfo->m_info.m_szEndDebug);
            steamNetworking->CloseConnection( pInfo->m_hConn, 0, nullptr, false );
            connection = k_HSteamNetConnection_Invalid;
            isConnected.store(false);
            isConnecting.store(false);
            isAuthenticated.store(false);
            {
                std::lock_guard<std::mutex> lock(authMutex);
                userId = -1;
                displayName.clear();
                authMessage = "Not logged in.";
            }
            if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) {
                SetStatusMessage(std::string("Connect failed: ") + pInfo->m_info.m_szEndDebug);
            } else {
                SetStatusMessage(std::string("Disconnected: ") + pInfo->m_info.m_szEndDebug);
            }
            break;
        }

        case k_ESteamNetworkingConnectionState_Connecting:
            isConnecting.store(true);
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            printf( "Connected to server OK\n" );
            isConnected.store(true);
            isConnecting.store(false);
            isAuthenticated.store(false);
            SetAuthMessage("Connected. Please login.");
            SetStatusMessage("Connected to server.");
            break;

        default:
            // Silences -Wswitch
            break;
    }
}



void SocketClient::Disconnect() {
    if (connection != k_HSteamNetConnection_Invalid && steamNetworking != nullptr) {
        steamNetworking->CloseConnection(connection, 0, nullptr, false);
        connection = k_HSteamNetConnection_Invalid;
    }
    isConnected.store(false);
    isConnecting.store(false);
    isAuthenticated.store(false);
    {
        std::lock_guard<std::mutex> lock(authMutex);
        userId = -1;
        displayName.clear();
        authMessage.clear();
    }
    {
        std::lock_guard<std::mutex> lock(friendMutex);
        friendList.clear();
        searchResults.clear();
        pendingRequests.clear();
        sentRequests.clear();
    }
    {
        std::lock_guard<std::mutex> lock(roomMutex);
        roomList.clear();
        roomMembers.clear();
        selectedRoomId = -1;
    }
    {
        std::lock_guard<std::mutex> lock(notificationMutex);
        notificationQueue.clear();
    }
    {
        std::lock_guard<std::mutex> lock(speakingMutex);
        speakingUsers.clear();
    }
    SetStatusMessage("Disconnected.");
}

bool SocketClient::Connect(SteamNetworkingIPAddr add) {

    printf("trying to connect to server....");
    isConnected.store(false);
    isConnecting.store(true);
    SetStatusMessage("Connecting...");


    SteamNetworkingConfigValue_t options;
    options.SetPtr( k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)  OnSteamNetConnectionStatusChanged );

    connection = steamNetworking->ConnectByIPAddress(add, 1, &options );

    if ( connection == k_HSteamNetConnection_Invalid )
    {
        printf( "Failed to create connection" );
        isConnecting.store(false);
        SetStatusMessage("Failed to create connection.");
        return false;
    }

    return true;
}

int receiveCounter = 0;
void SocketClient::PollIncomingMessages(NetworkBuffer* _voiceAudioBuffer)
{
    if (connection == k_HSteamNetConnection_Invalid){
        printf("connection is invalid \n");
        return;
    }

    while ( 1 )
    {
        ISteamNetworkingMessage *pIncomingMsg = nullptr;
        int numMsgs = steamNetworking->ReceiveMessagesOnConnection(connection, &pIncomingMsg, 1 );
        if ( numMsgs == 0 )
            break;
        if (numMsgs == -1)
        {
            printf("connection handle is invalid \n");
            break;
        }


        if (pIncomingMsg->m_pData == nullptr || pIncomingMsg->GetSize() == 0) {
            pIncomingMsg->Release();
            continue;
        }

        const uint8_t messageType = static_cast<uint8_t*>(pIncomingMsg->m_pData)[0];

        if (messageType == REGISTER_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(RegisterResponse)) {
                auto* registerResponse = static_cast<RegisterResponse*>(pIncomingMsg->m_pData);
                if (registerResponse->success == 1) {
                    SetAuthMessage(registerResponse->message[0] != '\0' ? registerResponse->message : "Registration successful. Please login.");
                } else {
                    SetAuthMessage(registerResponse->message[0] != '\0' ? registerResponse->message : "Registration failed.");
                }
            } else {
                SetAuthMessage("Server returned malformed register response.");
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == LOGIN_RES) {
            if (pIncomingMsg->GetSize() < sizeof(LoginResponse)) {
                SetAuthMessage("Server returned malformed login response.");
                pIncomingMsg->Release();
                continue;
            }
            auto* loginResponse = static_cast<LoginResponse*>(pIncomingMsg->m_pData);
            if (loginResponse->success == 1) {
                isAuthenticated.store(true);
                {
                    std::lock_guard<std::mutex> lock(authMutex);
                    userId = loginResponse->userId;
                    displayName = loginResponse->displayName;
                }
                SetAuthMessage(loginResponse->message[0] != '\0' ? loginResponse->message : "Login success.");
            } else {
                isAuthenticated.store(false);
                {
                    std::lock_guard<std::mutex> lock(authMutex);
                    userId = -1;
                    displayName.clear();
                }
                SetAuthMessage(loginResponse->message[0] != '\0' ? loginResponse->message : "Login failed.");
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == SERVER_ERROR) {
            if (pIncomingMsg->GetSize() >= sizeof(ServerErrorMessage)) {
                auto* serverError = static_cast<ServerErrorMessage*>(pIncomingMsg->m_pData);
                SetStatusMessage(std::string("Server error: ") + serverError->message);
            } else {
                SetStatusMessage("Server returned malformed error message.");
            }
            pIncomingMsg->Release();
            continue;
        }

        // Friend messages
        if (messageType == FRIEND_SEARCH_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(FriendSearchResult)) {
                auto* result = static_cast<FriendSearchResult*>(pIncomingMsg->m_pData);
                std::lock_guard<std::mutex> lock(friendMutex);
                searchResults.clear();
                for (uint8_t i = 0; i < result->resultCount && i < MAX_SEARCH_RESULTS; ++i) {
                    SearchResultUi entry;
                    entry.userId = result->userIds[i];
                    entry.username = result->usernames[i];
                    entry.displayName = result->displayNames[i];
                    entry.relationshipStatus = result->relationshipStatus[i];
                    entry.isOnline = result->isOnline[i] != 0;
                    searchResults.push_back(entry);
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == FRIEND_ADD_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(FriendAddResponse)) {
                auto* resp = static_cast<FriendAddResponse*>(pIncomingMsg->m_pData);
                if (resp->success) {
                    SetStatusMessage("Friend request sent.");
                } else {
                    SetStatusMessage(resp->message);
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == FRIEND_ACCEPT_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(FriendAcceptResponse)) {
                auto* resp = static_cast<FriendAcceptResponse*>(pIncomingMsg->m_pData);
                if (resp->success) {
                    SetStatusMessage(std::string("Friend accepted: ") + resp->friendDisplayName);
                    friendListNeedsRefresh.store(true);
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == FRIEND_REJECT_RES) {
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == FRIEND_LIST_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(FriendListResponse)) {
                auto* resp = static_cast<FriendListResponse*>(pIncomingMsg->m_pData);
                std::lock_guard<std::mutex> lock(friendMutex);
                friendList.clear();
                for (uint8_t i = 0; i < resp->friendCount && i < MAX_FRIENDS_PER_MSG; ++i) {
                    FriendEntryUi entry;
                    entry.userId = resp->userIds[i];
                    entry.username = resp->usernames[i];
                    entry.displayName = resp->displayNames[i];
                    entry.isOnline = resp->isOnline[i] != 0;
                    friendList.push_back(entry);
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == FRIEND_REMOVE_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(FriendRemoveResponse)) {
                auto* resp = static_cast<FriendRemoveResponse*>(pIncomingMsg->m_pData);
                if (resp->success) {
                    SetStatusMessage("Friend removed.");
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == FRIEND_REQUEST_NOTIFY) {
            if (pIncomingMsg->GetSize() >= sizeof(FriendRequestNotify)) {
                auto* notify = static_cast<FriendRequestNotify*>(pIncomingMsg->m_pData);
                std::lock_guard<std::mutex> lock(friendMutex);
                PendingRequestUi entry;
                entry.userId = notify->fromUserId;
                entry.username = notify->fromUsername;
                entry.displayName = notify->fromDisplayName;
                pendingRequests.push_back(entry);
                {
                    std::lock_guard<std::mutex> nl(notificationMutex);
                    notificationQueue.push_back(std::string("Friend request from ") + notify->fromDisplayName);
                }
                friendListNeedsRefresh.store(true);
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == FRIEND_ONLINE_NOTIFY) {
            if (pIncomingMsg->GetSize() >= sizeof(FriendOnlineNotify)) {
                auto* notify = static_cast<FriendOnlineNotify*>(pIncomingMsg->m_pData);
                std::lock_guard<std::mutex> lock(friendMutex);
                for (auto& f : friendList) {
                    if (f.userId == notify->userId) {
                        f.isOnline = notify->isOnline != 0;
                        break;
                    }
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == FRIEND_SENT_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(FriendSentResponse)) {
                auto* resp = static_cast<FriendSentResponse*>(pIncomingMsg->m_pData);
                std::lock_guard<std::mutex> lock(friendMutex);
                sentRequests.clear();
                for (uint8_t i = 0; i < resp->count && i < MAX_SENT_REQUESTS; ++i) {
                    SentRequestUi entry;
                    entry.userId = resp->userIds[i];
                    entry.username = resp->usernames[i];
                    entry.displayName = resp->displayNames[i];
                    sentRequests.push_back(entry);
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        // Room messages
        if (messageType == ROOM_CREATE_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(RoomCreateResponse)) {
                auto* resp = static_cast<RoomCreateResponse*>(pIncomingMsg->m_pData);
                if (resp->success) {
                    SetStatusMessage(std::string("Room created: ") + resp->roomName);
                } else {
                    SetStatusMessage(resp->message);
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == ROOM_INVITE_RES) {
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == ROOM_INVITE_NOTIFY) {
            if (pIncomingMsg->GetSize() >= sizeof(RoomInviteNotify)) {
                auto* notify = static_cast<RoomInviteNotify*>(pIncomingMsg->m_pData);
                {
                    std::lock_guard<std::mutex> lock(notificationMutex);
                    notificationQueue.push_back(std::string("Invited to room: ") + notify->roomName + " by " + notify->fromDisplayName);
                }
                roomListNeedsRefresh.store(true);
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == ROOM_JOIN_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(RoomJoinResponse)) {
                auto* resp = static_cast<RoomJoinResponse*>(pIncomingMsg->m_pData);
                if (resp->success) {
                    SetStatusMessage(std::string("Joined room: ") + resp->roomName);
                    // Store the channel for UI to use for audio
                    {
                        std::lock_guard<std::mutex> lock(roomMutex);
                        selectedRoomId = resp->roomId;
                    }
                } else {
                    SetStatusMessage(resp->message);
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == ROOM_LEAVE_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(RoomLeaveResponse)) {
                auto* resp = static_cast<RoomLeaveResponse*>(pIncomingMsg->m_pData);
                if (resp->success) {
                    SetStatusMessage("Left room.");
                    std::lock_guard<std::mutex> lock(roomMutex);
                    selectedRoomId = -1;
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == ROOM_LIST_RES) {
            if (pIncomingMsg->GetSize() >= sizeof(RoomListResponse)) {
                auto* resp = static_cast<RoomListResponse*>(pIncomingMsg->m_pData);
                std::lock_guard<std::mutex> lock(roomMutex);
                roomList.clear();
                for (uint8_t i = 0; i < resp->roomCount && i < MAX_ROOMS_PER_MSG; ++i) {
                    RoomEntryUi entry;
                    entry.roomId = resp->roomIds[i];
                    entry.channel = resp->channels[i];
                    entry.roomName = resp->roomNames[i];
                    entry.ownerUserId = resp->ownerUserIds[i];
                    entry.memberCount = resp->memberCounts[i];
                    entry.isJoined = resp->isJoined[i] != 0;
                    entry.isInvited = resp->isInvited[i] != 0;
                    roomList.push_back(entry);
                }
            }
            pIncomingMsg->Release();
            continue;
        }

        if (messageType == ROOM_MEMBER_UPDATE) {
            if (pIncomingMsg->GetSize() >= sizeof(RoomMemberUpdate)) {
                auto* update = static_cast<RoomMemberUpdate*>(pIncomingMsg->m_pData);
                std::lock_guard<std::mutex> lock(roomMutex);
                roomMembers.clear();
                for (uint8_t i = 0; i < update->memberCount && i < MAX_ROOM_MEMBERS; ++i) {
                    RoomMemberUi member;
                    member.userId = update->userIds[i];
                    member.displayName = update->displayNames[i];
                    roomMembers.push_back(member);
                }
                roomListNeedsRefresh.store(true);
            }
            pIncomingMsg->Release();
            continue;
        }

        // Default: treat as audio data
        if (pIncomingMsg->GetSize() < sizeof(AudioData)) {
            pIncomingMsg->Release();
            continue;
        }
        auto* audioData = static_cast<AudioData*>(pIncomingMsg->m_pData);

        // Track who is speaking
        if (audioData->senderUserId > 0) {
            RecordAudioFromUser(audioData->senderUserId);
        }

        // Playback
        const size_t buffer_size = audioData->inputCurrentCounter;
        for (size_t i = 0; i < buffer_size; ++i) {
            if (audioData->Input[i] != 0)
                _voiceAudioBuffer->AddInput(audioData->Input[i]);
        }

        // We don't need this anymore.
        pIncomingMsg->Release();
    }
}

void SocketClient::PollConnectionStateChanges()
{
    steamNetworking->RunCallbacks();
}

SocketClient::SocketClient() {
    // Create client and server sockets
    InitSteamDatagramConnectionSockets();
    steamNetworking = SteamNetworkingSockets();
}

SocketClient::~SocketClient() {
    steamNetworking = nullptr;
}


void SocketClient::Send(const void *data, uint32 size) {
    if (connection == k_HSteamNetConnection_Invalid) {
        SetStatusMessage("Cannot send: no active connection.");
        return;
    }
    steamNetworking->SendMessageToConnection(connection, data,size,
                                             k_nSteamNetworkingSend_ReliableNoNagle,
                                             nullptr);
}

void SocketClient::SendLoginRequest(const char* username, const char* password) {
    LoginRequest request;
    std::snprintf(request.username, sizeof(request.username), "%s", username == nullptr ? "" : username);
    std::snprintf(request.password, sizeof(request.password), "%s", password == nullptr ? "" : password);
    Send(&request, sizeof(request));
    SetAuthMessage("Login request sent.");
}

void SocketClient::SendRegisterRequest(const char* username, const char* password, const char* displayName) {
    RegisterRequest request;
    std::snprintf(request.username, sizeof(request.username), "%s", username == nullptr ? "" : username);
    std::snprintf(request.password, sizeof(request.password), "%s", password == nullptr ? "" : password);
    std::snprintf(request.displayName, sizeof(request.displayName), "%s", displayName == nullptr ? "" : displayName);
    Send(&request, sizeof(request));
    SetAuthMessage("Register request sent.");
}

bool SocketClient::IsConnected() {
    return isConnected.load();
}

bool SocketClient::IsConnecting() {
    return isConnecting.load();
}

bool SocketClient::IsAuthenticated() {
    return isAuthenticated.load();
}

void SocketClient::SetStatusMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(statusMessageMutex);
    statusMessage = message;
}

std::string SocketClient::GetStatusMessage() {
    std::lock_guard<std::mutex> lock(statusMessageMutex);
    return statusMessage;
}

void SocketClient::SetAuthMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(authMutex);
    authMessage = message;
}

std::string SocketClient::GetAuthMessage() {
    std::lock_guard<std::mutex> lock(authMutex);
    return authMessage;
}

std::string SocketClient::GetDisplayName() {
    std::lock_guard<std::mutex> lock(authMutex);
    return displayName;
}

int32_t SocketClient::GetUserId() {
    std::lock_guard<std::mutex> lock(authMutex);
    return userId;
}

// --- Friend send methods ---

void SocketClient::SendFriendSearchRequest(const char* query) {
    FriendSearchRequest request;
    std::snprintf(request.query, sizeof(request.query), "%s", query == nullptr ? "" : query);
    Send(&request, sizeof(request));
}

void SocketClient::SendFriendAddRequest(int32_t targetUserId) {
    FriendAddRequest request;
    request.targetUserId = targetUserId;
    Send(&request, sizeof(request));
}

void SocketClient::SendFriendAcceptRequest(int32_t fromUserId) {
    FriendAcceptRequest request;
    request.fromUserId = fromUserId;
    Send(&request, sizeof(request));
}

void SocketClient::SendFriendRejectRequest(int32_t fromUserId) {
    FriendRejectRequest request;
    request.fromUserId = fromUserId;
    Send(&request, sizeof(request));
}

void SocketClient::SendFriendListRequest() {
    FriendListRequest request;
    Send(&request, sizeof(request));
}

void SocketClient::SendFriendRemoveRequest(int32_t friendUserId) {
    FriendRemoveRequest request;
    request.friendUserId = friendUserId;
    Send(&request, sizeof(request));
}

void SocketClient::SendFriendSentRequest() {
    FriendSentRequest request;
    Send(&request, sizeof(request));
}

// --- Room send methods ---

void SocketClient::SendRoomCreateRequest(const char* roomName, const char* password) {
    RoomCreateRequest request;
    std::snprintf(request.roomName, sizeof(request.roomName), "%s", roomName == nullptr ? "" : roomName);
    std::snprintf(request.password, sizeof(request.password), "%s", password == nullptr ? "" : password);
    Send(&request, sizeof(request));
}

void SocketClient::SendRoomInviteRequest(int32_t roomId, int32_t targetUserId) {
    RoomInviteRequest request;
    request.roomId = roomId;
    request.targetUserId = targetUserId;
    Send(&request, sizeof(request));
}

void SocketClient::SendRoomJoinRequest(int32_t roomId, const char* password) {
    RoomJoinRequest request;
    request.roomId = roomId;
    std::snprintf(request.password, sizeof(request.password), "%s", password == nullptr ? "" : password);
    Send(&request, sizeof(request));
}

void SocketClient::SendRoomLeaveRequest(int32_t roomId) {
    RoomLeaveRequest request;
    request.roomId = roomId;
    Send(&request, sizeof(request));
}

void SocketClient::SendRoomListRequest() {
    RoomListRequest request;
    Send(&request, sizeof(request));
}

// --- Friend accessors ---

std::vector<FriendEntryUi> SocketClient::GetFriendList() {
    std::lock_guard<std::mutex> lock(friendMutex);
    return friendList;
}

std::vector<SearchResultUi> SocketClient::GetSearchResults() {
    std::lock_guard<std::mutex> lock(friendMutex);
    return searchResults;
}

std::vector<PendingRequestUi> SocketClient::GetPendingRequests() {
    std::lock_guard<std::mutex> lock(friendMutex);
    return pendingRequests;
}

std::vector<std::string> SocketClient::DrainNotifications() {
    std::lock_guard<std::mutex> lock(notificationMutex);
    std::vector<std::string> drained;
    drained.swap(notificationQueue);
    return drained;
}

// --- Room accessors ---

std::vector<RoomEntryUi> SocketClient::GetRoomList() {
    std::lock_guard<std::mutex> lock(roomMutex);
    return roomList;
}

std::vector<RoomMemberUi> SocketClient::GetRoomMembers() {
    std::lock_guard<std::mutex> lock(roomMutex);
    return roomMembers;
}

int32_t SocketClient::GetSelectedRoomId() {
    std::lock_guard<std::mutex> lock(roomMutex);
    return selectedRoomId;
}

void SocketClient::SetSelectedRoomId(int32_t roomId) {
    std::lock_guard<std::mutex> lock(roomMutex);
    selectedRoomId = roomId;
}

bool SocketClient::IsFriendListRefreshNeeded() {
    return friendListNeedsRefresh.load();
}

void SocketClient::ClearFriendListRefreshFlag() {
    friendListNeedsRefresh.store(false);
}

bool SocketClient::IsRoomListRefreshNeeded() {
    return roomListNeedsRefresh.load();
}

void SocketClient::ClearRoomListRefreshFlag() {
    roomListNeedsRefresh.store(false);
}

// --- Sent requests accessor ---

std::vector<SentRequestUi> SocketClient::GetSentRequests() {
    std::lock_guard<std::mutex> lock(friendMutex);
    return sentRequests;
}

// --- Speaking detection ---

void SocketClient::RecordAudioFromUser(int32_t userId) {
    std::lock_guard<std::mutex> lock(speakingMutex);
    auto now = std::chrono::steady_clock::now();
    float nowFloat = std::chrono::duration<float>(now.time_since_epoch()).count();
    for (auto& s : speakingUsers) {
        if (s.userId == userId) {
            s.lastAudioTime = nowFloat;
            return;
        }
    }
    speakingUsers.push_back({userId, nowFloat});
}

std::vector<SpeakingUser> SocketClient::GetSpeakingUsers() {
    std::lock_guard<std::mutex> lock(speakingMutex);
    return speakingUsers;
}

void SocketClient::PruneSilentUsers(float thresholdSeconds) {
    std::lock_guard<std::mutex> lock(speakingMutex);
    auto now = std::chrono::steady_clock::now();
    float nowFloat = std::chrono::duration<float>(now.time_since_epoch()).count();
    speakingUsers.erase(
        std::remove_if(speakingUsers.begin(), speakingUsers.end(),
            [&](const SpeakingUser& s) { return (nowFloat - s.lastAudioTime) > thresholdSeconds; }),
        speakingUsers.end());
}
