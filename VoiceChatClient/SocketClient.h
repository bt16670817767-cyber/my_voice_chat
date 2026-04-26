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
#include "Utils.h"
#include <steam/isteamnetworkingutils.h>
#include <cassert>
#include <atomic>
#include <mutex>
#include <string>
#include <cstdint>



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

    static void InitSteamDatagramConnectionSockets();
    static void DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg );
    static void OnSteamNetConnectionStatusChanged( SteamNetConnectionStatusChangedCallback_t *pInfo );
    static void SetStatusMessage(const std::string& message);
    static void SetAuthMessage(const std::string& message);

public:
    bool IsConnected();
    bool IsConnecting();
    bool Connect(SteamNetworkingIPAddr add);
    void PollIncomingMessages(NetworkBuffer* _voiceOutputBuffer);
    void PollConnectionStateChanges();
    void Send(const void* data, uint32 size);
    void SendLoginRequest(const char* username, const char* password);
    bool IsAuthenticated();
    std::string GetAuthMessage();
    std::string GetDisplayName();
    int32_t GetUserId();
    std::string GetStatusMessage();
    SocketClient();
    ~SocketClient();
};
