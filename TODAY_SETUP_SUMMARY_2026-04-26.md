# SimpleVoiceChat 今日配置记录（2026-04-26）

本文档记录今天完成的工作：

1. 登录系统一期打通：协议扩展、服务端连接态鉴权、客户端 ImGui 登录 UI、错误提示
2. 服务端账号存储升级：接入 SQLiteCpp + libsodium（Argon2id），实现真实落盘校验
3. Docker 构建链路修复：补齐 libsodium 需要的 autotools 依赖，镜像可稳定构建
4. 运行脚本拆分：将一键 demo 脚本拆为独立的服务端/客户端启动脚本，便于验收

---

## 1) 登录系统一期（协议 + 会话鉴权 + UI）

### 本次完成内容

- **协议扩展**：新增登录请求/响应类型，用于端到端登录链路。
  - `LOGIN_REQ`：客户端发送用户名/密码
  - `LOGIN_RES`：服务端返回成功/失败、错误码、用户信息（如 `userId`、`displayName`）
  - `SERVER_ERROR`：用于未登录访问业务消息等场景的明确错误提示

- **服务端连接态鉴权**（不在音频包携带 token）：
  - 维护 `HSteamNetConnection -> SessionInfo` 内存映射
  - 登录成功后将连接标记为 authenticated
  - 登录成功前拦截 `SET_CHANNEL` / `AUDIO` 并返回 `ERR_UNAUTHENTICATED`
  - 连接断开时自动清理会话映射，避免脏会话

- **客户端 ImGui 登录 UI**：
  - 登录面板：用户名/密码输入、登录按钮、状态提示
  - 登录成功前禁用频道切换与语音入口
  - 接收 `LOGIN_RES` / `SERVER_ERROR` 后更新状态与提示

---

## 2) 服务端账号落盘：SQLiteCpp + libsodium（Argon2id）

### 本次完成内容

- **SQLite 持久化**：服务端从“伪数据库文本文件”升级为真实 SQLite 数据库文件。
- **密码哈希**：使用 libsodium 内置的 Argon2id（`crypto_pwhash_str` / `crypto_pwhash_str_verify`）。
- **极简 DB 封装**（服务端 `Auth` 模块）：
  - 自动建表（若不存在）
  - 默认 seed 两个账号（用于验收）
  - 按用户名查询用户
  - 登录时校验明文密码与 DB 内的哈希是否匹配

### 默认验收账号

- 用户名：`demo`，密码：`123456`
- 用户名：`tester`，密码：`123456`

> 注：数据库里存储的是带盐哈希，不会存明文。

### 数据库文件位置

- 当前服务端在容器内工作目录 `/app` 下创建/使用：`users.sqlite`
- 若希望重启容器不丢数据，建议运行时对 DB 文件做 volume 挂载（见“可选：DB 持久化”）。

---

## 3) Docker 构建/运行（服务端）

### 本次完成内容

- `VoiceChatServer/vcpkg.json` 增加依赖：
  - `sqlitecpp`
  - `libsodium`
- `VoiceChatServer/CMakeLists.txt` 增加：
  - `find_package(SQLiteCpp CONFIG REQUIRED)`
  - `find_package(unofficial-sodium CONFIG REQUIRED)`
  - 并处理 SQLiteCpp 在不同环境下 target 名称不一致的问题（自动探测 target）
- Dockerfile（build stage）补齐 libsodium 需要的系统工具：
  - `autoconf`、`autoconf-archive`、`automake`、`libtool`

### 构建服务端镜像

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main
docker build -t voicechat-server-ftxui:latest .
```

### 运行服务端容器（交互式，FTXUI 必须）

```powershell
docker run --rm -it -p 27020:27020/udp --name voicechat_server_ui voicechat-server-ftxui:latest
```

### （可选）DB 持久化：挂载 users.sqlite 到宿主机

示例（将 DB 存在宿主机 `E:\my_voice_chat\SimpleVoiceChat-main\VoiceChatServer\data`）：

```powershell
mkdir e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatServer\data -Force

docker run --rm -it \
  -p 27020:27020/udp \
  -v e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatServer\data:/app/data \
  --name voicechat_server_ui \
  voicechat-server-ftxui:latest
```

> 若采用挂载方式，需要把服务端数据库路径从 `users.sqlite` 切到 `/app/data/users.sqlite`（可后续做参数化）。

---

## 4) 客户端构建/运行（Windows 本地）

### 本次完成内容

- 客户端可继续本地编译运行。
- 注意：vcpkg 的工具链路径应指向 `e:/my_voice_chat/vcpkg/...`（vcpkg 目录与 `SimpleVoiceChat-main` 同级）。

### 配置 + 编译

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="e:/my_voice_chat/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

### 启动客户端

```powershell
e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient\build\Release\VoiceChatClient.exe 127.0.0.1 27020 0
```

---

## 5) 运行脚本（拆分版）

### 本次完成内容

- 将原 `run_mvp_demo.ps1` 拆分为两个脚本，便于分别控制：
  - `start-server.ps1`：启动服务端 Docker 容器
  - `start-client.ps1`：启动一个客户端实例（可运行两次启动 A/B）

### 推荐验收顺序

1. 先启动服务端：

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main
.\start-server.ps1
```

2. 再启动客户端（开两个客户端则执行两次）：

```powershell
.\start-client.ps1
.\start-client.ps1
```

3. 在客户端登录界面输入默认账号并登录，然后再进入频道并开语音。

---

## 6) 常见问题

### VS Code 里服务端代码报红，但 Docker 里能跑

- 原因：Docker 编译的是 Linux 环境；VS Code 的 IntelliSense 用的是本机 Windows includePath/compile database。
- 建议：本机为 `VoiceChatServer` 生成 `compile_commands.json` 并让 VS Code 指向它，可消除 `sodium.h` / `SQLiteCpp` 头文件报错。
