#pragma once

#include <optional>
#include <string>
#include <vector>

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
    bool EnsureDefaultData() const;

private:
    std::string databasePath;
    std::vector<UserRecord> LoadUsers() const;
};
