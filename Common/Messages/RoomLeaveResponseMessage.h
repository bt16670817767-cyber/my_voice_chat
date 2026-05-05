#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct RoomLeaveResponse {
    uint8_t type = ROOM_LEAVE_RES;
    uint8_t success = 0;
    int32_t errorCode = ERR_NONE;
    char message[64] = {};
};
