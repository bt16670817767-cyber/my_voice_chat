#include "Server.h"

#include <atomic>
#include <cstdio>
#include <string>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace {
std::atomic<bool> shouldExit{false};

void signal_callback_handler(int) {
    shouldExit = true;
}

std::string FormatBytes(uint64 bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double value = static_cast<double>(bytes);
    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        unit++;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f %s", value, units[unit]);
    return buf;
}

bool HasTTY() {
#if defined(__linux__) || defined(__APPLE__)
    return isatty(fileno(stdin)) && isatty(fileno(stdout));
#else
    return true; // Windows: assume TTY
#endif
}
}  // namespace

int main(int argc, const char* argv[]) {
    signal(SIGINT, signal_callback_handler);
    signal(SIGTERM, signal_callback_handler);

    uint16 port = 27020;
    if (argc == 2) {
        port = static_cast<uint16>(std::stoi(argv[1]));
    }

    auto* server = new Server();
    const bool socket_success = server->StartServer(port);

    if (!socket_success) {
        printf("Failed to start server on port %u\n", port);
        delete server;
        return 1;
    }

    const bool useTUI = HasTTY();
    printf("Voice Chat Server starting on port %u (%s mode)\n",
        port, useTUI ? "TUI" : "headless");

    if (!useTUI) {
        // Headless mode: simple polling loop with periodic status
        uint64_t tickCounter = 0;
        while (!shouldExit.load()) {
            server->PollIncomingMessages();
            server->PollConnectionStateChanges();
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            ++tickCounter;
            if (tickCounter % 200 == 0) { // ~every 6 seconds
                printf("[server] users=%zu rooms=%zu sent=%s recv=%s\n",
                    server->GetConnectedUserCount(),
                    server->GetAllRooms().size(),
                    FormatBytes(server->GetSentBytes()).c_str(),
                    FormatBytes(server->GetRecievedBytes()).c_str());
                fflush(stdout);
            }
        }
        delete server;
        return 0;
    }

    // TUI mode with ftxui dashboard
    std::atomic<uint64> sentBytes{0};
    std::atomic<uint64> recvBytes{0};
    std::atomic<size_t> userCount{0};
    std::atomic<size_t> channelCount{0};
    std::atomic<size_t> roomCount{0};

    auto screen = ftxui::ScreenInteractive::TerminalOutput();

    std::thread network_loop([&] {
        while (!shouldExit.load()) {
            server->PollIncomingMessages();
            server->PollConnectionStateChanges();
            sentBytes.store(server->GetSentBytes());
            recvBytes.store(server->GetRecievedBytes());
            userCount.store(server->GetConnectedUserCount());
            channelCount.store(server->GetActiveChannels().size());
            roomCount.store(server->GetAllRooms().size());
            screen.PostEvent(ftxui::Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        screen.PostEvent(ftxui::Event::Custom);
    });

    int activeTab = 0;

    auto overviewRenderer = ftxui::Renderer([&] {
        return ftxui::vbox({
            ftxui::text(" Voice Chat Server ") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            ftxui::text(""),
            ftxui::text(" Port            : " + std::to_string(port)),
            ftxui::text(" Connected Users : " + std::to_string(userCount.load())),
            ftxui::text(" Rooms           : " + std::to_string(roomCount.load())),
            ftxui::text(""),
            ftxui::text(" Data Sent       : " + FormatBytes(sentBytes.load())),
            ftxui::text(" Data Received   : " + FormatBytes(recvBytes.load())),
        }) | ftxui::border;
    });

    auto usersRenderer = ftxui::Renderer([&] {
        auto users = server->GetConnectedUsers();
        ftxui::Elements elements;
        elements.push_back(ftxui::text(" Connected Users (" + std::to_string(users.size()) + ") ") | ftxui::bold);
        elements.push_back(ftxui::separator());
        if (users.empty()) {
            elements.push_back(ftxui::text(" No users connected.") | ftxui::dim);
        } else {
            for (const auto& u : users) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), " #%-8d %-20s %-20s %d rooms",
                    u.userId, u.username.c_str(), u.displayName.c_str(), u.activeRoomCount);
                elements.push_back(ftxui::text(buf));
            }
        }
        return ftxui::vbox(std::move(elements)) | ftxui::border;
    });

    auto roomsRenderer = ftxui::Renderer([&] {
        auto rooms = server->GetAllRooms();
        ftxui::Elements elements;
        elements.push_back(ftxui::text(" Rooms (" + std::to_string(rooms.size()) + ") ") | ftxui::bold);
        elements.push_back(ftxui::separator());
        if (rooms.empty()) {
            elements.push_back(ftxui::text(" No rooms created.") | ftxui::dim);
        } else {
            for (const auto& r : rooms) {
                char buf[256];
                std::snprintf(buf, sizeof(buf), " #%-7d %-26s %-8zu members",
                    r.roomId, r.roomName.c_str(), r.memberCount);
                elements.push_back(ftxui::text(buf));
            }
        }
        return ftxui::vbox(std::move(elements)) | ftxui::border;
    });

    auto logRenderer = ftxui::Renderer([&] {
        auto log = server->GetRecentLogEntries();
        ftxui::Elements elements;
        elements.push_back(ftxui::text(" Event Log ") | ftxui::bold);
        elements.push_back(ftxui::separator());
        size_t startIdx = (log.size() > 50) ? (log.size() - 50) : 0;
        for (size_t i = startIdx; i < log.size(); ++i) {
            elements.push_back(ftxui::text(" [" + log[i].timestamp + "] " + log[i].message));
        }
        return ftxui::vbox(std::move(elements)) | ftxui::border;
    });

    auto tabContainer = ftxui::Container::Tab({
        overviewRenderer, usersRenderer, roomsRenderer, logRenderer,
    }, &activeTab);

    auto tabToggle = ftxui::Container::Horizontal({
        ftxui::Button(" Overview  ", [&] { activeTab = 0; }),
        ftxui::Button(" Users     ", [&] { activeTab = 1; }),
        ftxui::Button(" Rooms     ", [&] { activeTab = 2; }),
        ftxui::Button(" Event Log ", [&] { activeTab = 3; }),
    });

    auto mainContainer = ftxui::Container::Vertical({ tabToggle, tabContainer });

    auto mainRenderer = ftxui::Renderer(mainContainer, [&] {
        return ftxui::vbox({
            ftxui::hbox({ tabToggle->Render() | ftxui::flex, }) | ftxui::border,
            tabContainer->Render() | ftxui::flex,
            ftxui::separator(),
            ftxui::hbox({
                ftxui::text(" q:Quit  Tab:Switch  ") | ftxui::dim,
                ftxui::text("Users:" + std::to_string(userCount.load()) + "  ") | ftxui::dim,
                ftxui::text("Sent:" + FormatBytes(sentBytes.load()) + "  ") | ftxui::dim,
                ftxui::text("Recv:" + FormatBytes(recvBytes.load())) | ftxui::dim,
            }),
        });
    });

    auto app = ftxui::CatchEvent(mainRenderer, [&](ftxui::Event event) {
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            shouldExit = true;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == ftxui::Event::Character('\t')) {
            activeTab = (activeTab + 1) % 4;
            return true;
        }
        return false;
    });

    screen.Loop(app);

    shouldExit = true;
    if (network_loop.joinable()) {
        network_loop.join();
    }

    delete server;
    return 0;
}
