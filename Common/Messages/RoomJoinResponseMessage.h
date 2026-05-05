#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct RoomJoinResponse {
    uint8_t type = ROOM_JOIN_RES;
    uint8_t success = 0;
    int32_t errorCode = ERR_NONE;
    int32_t roomId = -1;
    int64 channel = 0;
    char roomName[32] = {};
    char message[64] = {};
};
