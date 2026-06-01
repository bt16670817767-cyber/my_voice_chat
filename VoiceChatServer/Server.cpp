#include "Server.h"
#include "Auth/PasswordHasher.h"  // <--- 新增这行，用于密码哈希和验证
#include <thread>
#include <chrono>

Server::Server(uint16_t port) 
    : m_port(port), 
      m_isRunning(false), 
      m_userRepository("data/users.sqlite"),
      m_friendRepo("data/users.sqlite"),
      m_roomManager()
{
    m_userRepository.EnsureDefaultData();
    m_friendRepo.EnsureTables();

    m_app = std::make_unique<uWS::App>();

    // 这里改用 PerSocketData
    uWS::App::WebSocketBehavior<PerSocketData> behavior;
    behavior.compression = uWS::SHARED_COMPRESSOR;
    behavior.maxPayloadLength = 16 * 1024 * 1024;
    behavior.idleTimeout = 120;
    behavior.maxBackpressure = 1 * 1024 * 1024;
    
    behavior.open = [this](auto* ws) { this->OnConnection(ws); };
    behavior.message = [this](auto* ws, std::string_view message, uWS::OpCode opCode) {
        this->OnMessage(ws, message, opCode);
    };
    behavior.close = [this](auto* ws, int code, std::string_view message) {
        this->OnDisconnection(ws, code, message);
    };

    // 这里也改用 PerSocketData
    m_app->ws<PerSocketData>("/*", std::move(behavior));
}

Server::~Server() {
    m_isRunning = false;
}

void Server::LogMessage(const std::string& msg) {
    std::cout << "[Server] " << msg << std::endl;
}

void Server::Run() {
    m_isRunning = true;
    LogMessage("WebSocket Server starting on port " + std::to_string(m_port) + "...");
    
    m_app->listen(m_port, [this](auto* listen_socket) {
        if (listen_socket) {
            LogMessage("Successfully listening on port " + std::to_string(m_port));
        } else {
            LogMessage("Failed to listen on port " + std::to_string(m_port));
        }
    }).run();
    
    LogMessage("Server shutdown.");
}

void Server::SendJson(WebSocket* ws, const json& payload) {
    ws->send(payload.dump(), uWS::OpCode::TEXT);
}

void Server::SendMessageToUser(int32_t userId, const json& payload) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    auto it = m_userToSocket.find(userId);
    if (it != m_userToSocket.end()) {
        SendJson(it->second, payload);
    }
}

void Server::BroadcastToRoom(int32_t roomId, const json& payload, int32_t excludeUserId) {
    auto* room = m_roomManager.GetRoom(roomId);
    if (!room) return;

    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    for (int32_t memberId : room->memberUserIds) {
        if (memberId == excludeUserId) continue;
        auto it = m_userToSocket.find(memberId);
        if (it != m_userToSocket.end()) {
            SendJson(it->second, payload);
        }
    }
}

void Server::OnConnection(WebSocket* ws) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    m_sessions[ws] = SessionInfo{}; 
    LogMessage("New client connected.");
}

void Server::HandleUserDisconnect(WebSocket* ws) {
    int32_t userId = m_sessions[ws].userId;
    if (userId < 0) return;

    LogMessage("User ID " + std::to_string(userId) + " disconnected.");

    auto activeRoomIds = m_roomManager.GetUserActiveRoomIds(userId);
    for (int32_t roomId : activeRoomIds) {
        m_roomManager.RemoveMember(roomId, userId);
        
        const auto* room = m_roomManager.GetRoom(roomId);
        if (room) {
            json updateMsg = {
                {"type", "room_member_update"},
                {"roomId", roomId},
                {"members", json::array()}
            };
            for (int32_t mId : room->memberUserIds) {
                auto m_it = m_userToSocket.find(mId);
                if (m_it != m_userToSocket.end()) {
                    updateMsg["members"].push_back({
                        {"id", mId},
                        {"displayName", m_sessions[m_it->second].displayName}
                    });
                }
            }
            BroadcastToRoom(roomId, updateMsg);

            if (room->memberUserIds.empty()) {
                m_roomManager.DeleteRoom(roomId);
            }
        }
    }

    NotifyFriendsStatusChange(userId, false);
    m_userToSocket.erase(userId);
}

void Server::OnDisconnection(WebSocket* ws, int code, std::string_view message) {
    std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
    HandleUserDisconnect(ws);
    m_sessions.erase(ws);
}

