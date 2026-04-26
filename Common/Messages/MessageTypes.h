#pragma once

#include <steam/steamnetworkingsockets.h>


enum MessageType
{
	SET_CHANNEL = 1,
	AUDIO = 2,
	LOGIN_REQ = 3,
	LOGIN_RES = 4,
	SERVER_ERROR = 5
};

enum ServerErrorCode
{
	ERR_NONE = 0,
	ERR_UNAUTHENTICATED = 1,
	ERR_ALREADY_AUTHENTICATED = 2,
	ERR_INVALID_REQUEST = 3
};