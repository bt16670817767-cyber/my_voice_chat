#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <atomic>

struct RoomInfo {
    int32_t roomId = -1;
    std::string roomName;
    int64_t channel = 0;
    int32_t ownerUserId = -1;
    std::string password;
    std::set<int32_t> memberUserIds;
    std::set<int32_t> invitedUserIds;
};

class RoomManager {
public:
    RoomManager();

    int32_t CreateRoom(int32_t ownerUserId, const std::string& roomName, const std::string& password);
    bool DeleteRoom(int32_t roomId);
    bool AddMember(int32_t roomId, int32_t userId);
    bool RemoveMember(int32_t roomId, int32_t userId);
    bool AddInvite(int32_t roomId, int32_t userId);
    bool IsMember(int32_t roomId, int32_t userId) const;
    bool IsInvited(int32_t roomId, int32_t userId) const;

    const RoomInfo* GetRoom(int32_t roomId) const;
    std::vector<RoomInfo> GetRoomsForUser(int32_t userId) const;
    std::vector<RoomInfo> GetAllRooms() const;
    std::vector<int32_t> GetRoomMembers(int32_t roomId) const;
    std::set<int32_t> GetUserActiveRoomIds(int32_t userId) const;

    mutable std::mutex mutex;

private:
    std::map<int32_t, RoomInfo> rooms;
    std::atomic<int32_t> nextRoomId{1};
    std::atomic<int64_t> nextRoomChannel{1000000};
};
