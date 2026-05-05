#pragma once
#include "MessageTypes.h"

struct RegisterRequest {
    uint8_t type = REGISTER_REQ;
    char username[32] = {};
    char password[64] = {};
    char displayName[32] = {};
};
