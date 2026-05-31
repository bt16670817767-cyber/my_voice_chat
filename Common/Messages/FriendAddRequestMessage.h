#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendAddRequest {
    uint8_t type = FRIEND_ADD_REQ;
    int32_t targetUserId = -1;
};
