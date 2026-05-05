#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct FriendOnlineNotify {
    uint8_t type = FRIEND_ONLINE_NOTIFY;
    int32_t userId = -1;
    uint8_t isOnline = 0;
    char displayName[32] = {};
};
