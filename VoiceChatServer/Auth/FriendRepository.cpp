#include "FriendRepository.h"
#include <SQLiteCpp/SQLiteCpp.h>

namespace {
constexpr const char* kCreateTableSql =
    "CREATE TABLE IF NOT EXISTS friendships ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  user_id INTEGER NOT NULL,"
    "  friend_id INTEGER NOT NULL,"
    "  status INTEGER NOT NULL DEFAULT 0,"
    "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
    "  UNIQUE(user_id, friend_id)"
    ");";

constexpr const char* kCreateIndexSql1 =
    "CREATE INDEX IF NOT EXISTS idx_friendships_user ON friendships(user_id);";
constexpr const char* kCreateIndexSql2 =
    "CREATE INDEX IF NOT EXISTS idx_friendships_friend ON friendships(friend_id);";

constexpr const char* kInsertFriendshipSql =
    "INSERT OR IGNORE INTO friendships(user_id, friend_id, status) VALUES(?, ?, ?);";

constexpr const char* kAcceptFriendshipSql =
    "UPDATE friendships SET status = 1, created_at = datetime('now') WHERE user_id = ? AND friend_id = ? AND status = 0;";

constexpr const char* kDeleteFriendshipSql =
    "DELETE FROM friendships WHERE user_id = ? AND friend_id = ?;";

constexpr const char* kGetFriendsSql =
    "SELECT u.id, u.username, u.display_name FROM users u "
    "JOIN friendships f ON (f.friend_id = u.id AND f.user_id = ? AND f.status = 1) "
    "OR (f.user_id = u.id AND f.friend_id = ? AND f.status = 1) "
    "WHERE u.id != ?;";

constexpr const char* kGetPendingRequestsSql =
    "SELECT u.id, u.username, u.display_name FROM users u "
    "JOIN friendships f ON f.user_id = u.id "
    "WHERE f.friend_id = ? AND f.status = 0;";

constexpr const char* kGetRelationshipSql =
    "SELECT status, user_id FROM friendships WHERE "
    "(user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?);";
}

FriendRepository::FriendRepository(std::string databasePath)
    : databasePath(std::move(databasePath)) {}

bool FriendRepository::EnsureTables() const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db.exec(kCreateTableSql);
        db.exec(kCreateIndexSql1);
        db.exec(kCreateIndexSql2);
        return true;
    } catch (...) {
        return false;
    }
}

bool FriendRepository::SendFriendRequest(int32_t fromUserId, int32_t toUserId) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db.exec(kCreateTableSql);
        SQLite::Statement stmt(db, kInsertFriendshipSql);
        stmt.bind(1, fromUserId);
        stmt.bind(2, toUserId);
        stmt.bind(3, 0); // pending
        stmt.exec();
        return db.getChanges() > 0;
    } catch (...) {
        return false;
    }
}

bool FriendRepository::AcceptFriendRequest(int32_t fromUserId, int32_t toUserId) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        SQLite::Statement stmt(db, kAcceptFriendshipSql);
        stmt.bind(1, fromUserId);
        stmt.bind(2, toUserId);
        stmt.exec();
        return db.getChanges() > 0;
    } catch (...) {
        return false;
    }
}

bool FriendRepository::RejectFriendRequest(int32_t fromUserId, int32_t toUserId) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        SQLite::Statement stmt(db, kDeleteFriendshipSql);
        stmt.bind(1, fromUserId);
        stmt.bind(2, toUserId);
        stmt.exec();
        return db.getChanges() > 0;
    } catch (...) {
        return false;
    }
}

bool FriendRepository::RemoveFriend(int32_t userId, int32_t friendId) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        // Delete in both directions
        {
            SQLite::Statement stmt(db, kDeleteFriendshipSql);
            stmt.bind(1, userId);
            stmt.bind(2, friendId);
            stmt.exec();
        }
        {
            SQLite::Statement stmt(db, kDeleteFriendshipSql);
            stmt.bind(1, friendId);
            stmt.bind(2, userId);
            stmt.exec();
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<int32_t> FriendRepository::GetFriendIds(int32_t userId) const {
    std::vector<int32_t> ids;
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);
        SQLite::Statement stmt(db, kGetFriendsSql);
        stmt.bind(1, userId);
        stmt.bind(2, userId);
        stmt.bind(3, userId);
        while (stmt.executeStep()) {
            ids.push_back(stmt.getColumn(0).getInt());
        }
    } catch (...) {}
    return ids;
}

std::vector<FriendEntry> FriendRepository::GetFriends(int32_t userId) const {
    std::vector<FriendEntry> friends;
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);
        SQLite::Statement stmt(db, kGetFriendsSql);
        stmt.bind(1, userId);
        stmt.bind(2, userId);
        stmt.bind(3, userId);
        while (stmt.executeStep()) {
            FriendEntry entry;
            entry.userId = stmt.getColumn(0).getInt();
            entry.username = stmt.getColumn(1).getString();
            entry.displayName = stmt.getColumn(2).getString();
            friends.push_back(entry);
        }
    } catch (...) {}
    return friends;
}

