#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendAddResponse {
    uint8_t type = FRIEND_ADD_RES;
    uint8_t success = 0;
    int32_t errorCode = ERR_NONE;
    int32_t targetUserId = -1;
    char targetUsername[32] = {};
    char message[64] = {};
};
