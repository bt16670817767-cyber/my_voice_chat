#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct RoomLeaveRequest {
    uint8_t type = ROOM_LEAVE_REQ;
    int32_t roomId = -1;
};
