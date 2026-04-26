#pragma once

#include <string>

class PasswordHasher {
public:
    static std::string Hash(const std::string& plainText);
    static bool Verify(const std::string& plainText, const std::string& expectedHash);
};
