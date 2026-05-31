#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendAcceptResponse {
    uint8_t type = FRIEND_ACCEPT_RES;
    uint8_t success = 0;
    int32_t errorCode = ERR_NONE;
    int32_t friendUserId = -1;
    char friendDisplayName[32] = {};
    char message[64] = {};
};
