#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendRequestNotify {
    uint8_t type = FRIEND_REQUEST_NOTIFY;
    int32_t fromUserId = -1;
    char fromUsername[32] = {};
    char fromDisplayName[32] = {};
};
