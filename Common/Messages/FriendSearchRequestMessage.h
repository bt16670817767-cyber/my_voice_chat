#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendSearchRequest {
    uint8_t type = FRIEND_SEARCH_REQ;
    char query[32] = {};
};
