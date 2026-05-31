#pragma once

#include <string>

class PasswordHasher {
public:
    // Phase 2: use libsodium's crypto_pwhash_str (Argon2id) for salted password hashing.
    static std::string HashPassword(const std::string& raw_pass);
    static bool VerifyPassword(const std::string& raw_pass, const std::string& db_hash);

    // Backward-compatible wrappers.
    static std::string Hash(const std::string& plainText);
    static bool Verify(const std::string& plainText, const std::string& expectedHash);
};
