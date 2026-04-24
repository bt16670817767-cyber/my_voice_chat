# SimpleVoiceChat 今日配置记录（2026-04-23）

本文档记录今天完成的两项工作：

1. 服务端接入 `FTXUI` 并使用 Docker 构建/运行  
2. 客户端接入 `ImGui + GLFW` 并完成本地构建/运行

---

## 1) 服务端配置（FTXUI + Docker）

### 本次完成内容

- 服务端 `CMake` 已加入 `ftxui` 依赖与链接：
  - `ftxui::screen`
  - `ftxui::dom`
  - `ftxui::component`
- 服务端 `main.cpp` 已改为 FTXUI 终端界面骨架（显示状态、端口、收发统计，支持 `q/Esc` 退出）。
- Docker 构建依赖补齐：在镜像中安装 `ftxui:x64-linux`（与 `gamenetworkingsockets:x64-linux` 一起）。
- 服务端 `vcpkg.json` 增加 `ftxui` 依赖。

### 服务端实际运行命令

#### A. 构建服务端镜像

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main
docker build -t voicechat-server-ftxui:latest .
```

#### B. 运行服务端容器（交互式，FTXUI 必须）

```powershell
docker run --rm -it -p 27020:27020/udp --name voicechat_server_ui voicechat-server-ftxui:latest
```

#### C. （可选）指定服务端端口

```powershell
docker run --rm -it -p 28000:28000/udp --name voicechat_server_ui voicechat-server-ftxui:latest ./VoiceChatServer 28000
```

#### D. 停止服务

- 在容器运行界面按 `q` / `Esc` / `Ctrl + C`

---

## 2) 客户端配置（ImGui + GLFW）

### 本次完成内容

- 客户端 `vcpkg.json` 新增依赖：
  - `imgui`（启用 `glfw-binding`、`opengl3-binding`）
  - `glfw3`
- 客户端 `CMakeLists.txt` 新增：
  - `find_package(imgui CONFIG REQUIRED)`
  - `find_package(glfw3 CONFIG REQUIRED)`
  - `find_package(OpenGL REQUIRED)`
  - 链接 `imgui::imgui`、`glfw`、`OpenGL::GL`
- 客户端 `main.cpp` 已改为 ImGui 窗口骨架，包含：
  - Server IP / Port / Channel 输入
  - Connect 按钮
  - Start Audio / Stop Audio 按钮
  - 连接状态与运行状态显示
- 已验证客户端编译成功，生成：
  - `e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient\build\Release\VoiceChatClient.exe`

### 客户端实际运行命令（Windows 本地）

#### A. 配置 + 编译

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="e:/my_voice_chat/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

#### B. 启动客户端（默认参数）

```powershell
e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient\build\Release\VoiceChatClient.exe
```

#### C. 启动客户端（自定义参数）

```powershell
e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient\build\Release\VoiceChatClient.exe 127.0.0.1 27020 0
```

---

## 联调最小命令顺序（建议）

1. 先启动服务端容器：

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main
docker run --rm -it -p 27020:27020/udp --name voicechat_server_ui voicechat-server-ftxui:latest
```

2. 再启动客户端：

```powershell
e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient\build\Release\VoiceChatClient.exe 127.0.0.1 27020 0
```

3. 在客户端 UI 中点击 `Connect`，连接后点击 `Start Audio`。

