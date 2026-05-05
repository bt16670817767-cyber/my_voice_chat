#pragma once
#include "MessageTypes.h"
#include <cstdint>

#define MAX_SEARCH_RESULTS 5

struct FriendSearchResult {
    uint8_t type = FRIEND_SEARCH_RES;
    uint8_t resultCount = 0;
    int32_t userIds[MAX_SEARCH_RESULTS] = {};
    char usernames[MAX_SEARCH_RESULTS][32] = {};
    char displayNames[MAX_SEARCH_RESULTS][32] = {};
    uint8_t relationshipStatus[MAX_SEARCH_RESULTS] = {}; // 0=none, 1=friend, 2=outgoing, 3=incoming
    uint8_t isOnline[MAX_SEARCH_RESULTS] = {};
};
