#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendAcceptRequest {
    uint8_t type = FRIEND_ACCEPT_REQ;
    int32_t fromUserId = -1;
};