void Server::NotifyFriendsStatusChange(int32_t userId, bool isOnline) {
    const auto friendIds = m_friendRepo.GetFriendIds(userId);
    json notify = {
        {"type", "friend_online_notify"},
        {"userId", userId},
        {"isOnline", isOnline}
    };
    for (int32_t fId : friendIds) {
        SendMessageToUser(fId, notify);
    }
}

void Server::OnMessage(WebSocket* ws, std::string_view message, uWS::OpCode opCode) {
    if (opCode != uWS::OpCode::TEXT) return;

    try {
        json payload = json::parse(message);
        if (!payload.contains("type") || !payload["type"].is_string()) return;
        
        std::string type = payload["type"].get<std::string>();
        LogMessage("Received packet type: " + type);

        if (type == "login") {
            HandleLoginRequest(ws, payload);
            return;
        } else if (type == "register") {
            HandleRegisterRequest(ws, payload);
            return;
        }

        int32_t userId = -1;
        {
            std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
            auto it = m_sessions.find(ws);
            if (it == m_sessions.end() || !it->second.authenticated) {
                SendJson(ws, {{"type", "error"}, {"message", "Unauthorized. Please login first."}});
                return;
            }
            userId = it->second.userId;
        }

        if (type == "friend_search") HandleFriendSearchRequest(ws, userId, payload);
        else if (type == "friend_add") HandleFriendAddRequest(ws, userId, payload);
        else if (type == "friend_accept") HandleFriendAcceptRequest(ws, userId, payload);
        else if (type == "friend_reject") HandleFriendRejectRequest(ws, userId, payload);
        else if (type == "friend_remove") HandleFriendRemoveRequest(ws, userId, payload);
        else if (type == "friend_list") HandleFriendListRequest(ws, userId);
        else if (type == "friend_sent_requests") HandleFriendSentRequests(ws, userId);
        else if (type == "room_create") HandleRoomCreateRequest(ws, userId, payload);
        else if (type == "room_join") HandleRoomJoinRequest(ws, userId, payload);
        else if (type == "room_leave") HandleRoomLeaveRequest(ws, userId, payload);
        else if (type == "room_list") HandleRoomListRequest(ws, userId);
        else if (type == "room_invite") HandleRoomInviteRequest(ws, userId, payload);
        else if (type == "logout") {
            HandleLogoutRequest(ws, userId);
        }
        else if (type == "webrtc_offer" || type == "webrtc_answer" || type == "webrtc_ice") {
            HandleWebRTCSignaling(ws, userId, payload);
        }
        else {
            LogMessage("Unknown message type: " + type);
        }

    // 把原来的 catch (const json::parse_error& e) 替换为：
    } catch (const json::exception& e) {
        LogMessage(std::string("JSON error: ") + e.what());
    }
}

// 业务空实现
// ==========================================
// 业务处理器：认证模块 (Auth)
// ==========================================

void Server::HandleRegisterRequest(WebSocket* ws, const json& payload) {
    // 1. 校验字段完整性
    if (!payload.contains("username") || !payload.contains("password") || !payload.contains("displayName")) {
        SendJson(ws, {{"type", "register_result"}, {"success", false}, {"message", "缺少必填字段"}});
        return;
    }

    std::string username = payload["username"].get<std::string>();
    std::string password = payload["password"].get<std::string>();
    std::string displayName = payload["displayName"].get<std::string>();

    // 2. 检查用户名是否已被注册
    if (m_userRepository.UsernameExists(username)) {
        SendJson(ws, {{"type", "register_result"}, {"success", false}, {"message", "该用户名已被注册，请换一个"}});
        return;
    }

    // 3. 使用 libsodium (Argon2id) 进行高强度哈希加密
    std::string hashedPw = PasswordHasher::HashPassword(password);
    if (hashedPw.empty()) {
        SendJson(ws, {{"type", "register_result"}, {"success", false}, {"message", "服务器加密模块异常"}});
        return;
    }

    // 4. 写入 SQLite 数据库
    auto newUser = m_userRepository.CreateUser(username, hashedPw, displayName);
    if (newUser) {
        LogMessage("New user registered successfully: " + username);
        
        // 注册成功，直接通知网页端
        SendJson(ws, {
            {"type", "register_result"}, 
            {"success", true}, 
            {"userId", newUser->id}, 
            {"displayName", newUser->displayName}
        });
    } else {
        SendJson(ws, {{"type", "register_result"}, {"success", false}, {"message", "数据库写入失败"}});
    }
}


