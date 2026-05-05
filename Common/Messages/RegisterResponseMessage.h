#pragma once
#include "MessageTypes.h"
#include <cstdint>

struct RegisterResponse {
    uint8_t type = REGISTER_RES;
    uint8_t success = 0;
    int32_t errorCode = ERR_NONE;
    int32_t userId = -1;
    char message[96] = {};
};
