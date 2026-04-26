#pragma once

#include "MessageTypes.h"

struct LoginRequest
{
    uint8_t type = LOGIN_REQ;
    char username[32] = {0};
    char password[64] = {0};
};