void Server::HandleLoginRequest(WebSocket* ws, const json& payload) {
    // 1. 校验字段完整性
    if (!payload.contains("username") || !payload.contains("password")) {
        SendJson(ws, {{"type", "login_result"}, {"success", false}, {"message", "缺少必填字段"}});
        return;
    }

    std::string username = payload["username"].get<std::string>();
    std::string password = payload["password"].get<std::string>();

    LogMessage("Login attempt: " + username);

    // 2. 从数据库查找用户
    auto userOpt = m_userRepository.FindByUsername(username);
    if (!userOpt) {
        SendJson(ws, {{"type", "login_result"}, {"success", false}, {"message", "用户名不存在"}});
        return;
    }

    // 3. 验证密码哈希
    if (!PasswordHasher::VerifyPassword(password, userOpt->passwordHash)) {
        SendJson(ws, {{"type", "login_result"}, {"success", false}, {"message", "密码错误"}});
        return;
    }

    // 4. 登录成功！登记 Session 会话状态
    {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        
        // 检查该用户是否已经在别处登录？如果是，我们可以选择踢掉旧连接（这里为了简单先允许覆盖映射）
        m_sessions[ws].userId = userOpt->id;
        m_sessions[ws].username = userOpt->username;
        m_sessions[ws].displayName = userOpt->displayName;
        m_sessions[ws].authenticated = true;
        
        // 将该用户的 ID 绑定到当前的 WebSocket 连接上，方便后续精准推送消息
        m_userToSocket[userOpt->id] = ws;
    }

    LogMessage("User logged in: " + username + " (ID: " + std::to_string(userOpt->id) + ")");

    // 5. 将结果返回给前端网页
    SendJson(ws, {
        {"type", "login_result"}, 
        {"success", true}, 
        {"userId", userOpt->id}, 
        {"displayName", userOpt->displayName}
    });

    // 6. 广播上线通知：告诉我的所有好友“我上线了”
    NotifyFriendsStatusChange(userOpt->id, true);
}


// ==========================================
// 业务处理器：好友模块 (Friends)
// ==========================================

void Server::HandleFriendListRequest(WebSocket* ws, int32_t userId) {
    // 从数据库获取已通过的好友 和 收到的好友请求
    auto friends = m_friendRepo.GetFriends(userId);
    auto pending = m_friendRepo.GetPendingRequests(userId);

    json response = {
        {"type", "friend_list_result"},
        {"friends", json::array()},
        {"pending", json::array()}
    };

    // 打包好友列表（并结合内存，实时判断对方是否在线）
    for (const auto& f : friends) {
        bool isOnline = false;
        {
            std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
            isOnline = (m_userToSocket.find(f.userId) != m_userToSocket.end());
        }
        response["friends"].push_back({
            {"id", f.userId},
            {"username", f.username},
            {"displayName", f.displayName},
            {"isOnline", isOnline}
        });
    }

    // 打包待处理的请求
    for (const auto& p : pending) {
        response["pending"].push_back({
            {"id", p.userId},
            {"username", p.username},
            {"displayName", p.displayName}
        });
    }

    SendJson(ws, response);
}

void Server::HandleFriendSearchRequest(WebSocket* ws, int32_t userId, const json& payload) {
    if (!payload.contains("query")) return;
    std::string query = payload["query"].get<std::string>();

    auto results = m_friendRepo.SearchUsers(query, userId);

    json response = {
        {"type", "friend_search_result"},
        {"results", json::array()}
    };

    for (const auto& r : results) {
        bool isOnline = false;
        {
            std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
            isOnline = (m_userToSocket.find(r.userId) != m_userToSocket.end());
        }
        response["results"].push_back({
            {"id", r.userId},
            {"username", r.username},
            {"displayName", r.displayName},
            {"relationshipStatus", r.relationshipStatus},
            {"isOnline", isOnline}
        });
    }
    SendJson(ws, response);
}

void Server::HandleFriendAddRequest(WebSocket* ws, int32_t userId, const json& payload) {
    if (!payload.contains("targetId")) return;
    int32_t targetId = payload["targetId"].get<int32_t>();

    bool success = m_friendRepo.SendFriendRequest(userId, targetId);
    
    SendJson(ws, {
        {"type", "friend_add_result"},
        {"success", success},
        {"targetId", targetId}
    });

    // 【高光时刻】：如果发送请求成功，且对方恰好在线，我们直接把请求“推”给对方！
    if (success) {
        std::string myName;
        {
            std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
            myName = m_sessions[ws].displayName;
        }
        SendMessageToUser(targetId, {
            {"type", "friend_request_notify"},
            {"fromId", userId},
            {"fromName", myName}
        });
    }
}

