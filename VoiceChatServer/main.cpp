#include "Server.h"

#include <atomic>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace {
std::atomic<bool> shouldExit{false};

void signal_callback_handler(int) {
    shouldExit = true;
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

    std::atomic<uint16> sent_kb{0};
    std::atomic<uint16> received_kb{0};

    auto screen = ftxui::ScreenInteractive::TerminalOutput();

    std::thread network_loop([&] {
        while (!shouldExit.load()) {
            server->PollIncomingMessages();
            server->PollConnectionStateChanges();
            sent_kb.store(server->GetSentBytes());
            received_kb.store(server->GetRecievedBytes());
            screen.PostEvent(ftxui::Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    });

    auto renderer = ftxui::Renderer([&] {
        const bool running = !shouldExit.load();
        const std::string status = running ? "RUNNING" : "STOPPING";
        return ftxui::vbox({
                   ftxui::text("Voice Chat Server") | ftxui::bold,
                   ftxui::separator(),
                   ftxui::text("Status        : " + status),
                   ftxui::text("Listen Port   : " + std::to_string(port)),
                   ftxui::text("Received (Kb) : " + std::to_string(received_kb.load())),
                   ftxui::text("Sent (Kb)     : " + std::to_string(sent_kb.load())),
                   ftxui::separator(),
                   ftxui::text("Press q or Esc to shutdown"),
               }) |
               ftxui::border;
    });

    auto app = ftxui::CatchEvent(renderer, [&](ftxui::Event event) {
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            shouldExit = true;
            screen.ExitLoopClosure()();
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