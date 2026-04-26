#include "PasswordHasher.h"

#include <cstdint>
#include <sstream>

std::string PasswordHasher::Hash(const std::string& plainText) {
    // Lightweight deterministic hash for phase 1.
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : plainText) {
        hash ^= c;
        hash *= 1099511628211ull;
    }

    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

bool PasswordHasher::Verify(const std::string& plainText, const std::string& expectedHash) {
    return Hash(plainText) == expectedHash;
}
