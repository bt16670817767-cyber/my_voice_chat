#pragma once
#include "MessageTypes.h"
#include <cstdint>

#define MAX_ROOMS_PER_MSG 10

struct RoomListResponse {
    uint8_t type = ROOM_LIST_RES;
    uint8_t roomCount = 0;
    int32_t roomIds[MAX_ROOMS_PER_MSG] = {};
    int64 channels[MAX_ROOMS_PER_MSG] = {};
    char roomNames[MAX_ROOMS_PER_MSG][32] = {};
    int32_t ownerUserIds[MAX_ROOMS_PER_MSG] = {};
    uint8_t memberCounts[MAX_ROOMS_PER_MSG] = {};
    uint8_t isJoined[MAX_ROOMS_PER_MSG] = {};
    uint8_t isInvited[MAX_ROOMS_PER_MSG] = {};
};
