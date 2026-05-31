#pragma once

#include "UserRepository.h"
#include <optional>
#include <string>
#include <vector>

struct FriendEntry {
    int32_t userId = -1;
    std::string username;
    std::string displayName;
};

struct SearchResultEntry {
    int32_t userId = -1;
    std::string username;
    std::string displayName;
    int relationshipStatus = 0; // 0=none, 1=accepted friends, 2=pending from searcher, 3=pending to searcher
    bool isOnline = false;
};

class FriendRepository {
public:
    explicit FriendRepository(std::string databasePath);

    bool EnsureTables() const;

    bool SendFriendRequest(int32_t fromUserId, int32_t toUserId) const;
    bool AcceptFriendRequest(int32_t fromUserId, int32_t toUserId) const;
    bool RejectFriendRequest(int32_t fromUserId, int32_t toUserId) const;
    bool RemoveFriend(int32_t userId, int32_t friendId) const;

    // Returns accepted friend userIds
    std::vector<int32_t> GetFriendIds(int32_t userId) const;

    // Returns full friend details
    std::vector<FriendEntry> GetFriends(int32_t userId) const;

    // Returns incoming pending friend requests
    std::vector<FriendEntry> GetPendingRequests(int32_t userId) const;

    // Search users by ID or name (username + display_name). Returns up to maxResults.
    std::vector<SearchResultEntry> SearchUsers(const std::string& query, int32_t searcherId, int maxResults = 5) const;

    // Get outgoing pending friend requests (sent by userId, not yet accepted)
    std::vector<FriendEntry> GetSentRequests(int32_t userId) const;

    // Check relationship: 0=none, 1=accepted friends, 2=pending from A to B, 3=pending from B to A
    int GetRelationshipStatus(int32_t userIdA, int32_t userIdB) const;

private:
    std::string databasePath;
};
