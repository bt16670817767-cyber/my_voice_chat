#include "Server.h"

int main() {
    // 在 27020 端口启动 WebSocket 服务器
    Server server(27020);
    server.Run();
    return 0;
}