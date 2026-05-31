#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendRemoveRequest {
    uint8_t type = FRIEND_REMOVE_REQ;
    int32_t friendUserId = -1;
};
