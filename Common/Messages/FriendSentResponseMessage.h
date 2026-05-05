#pragma once
#include "MessageTypes.h"
#include <cstdint>

#define MAX_SENT_REQUESTS 20

struct FriendSentResponse {
    uint8_t type = FRIEND_SENT_RES;
    uint8_t count = 0;
    int32_t userIds[MAX_SENT_REQUESTS] = {};
    char usernames[MAX_SENT_REQUESTS][32] = {};
    char displayNames[MAX_SENT_REQUESTS][32] = {};
};