std::vector<FriendEntry> FriendRepository::GetPendingRequests(int32_t userId) const {
    std::vector<FriendEntry> requests;
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);
        SQLite::Statement stmt(db, kGetPendingRequestsSql);
        stmt.bind(1, userId);
        while (stmt.executeStep()) {
            FriendEntry entry;
            entry.userId = stmt.getColumn(0).getInt();
            entry.username = stmt.getColumn(1).getString();
            entry.displayName = stmt.getColumn(2).getString();
            requests.push_back(entry);
        }
    } catch (...) {}
    return requests;
}

int FriendRepository::GetRelationshipStatus(int32_t userIdA, int32_t userIdB) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);
        SQLite::Statement stmt(db, kGetRelationshipSql);
        stmt.bind(1, userIdA);
        stmt.bind(2, userIdB);
        stmt.bind(3, userIdB);
        stmt.bind(4, userIdA);
        while (stmt.executeStep()) {
            int status = stmt.getColumn(0).getInt();
            int fromUser = stmt.getColumn(1).getInt();
            if (status == 1) return 1; // accepted friends
            if (status == 0) {
                return (fromUser == userIdA) ? 2 : 3; // 2=outgoing, 3=incoming
            }
        }
    } catch (...) {}
    return 0; // no relationship
}

std::vector<SearchResultEntry> FriendRepository::SearchUsers(const std::string& query, int32_t searcherId, int maxResults) const {
    std::vector<SearchResultEntry> results;
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);

        // First try exact ID match
        constexpr const char* kFindByIdSql =
            "SELECT id, username, display_name, status FROM users WHERE id = ? AND status = 1 LIMIT 1;";
        SQLite::Statement idStmt(db, kFindByIdSql);
        int32_t parsedId = 0;
        bool isNumeric = true;
        for (char c : query) {
            if (c >= '0' && c <= '9') parsedId = parsedId * 10 + (c - '0');
            else { isNumeric = false; break; }
        }
        if (isNumeric && parsedId > 0) {
            idStmt.bind(1, parsedId);
            if (idStmt.executeStep()) {
                const int32_t foundId = idStmt.getColumn(0).getInt();
                if (foundId != searcherId) {
                    SearchResultEntry entry;
                    entry.userId = foundId;
                    entry.username = idStmt.getColumn(1).getString();
                    entry.displayName = idStmt.getColumn(2).getString();
                    entry.relationshipStatus = GetRelationshipStatus(searcherId, foundId);
                    entry.isOnline = false;
                    results.push_back(entry);
                }
            }
        }

        // Also search by username and display_name
        if (results.size() < static_cast<size_t>(maxResults)) {
            constexpr const char* kSearchByNameSql =
                "SELECT id, username, display_name, status FROM users "
                "WHERE status = 1 AND (username LIKE ? OR display_name LIKE ?) "
                "ORDER BY username LIMIT ?;";
            SQLite::Statement stmt(db, kSearchByNameSql);
            const std::string likeQuery = "%" + query + "%";
            stmt.bind(1, likeQuery);
            stmt.bind(2, likeQuery);
            stmt.bind(3, maxResults);
            while (stmt.executeStep() && results.size() < static_cast<size_t>(maxResults)) {
                const int32_t foundId = stmt.getColumn(0).getInt();
                if (foundId == searcherId) continue;
                // Skip if already found by ID
                bool already = false;
                for (const auto& r : results) {
                    if (r.userId == foundId) { already = true; break; }
                }
                if (already) continue;
                SearchResultEntry entry;
                entry.userId = foundId;
                entry.username = stmt.getColumn(1).getString();
                entry.displayName = stmt.getColumn(2).getString();
                entry.relationshipStatus = GetRelationshipStatus(searcherId, foundId);
                entry.isOnline = false;
                results.push_back(entry);
            }
        }
    } catch (...) {}
    return results;
}

std::vector<FriendEntry> FriendRepository::GetSentRequests(int32_t userId) const {
    std::vector<FriendEntry> sent;
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);
        constexpr const char* kGetSentSql =
            "SELECT u.id, u.username, u.display_name FROM users u "
            "JOIN friendships f ON f.friend_id = u.id "
            "WHERE f.user_id = ? AND f.status = 0;";
        SQLite::Statement stmt(db, kGetSentSql);
        stmt.bind(1, userId);
        while (stmt.executeStep()) {
            FriendEntry entry;
            entry.userId = stmt.getColumn(0).getInt();
            entry.username = stmt.getColumn(1).getString();
            entry.displayName = stmt.getColumn(2).getString();
            sent.push_back(entry);
        }
    } catch (...) {}
    return sent;
}
