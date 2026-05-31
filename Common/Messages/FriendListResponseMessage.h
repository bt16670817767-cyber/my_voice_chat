#pragma once
#include "MessageTypes.h"
#include <cstdint>

#define MAX_FRIENDS_PER_MSG 20

struct FriendListResponse {
    uint8_t type = FRIEND_LIST_RES;
    uint8_t friendCount = 0;
    int32_t userIds[MAX_FRIENDS_PER_MSG] = {};
    char usernames[MAX_FRIENDS_PER_MSG][32] = {};
    char displayNames[MAX_FRIENDS_PER_MSG][32] = {};
    uint8_t isOnline[MAX_FRIENDS_PER_MSG] = {};
};
