#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct RoomInviteNotify {
    uint8_t type = ROOM_INVITE_NOTIFY;
    int32_t roomId = -1;
    int64 channel = 0;
    int32_t fromUserId = -1;
    char roomName[32] = {};
    char fromDisplayName[32] = {};
};
