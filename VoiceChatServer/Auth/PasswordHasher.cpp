#include "PasswordHasher.h"

#include <sodium.h>

std::string PasswordHasher::HashPassword(const std::string& raw_pass) {
    if (sodium_init() < 0) {
        return {};
    }

    char hashed[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(
            hashed,
            raw_pass.c_str(),
            static_cast<unsigned long long>(raw_pass.size()),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        return {};
    }

    return std::string(hashed);
}

bool PasswordHasher::VerifyPassword(const std::string& raw_pass, const std::string& db_hash) {
    if (db_hash.empty()) {
        return false;
    }
    if (sodium_init() < 0) {
        return false;
    }

    return crypto_pwhash_str_verify(
               db_hash.c_str(),
               raw_pass.c_str(),
               static_cast<unsigned long long>(raw_pass.size())) == 0;
}

std::string PasswordHasher::Hash(const std::string& plainText) {
    return HashPassword(plainText);
}

bool PasswordHasher::Verify(const std::string& plainText, const std::string& expectedHash) {
    return VerifyPassword(plainText, expectedHash);
}
