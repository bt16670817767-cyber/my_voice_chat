#pragma once

#include "MessageTypes.h"
#include <cstdint>

struct ServerErrorMessage
{
    uint8_t type = SERVER_ERROR;
    int32_t errorCode = ERR_NONE;
    char message[96] = {0};
};
