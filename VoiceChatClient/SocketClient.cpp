//
// Created by Amin on 10/17/23.
//

#include "SocketClient.h"
#include <cstdio>

SteamNetworkingMicroseconds SocketClient::g_logTimeZero;
HSteamNetConnection SocketClient::connection;
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
    assert(pInfo->m_hConn == connection || connection == k_HSteamNetConnection_Invalid );

    // What's the state of the connection?
    switch ( pInfo->m_info.m_eState )
    {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            // Print an appropriate message
            if ( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting )
            {
                // Note: we could distinguish between a timeout, a rejected connection,
                // or some other transport problem.
                printf( "(%s)\n", pInfo->m_info.m_szEndDebug );
            }
            else if ( pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally )
            {
                printf( "(%s)\n", pInfo->m_info.m_szEndDebug );
            }
            else
            {
                // NOTE: We could check the reason code for a normal disconnection
                printf( "(%s)\n", pInfo->m_info.m_szEndDebug );
            }

            // Clean up the connection.  This is important!
            // The connection is "closed" in the network sense, but
            // it has not been destroyed.  We must close it on our end, too
            // to finish up.  The reason information do not matter in this case,
            // and we cannot linger because it's already closed on the other end,
            // so we just pass 0's.
            printf("closing connection\n");
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
            // We will get this callback when we start connecting.
            // We can ignore this.
            isConnecting.store(true);
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            printf( "Connected to server OK" );
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

        auto* audioData = static_cast<AudioData*>(pIncomingMsg->m_pData);

        printf("received data from server, size: %u \n", audioData->inputCurrentCounter);

        printf("receive counter is %d \n", ++receiveCounter);
        // Playback
        const size_t buffer_size = audioData->inputCurrentCounter;
        for (size_t i = 0; i < buffer_size; ++i) {
            if (audioData->Input[i] != 0)
                _voiceAudioBuffer->AddInput(audioData->Input[i]);
            // Only enable this part for debugging, any action here causes delays on the voice
            //printf("%d," , audioData->Input[i]);
        }
        printf("\n");

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
