#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendRejectRequest {
    uint8_t type = FRIEND_REJECT_REQ;
    int32_t fromUserId = -1;
};
