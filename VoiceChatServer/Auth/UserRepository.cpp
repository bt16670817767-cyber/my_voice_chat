#include "UserRepository.h"

#include <fstream>
#include <sstream>

namespace {
constexpr const char* kDefaultUsersData =
    "1|demo|c359fb12feabd1d4|DemoUser|1\n"
    "2|tester|1ee9efd3d1cbf123|Tester|1\n";
}

UserRepository::UserRepository(std::string databasePath) : databasePath(std::move(databasePath)) {}

bool UserRepository::EnsureDefaultData() const {
    std::ifstream existing(databasePath);
    if (existing.good() && existing.peek() != std::ifstream::traits_type::eof()) {
        return true;
    }

    std::ofstream output(databasePath, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << kDefaultUsersData;
    return true;
}

std::vector<UserRecord> UserRepository::LoadUsers() const {
    std::vector<UserRecord> users;
    std::ifstream input(databasePath);
    if (!input.is_open()) {
        return users;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string part;
        UserRecord user;

        if (!std::getline(ss, part, '|')) continue;
        user.id = std::stoi(part);
        if (!std::getline(ss, user.username, '|')) continue;
        if (!std::getline(ss, user.passwordHash, '|')) continue;
        if (!std::getline(ss, user.displayName, '|')) continue;
        if (!std::getline(ss, part, '|')) continue;
        user.status = std::stoi(part);
        users.push_back(user);
    }
    return users;
}

std::optional<UserRecord> UserRepository::FindByUsername(const std::string& username) const {
    const auto users = LoadUsers();
    for (const auto& user : users) {
        if (user.username == username) {
            return user;
        }
    }
    return std::nullopt;
}
