//
// Created by Amin on 10/29/23.
//

#pragma once

#include "../Common/Messages/AudioMessage.h"
#include "SocketClient.h"
#include "rtaudio/RtAudio.h"


class AudioTools {
private:
    RtAudio audio;

public:
    bool StartRecording(SocketClient * socket, NetworkBuffer* networkBuffer);
    bool StopRecording();
    static float GetPeakLevel();
    ~AudioTools();
};

