//
// Created by Amin on 10/15/23.
//
#include "Server.h"
#include "Auth/UserRepository.h"
#include "Auth/PasswordHasher.h"
#include <cstring>

namespace {
UserRepository g_userRepository("users.db");
}

Server* Server::Instance = nullptr;
SteamNetworkingMicroseconds Server::g_logTimeZero;
ISteamNetworkingSockets* Server::steamNetworking;
HSteamNetPollGroup Server::connectionPollGroup;
std::map<int64, std::set<HSteamNetConnection>> Server::channelToConnnectionsMap;
std::map<HSteamNetConnection, Server::SessionInfo> Server::connectionSessions;
std::mutex Server::sessionMutex;
std::mutex Server::channelMutex;


bool Server::StartServer(uint16 port) {
    Instance = this;

    // Create client and server sockets
    InitSteamDatagramConnectionSockets();

    steamNetworking = SteamNetworkingSockets();
    if (!g_userRepository.EnsureDefaultData()) {
        printf("Failed to initialize users database file.\n");
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
                std::lock_guard<std::mutex> lock(sessionMutex);
                connectionSessions.erase(pInfo->m_hConn);
            }

            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
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
            // We will get a callback immediately after accepting the connection.
            // Since we are the server, we can ignore this, it's not news to us.

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
    if (user.has_value() && user->status == 1 && PasswordHasher::Verify(request->password, user->passwordHash)) {
        response.success = 1;
        response.errorCode = ERR_NONE;
        response.userId = user->id;
        std::snprintf(response.displayName, sizeof(response.displayName), "%s", user->displayName.c_str());
        std::snprintf(response.message, sizeof(response.message), "%s", "login success");

        std::lock_guard<std::mutex> lock(sessionMutex);
        SessionInfo& session = connectionSessions[connection];
        session.authenticated = true;
        session.userId = response.userId;
        session.username = request->username;
        session.displayName = response.displayName;
    } else {
        response.success = 0;
        response.errorCode = ERR_UNAUTHENTICATED;
        std::snprintf(response.message, sizeof(response.message), "%s", "invalid username or password");
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
            break;
        }

        uint8_t messageType = ((uint8_t*)pIncomingMsg->m_pData)[0];
        receivedBytesCount += pIncomingMsg->GetSize();

        //printf("message type is %d \n", messageType);


        switch (messageType)
        {
            case LOGIN_REQ:
                HandleLoginRequest(pIncomingMsg->m_conn, static_cast<LoginRequest*>(pIncomingMsg->m_pData), pIncomingMsg->GetSize());
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
                //printf("size of received data: %u \n", pIncomingMsg->GetSize());
                channel = pIncomingMsg->GetConnectionUserData();
                //printf("total connections %d \n", channelToConnnectionsMap[channel].size());
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
                //printf("\r Data received on server, data size = %u bytes", pIncomingMsg->GetSize());
                break;
            default:
                break;
        }

        // We don't need this anymore.
        pIncomingMsg->Release();


    }
}

void Server::PollConnectionStateChanges() {
    steamNetworking->RunCallbacks();
}

uint16 Server::GetSentBytes()
{
    return sentBytesCount * 8 / 1024;
}

uint16 Server::GetRecievedBytes()
{
    return receivedBytesCount * 8 / 1024;
}

bool Server::ResetCounters()
{
    sentBytesCount = 0;
    receivedBytesCount = 0;
    return true;
}
