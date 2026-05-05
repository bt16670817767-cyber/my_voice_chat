#include "UserRepository.h"

#include "PasswordHasher.h"

#include <SQLiteCpp/SQLiteCpp.h>

namespace {
constexpr const char* kCreateUsersTableSql =
    "CREATE TABLE IF NOT EXISTS users ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  username TEXT NOT NULL UNIQUE,"
    "  password_hash TEXT NOT NULL,"
    "  display_name TEXT NOT NULL,"
    "  status INTEGER NOT NULL DEFAULT 1,"
    "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
    "  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
    ");";

constexpr const char* kCountUsersSql = "SELECT COUNT(1) FROM users;";

constexpr const char* kInsertUserSql =
    "INSERT INTO users(username, password_hash, display_name, status) VALUES(?, ?, ?, ?);";

constexpr const char* kFindByUsernameSql =
    "SELECT id, username, password_hash, display_name, status FROM users WHERE username = ? LIMIT 1;";

constexpr const char* kUsernameExistsSql =
    "SELECT COUNT(1) FROM users WHERE username = ?;";

constexpr const char* kFindByIdSql =
    "SELECT id, username, password_hash, display_name, status FROM users WHERE id = ? LIMIT 1;";

constexpr const char* kLastInsertRowIdSql = "SELECT last_insert_rowid();";
}

UserRepository::UserRepository(std::string databasePath) : databasePath(std::move(databasePath)) {}

bool UserRepository::EnsureDefaultData() const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db.exec(kCreateUsersTableSql);

        SQLite::Statement countStmt(db, kCountUsersSql);
        const int existingUsers = countStmt.executeStep() ? countStmt.getColumn(0).getInt() : 0;
        if (existingUsers > 0) {
            return true;
        }

        const std::string defaultPasswordHash = PasswordHasher::HashPassword("123456");
        if (defaultPasswordHash.empty()) {
            return false;
        }

        SQLite::Transaction txn(db);
        {
            SQLite::Statement insert(db, kInsertUserSql);
            insert.bind(1, "demo");
            insert.bind(2, defaultPasswordHash);
            insert.bind(3, "DemoUser");
            insert.bind(4, 1);
            insert.exec();
        }
        {
            SQLite::Statement insert(db, kInsertUserSql);
            insert.bind(1, "tester");
            insert.bind(2, defaultPasswordHash);
            insert.bind(3, "Tester");
            insert.bind(4, 1);
            insert.exec();
        }
        txn.commit();

        return true;
    } catch (...) {
        return false;
    }
}

std::optional<UserRecord> UserRepository::FindByUsername(const std::string& username) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);
        SQLite::Statement query(db, kFindByUsernameSql);
        query.bind(1, username);

        if (!query.executeStep()) {
            return std::nullopt;
        }

        UserRecord user;
        user.id = query.getColumn(0).getInt();
        user.username = query.getColumn(1).getString();
        user.passwordHash = query.getColumn(2).getString();
        user.displayName = query.getColumn(3).getString();
        user.status = query.getColumn(4).getInt();
        return user;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<UserRecord> UserRepository::FindById(int32_t userId) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);
        SQLite::Statement query(db, kFindByIdSql);
        query.bind(1, userId);

        if (!query.executeStep()) {
            return std::nullopt;
        }

        UserRecord user;
        user.id = query.getColumn(0).getInt();
        user.username = query.getColumn(1).getString();
        user.passwordHash = query.getColumn(2).getString();
        user.displayName = query.getColumn(3).getString();
        user.status = query.getColumn(4).getInt();
        return user;
    } catch (...) {
        return std::nullopt;
    }
}

bool UserRepository::UsernameExists(const std::string& username) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READONLY);
        SQLite::Statement query(db, kUsernameExistsSql);
        query.bind(1, username);
        return query.executeStep() && query.getColumn(0).getInt() > 0;
    } catch (...) {
        return false;
    }
}

std::optional<UserRecord> UserRepository::CreateUser(const std::string& username, const std::string& passwordHash, const std::string& displayName) const {
    try {
        SQLite::Database db(databasePath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db.exec(kCreateUsersTableSql);

        SQLite::Statement insert(db, kInsertUserSql);
        insert.bind(1, username);
        insert.bind(2, passwordHash);
        insert.bind(3, displayName);
        insert.bind(4, 1);
        insert.exec();

        UserRecord user;
        user.id = static_cast<int32_t>(db.getLastInsertRowid());
        user.username = username;
        user.passwordHash = passwordHash;
        user.displayName = displayName;
        user.status = 1;
        return user;
    } catch (...) {
        return std::nullopt;
    }
}
