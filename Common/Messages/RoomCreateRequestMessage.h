#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct RoomCreateRequest {
    uint8_t type = ROOM_CREATE_REQ;
    char roomName[32] = {};
    char password[32] = {};
};
