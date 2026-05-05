#include "RoomManager.h"

RoomManager::RoomManager() {}

int32_t RoomManager::CreateRoom(int32_t ownerUserId, const std::string& roomName, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex);
    int32_t roomId = nextRoomId++;
    int64_t channel = nextRoomChannel++;

    RoomInfo room;
    room.roomId = roomId;
    room.roomName = roomName.empty() ? ("Room" + std::to_string(roomId)) : roomName;
    room.channel = channel;
    room.ownerUserId = ownerUserId;
    room.password = password;
    room.memberUserIds.insert(ownerUserId);

    rooms[roomId] = room;
    return roomId;
}

bool RoomManager::DeleteRoom(int32_t roomId) {
    std::lock_guard<std::mutex> lock(mutex);
    return rooms.erase(roomId) > 0;
}

bool RoomManager::AddMember(int32_t roomId, int32_t userId) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return false;
    it->second.memberUserIds.insert(userId);
    it->second.invitedUserIds.erase(userId);
    return true;
}

bool RoomManager::RemoveMember(int32_t roomId, int32_t userId) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return false;
    it->second.memberUserIds.erase(userId);
    return true;
}

bool RoomManager::AddInvite(int32_t roomId, int32_t userId) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return false;
    it->second.invitedUserIds.insert(userId);
    return true;
}

bool RoomManager::IsMember(int32_t roomId, int32_t userId) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return false;
    return it->second.memberUserIds.count(userId) > 0;
}

bool RoomManager::IsInvited(int32_t roomId, int32_t userId) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return false;
    return it->second.invitedUserIds.count(userId) > 0;
}

const RoomInfo* RoomManager::GetRoom(int32_t roomId) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return nullptr;
    return &it->second;
}

std::vector<RoomInfo> RoomManager::GetRoomsForUser(int32_t userId) const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<RoomInfo> result;
    for (const auto& pair : rooms) {
        const RoomInfo& room = pair.second;
        if (room.memberUserIds.count(userId) > 0 || room.invitedUserIds.count(userId) > 0) {
            result.push_back(room);
        }
    }
    return result;
}

std::vector<RoomInfo> RoomManager::GetAllRooms() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<RoomInfo> result;
    for (const auto& pair : rooms) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<int32_t> RoomManager::GetRoomMembers(int32_t roomId) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(roomId);
    if (it == rooms.end()) return {};
    return std::vector<int32_t>(it->second.memberUserIds.begin(), it->second.memberUserIds.end());
}

std::set<int32_t> RoomManager::GetUserActiveRoomIds(int32_t userId) const {
    std::lock_guard<std::mutex> lock(mutex);
    std::set<int32_t> result;
    for (const auto& pair : rooms) {
        if (pair.second.memberUserIds.count(userId) > 0) {
            result.insert(pair.first);
        }
    }
    return result;
}
