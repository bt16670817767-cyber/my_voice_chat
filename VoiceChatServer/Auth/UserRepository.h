#pragma once

#include <optional>
#include <string>

struct UserRecord {
    int32_t id = -1;
    std::string username;
    std::string passwordHash;
    std::string displayName;
    int status = 1;
};

class UserRepository {
public:
    explicit UserRepository(std::string databasePath);
    std::optional<UserRecord> FindByUsername(const std::string& username) const;
    std::optional<UserRecord> FindById(int32_t userId) const;
    bool EnsureDefaultData() const;
    bool UsernameExists(const std::string& username) const;
    std::optional<UserRecord> CreateUser(const std::string& username, const std::string& passwordHash, const std::string& displayName) const;

private:
    std::string databasePath;
};
