#pragma once
#include "MessageTypes.h"
#include <cstdint>

#define MAX_ROOM_MEMBERS 10

struct RoomMemberUpdate {
    uint8_t type = ROOM_MEMBER_UPDATE;
    int32_t roomId = -1;
    int32_t roomChannel = 0;
    uint8_t memberCount = 0;
    int32_t userIds[MAX_ROOM_MEMBERS] = {};
    char displayNames[MAX_ROOM_MEMBERS][32] = {};
};
