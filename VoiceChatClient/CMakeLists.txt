cmake_minimum_required(VERSION 3.20)
project(VoiceChatClient)

find_package( GameNetworkingSockets CONFIG REQUIRED)
find_package(rtaudio CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)


set(CMAKE_CXX_STANDARD 17)

add_executable(VoiceChatClient main.cpp
        SocketClient.cpp
        SocketClient.h
        AudioTools.cpp
        AudioTools.h
        "../Common/Messages/AudioMessage.h" "../Common/Messages/SetChannelMessage.h" "../Common/Messages/MessageTypes.h" "Utils.h")

target_link_libraries(VoiceChatClient
        PRIVATE
        GameNetworkingSockets::shared
        RtAudio::rtaudio
        imgui::imgui
        glfw
        OpenGL::GL)