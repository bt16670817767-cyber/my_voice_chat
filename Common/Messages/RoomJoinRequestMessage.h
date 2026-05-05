#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct RoomJoinRequest {
    uint8_t type = ROOM_JOIN_REQ;
    int32_t roomId = -1;
    char password[32] = {};
};
