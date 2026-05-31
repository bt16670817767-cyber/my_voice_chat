#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendRejectResponse {
    uint8_t type = FRIEND_REJECT_RES;
    uint8_t success = 0;
    int32_t errorCode = ERR_NONE;
    char message[64] = {};
};
