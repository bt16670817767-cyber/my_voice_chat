#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct RoomInviteRequest {
    uint8_t type = ROOM_INVITE_REQ;
    int32_t roomId = -1;
    int32_t targetUserId = -1;
};