void Server::HandleFriendAcceptRequest(WebSocket* ws, int32_t userId, const json& payload) {
    if (!payload.contains("targetId")) return;
    int32_t targetId = payload["targetId"].get<int32_t>();

    // 对方发给我的，所以我作为接受方，调用参数是 (targetId, userId)
    bool success = m_friendRepo.AcceptFriendRequest(targetId, userId); 
    
    if (success) {
        // 告诉自己接受成功了，并刷新自己的列表
        HandleFriendListRequest(ws, userId);
        
        // 如果对方在线，告诉对方“我通过了你的好友请求”，让他也刷新列表
        SendMessageToUser(targetId, {
            {"type", "friend_accept_notify"},
            {"friendId", userId}
        });
    }
}

void Server::HandleFriendRejectRequest(WebSocket* ws, int32_t userId, const json& payload) {
    if (!payload.contains("targetId")) return;
    int32_t targetId = payload["targetId"].get<int32_t>();

    bool success = m_friendRepo.RejectFriendRequest(targetId, userId);
    if (success) {
        // 拒绝后刷新自己的列表，清掉这个请求
        HandleFriendListRequest(ws, userId);
    }
}

void Server::HandleFriendRemoveRequest(WebSocket* ws, int32_t userId, const json& payload) {
    if (!payload.contains("targetId")) return;
    int32_t targetId = payload["targetId"].get<int32_t>();

    bool success = m_friendRepo.RemoveFriend(userId, targetId);
    if (success) {
        // 删完后刷新自己的好友列表
        HandleFriendListRequest(ws, userId);

        // 如果对方在线，通知他“你被删了”，让他也刷新列表
        SendMessageToUser(targetId, {
            {"type", "friend_remove_notify"},
            {"friendId", userId}
        });
    }
}

void Server::HandleFriendSentRequests(WebSocket* ws, int32_t userId) {
    // 前端如果需要查看“我发出的但未被处理的请求”，可以在这里实现
    // 目前 Vue 前端暂未用到，留空即可
}
// ==========================================
// 业务处理器：房间模块 (Rooms)
// ==========================================

void Server::HandleRoomListRequest(WebSocket* ws, int32_t userId) {
    auto rooms = m_roomManager.GetAllRooms();
    json response = {
        {"type", "room_list_result"},
        {"rooms", json::array()}
    };
    for (const auto& r : rooms) {
        response["rooms"].push_back({
            {"id", r.roomId},
            {"name", r.roomName},
            {"ownerId", r.ownerUserId},
            {"hasPassword", !r.password.empty()}, // 安全起见，只下发是否有密码的布尔值
            {"count", r.memberUserIds.size()}
        });
    }
    SendJson(ws, response);
}

void Server::HandleRoomCreateRequest(WebSocket* ws, int32_t userId, const json& payload) {
    std::string roomName = payload.value("roomName", "");
    std::string password = payload.value("password", "");

    // 1. 创建房间
    int32_t roomId = m_roomManager.CreateRoom(userId, roomName, password);
    
    // 2. 登记 Session
    {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        m_sessions[ws].activeRoomIds.insert(roomId);
    }

    // 3. 返回创建成功信息给前端
    SendJson(ws, {
        {"type", "room_create_result"},
        {"success", true},
        {"roomId", roomId},
        {"roomName", roomName}
    });

    // 创建者默认是唯一成员，发送一下成员列表让前端刷新
    std::string myName;
    {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        myName = m_sessions[ws].displayName;
    }
    SendJson(ws, {
        {"type", "room_member_update"},
        {"roomId", roomId},
        {"members", {{ {"id", userId}, {"displayName", myName} }}}
    });
}

