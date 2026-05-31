#pragma once

#include "MessageTypes.h"
#include <cstdint>

struct LoginResponse
{
    uint8_t type = LOGIN_RES;
    uint8_t success = 0;
    int32_t errorCode = ERR_NONE;
    int32_t userId = -1;
    char displayName[32] = {0};
    char message[96] = {0};
};
