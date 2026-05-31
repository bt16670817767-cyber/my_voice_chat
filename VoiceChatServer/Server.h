#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <set>

// 第三方库
#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>

// 业务模块依赖 
#include "Auth/UserRepository.h"
#include "Auth/FriendRepository.h"
#include "RoomManager.h"

// 解决编译器的 alignof(void) 报错，定义一个空的连接数据结构
struct PerSocketData {}; 

// 别名定义
using WebSocket = uWS::WebSocket<false, true, PerSocketData>;
using json = nlohmann::json;

// 管理每个 WebSocket 连接的会话状态
struct SessionInfo {
    int32_t userId = -1;
    std::string username;
    std::string displayName;
    bool authenticated = false;
    std::set<int32_t> activeRoomIds; 
};

class Server {
public:
    Server(uint16_t port);
    ~Server();

    void Run();

private:
    uint16_t m_port;
    bool m_isRunning;
    std::unique_ptr<uWS::App> m_app;

    // --- 核心业务模块 ---
    UserRepository m_userRepository;
    FriendRepository m_friendRepo;
    RoomManager m_roomManager;

    // --- 连接映射与并发控制 ---
    std::map<WebSocket*, SessionInfo> m_sessions;
    std::map<int32_t, WebSocket*> m_userToSocket;
    std::mutex m_clientsMutex;

    // --- 内部辅助函数 ---
    void LogMessage(const std::string& msg);
    void SendJson(WebSocket* ws, const json& payload);
    void SendMessageToUser(int32_t userId, const json& payload);
    void BroadcastToRoom(int32_t roomId, const json& payload, int32_t excludeUserId = -1);
    
    void HandleUserDisconnect(WebSocket* ws);
    void NotifyFriendsStatusChange(int32_t userId, bool isOnline);

    // --- uWS 核心事件 ---
    void OnConnection(WebSocket* ws);
    void OnDisconnection(WebSocket* ws, int code, std::string_view message);
    void OnMessage(WebSocket* ws, std::string_view message, uWS::OpCode opCode);

    // --- 业务路由处理器 (Handlers) ---
    void HandleRegisterRequest(WebSocket* ws, const json& payload);
    void HandleLoginRequest(WebSocket* ws, const json& payload);

    void HandleLogoutRequest(WebSocket* ws, int32_t userId);
    void HandleWebRTCSignaling(WebSocket* ws, int32_t userId, const json& payload);

    void HandleFriendSearchRequest(WebSocket* ws, int32_t userId, const json& payload);
    void HandleFriendAddRequest(WebSocket* ws, int32_t userId, const json& payload);
    void HandleFriendAcceptRequest(WebSocket* ws, int32_t userId, const json& payload);
    void HandleFriendRejectRequest(WebSocket* ws, int32_t userId, const json& payload);
    void HandleFriendRemoveRequest(WebSocket* ws, int32_t userId, const json& payload);
    void HandleFriendListRequest(WebSocket* ws, int32_t userId);
    void HandleFriendSentRequests(WebSocket* ws, int32_t userId);

    void HandleRoomCreateRequest(WebSocket* ws, int32_t userId, const json& payload);
    void HandleRoomJoinRequest(WebSocket* ws, int32_t userId, const json& payload);
    void HandleRoomLeaveRequest(WebSocket* ws, int32_t userId, const json& payload);
    void HandleRoomListRequest(WebSocket* ws, int32_t userId);
};