void Server::HandleRoomJoinRequest(WebSocket* ws, int32_t userId, const json& payload) {
    int32_t roomId = payload.value("roomId", -1);
    std::string password = payload.value("password", "");

    const RoomInfo* room = m_roomManager.GetRoom(roomId);
    if (!room) {
        SendJson(ws, {{"type", "room_join_result"}, {"success", false}, {"message", "房间不存在"}});
        return;
    }

    if (!room->password.empty() && room->password != password) {
        SendJson(ws, {{"type", "room_join_result"}, {"success", false}, {"message", "房间密码错误"}});
        return;
    }

    // 1. 加入房间
    m_roomManager.AddMember(roomId, userId);
    {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        m_sessions[ws].activeRoomIds.insert(roomId);
    }

    // 2. 告诉该用户加入成功
    SendJson(ws, {
        {"type", "room_join_result"},
        {"success", true},
        {"roomId", roomId},
        {"roomName", room->roomName}
    });

    // 3. 组装最新的成员列表，并广播给房间里所有人（包括刚进来的自己）
    json updateMsg = {
        {"type", "room_member_update"},
        {"roomId", roomId},
        {"members", json::array()}
    };
    
    const RoomInfo* updatedRoom = m_roomManager.GetRoom(roomId);
    if (updatedRoom) {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        for (int32_t mId : updatedRoom->memberUserIds) {
            auto m_it = m_userToSocket.find(mId);
            if (m_it != m_userToSocket.end()) {
                updateMsg["members"].push_back({
                    {"id", mId},
                    {"displayName", m_sessions[m_it->second].displayName}
                });
            }
        }
    }
    BroadcastToRoom(roomId, updateMsg); // excludeUserId 留空，广播给所有人
}

void Server::HandleRoomLeaveRequest(WebSocket* ws, int32_t userId, const json& payload) {
    int32_t roomId = payload.value("roomId", -1);
    
    m_roomManager.RemoveMember(roomId, userId);
    {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        m_sessions[ws].activeRoomIds.erase(roomId);
    }

    SendJson(ws, {
        {"type", "room_leave_result"},
        {"success", true}
    });

    // 检查房间是否空了，空了就销毁；没空就通知剩下的人
    const RoomInfo* room = m_roomManager.GetRoom(roomId);
    if (room) {
        if (room->memberUserIds.empty()) {
            m_roomManager.DeleteRoom(roomId);
        } else {
            json updateMsg = {
                {"type", "room_member_update"},
                {"roomId", roomId},
                {"members", json::array()}
            };
            std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
            for (int32_t mId : room->memberUserIds) {
                auto m_it = m_userToSocket.find(mId);
                if (m_it != m_userToSocket.end()) {
                    updateMsg["members"].push_back({
                        {"id", mId},
                        {"displayName", m_sessions[m_it->second].displayName}
                    });
                }
            }
            BroadcastToRoom(roomId, updateMsg);
        }
    }
}

// ==========================================
// 业务处理器：退出登录与 WebRTC 信令
// ==========================================

void Server::HandleLogoutRequest(WebSocket* ws, int32_t userId) {
    LogMessage("User ID " + std::to_string(userId) + " requested logout.");
    
    // 1. 复用掉线清理逻辑（自动退出房间、通知好友下线、清理映射）
    HandleUserDisconnect(ws);
    
    // 2. 清空该 WebSocket 的 Session 认证状态，但保持 TCP 连接不断开
    {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        m_sessions[ws] = SessionInfo{}; 
    }
    
    // 3. 告诉前端退出成功
    SendJson(ws, {
        {"type", "logout_result"},
        {"success", true}
    });
}

void Server::HandleWebRTCSignaling(WebSocket* ws, int32_t userId, const json& payload) {
    // WebRTC 信令服务器的核心职责就是“传话筒”
    // 用户 A 发送的 SDP 或 ICE 候选，服务器不需要解析内容，直接原封不动转交（Relay）给用户 B
    if (!payload.contains("targetId")) return;
    int32_t targetId = payload["targetId"].get<int32_t>();

    // 复制原数据，并附加上发送者的 ID，让对方知道是谁发来的
    json forwarded = payload;
    forwarded["fromId"] = userId;
    
    SendMessageToUser(targetId, forwarded);
}
// ==========================================
// 附加功能：邀请好友加入房间
// ==========================================
void Server::HandleRoomInviteRequest(WebSocket* ws, int32_t userId, const json& payload) {
    int32_t targetId = payload.value("targetId", -1);
    int32_t roomId = payload.value("roomId", -1);

    const RoomInfo* room = m_roomManager.GetRoom(roomId);
    if (!room) return;

    // 检查发送邀请的人是不是真的在这个房间里（防作弊）
    if (room->memberUserIds.count(userId) == 0) return;

    std::string myName;
    {
        std::lock_guard<std::recursive_mutex> lock(m_clientsMutex);
        myName = m_sessions[ws].displayName;
    }

    // 将邀请直接推送给目标好友
    SendMessageToUser(targetId, {
        {"type", "room_invite_notify"},
        {"roomId", roomId},
        {"roomName", room->roomName},
        {"fromId", userId},
        {"fromName", myName},
        {"hasPassword", !room->password.empty()}
    });
}