# SimpleVoiceChat 今日工作记录（2026-05-05）

本文档记录今天完成的客户端与服务端 UI 全面改造工作。

---

## 1) 服务端基础设施增强

### 本次完成内容

- **字节计数器类型修复**：
  - `Server.h` / `Server.cpp`：`sentBytesCount` / `receivedBytesCount` 从 `uint16`（65KB 溢出）改为 `uint64`
  - `GetSentBytes()` / `GetRecievedBytes()` 返回原始字节值，TUI 层用 `FormatBytes()` 格式化为 B/KB/MB/GB

- **新增 Server 公开数据查询接口**：
  - `GetConnectedUserCount()` / `GetConnectedUsers()` — 已连接用户列表（userId、username、displayName、房间数）
  - `GetActiveChannels()` — 活跃频道及每频道用户数
  - `GetAllRooms()` — 所有房间列表（通过 RoomManager 间接获取）
  - `GetRecentLogEntries()` / `AddLogEntry()` — 事件日志系统（最多 200 条，环形淘汰）

- **RoomManager 补充接口**：
  - 新增 `GetAllRooms()` 方法，返回所有房间信息

- **事件日志埋点**：
  - 连接/断开 (`OnSteamNetConnectionStatusChanged`)
  - 登录成功/失败 (`HandleLoginRequest`)
  - 注册成功 (`HandleRegisterRequest`)

### 新建 struct（Server.h）

| 结构体 | 字段 | 用途 |
|--------|------|------|
| `UserInfo` | userId, username, displayName, activeRoomCount | TUI 用户列表 |
| `ChannelInfo` | channelId, connectionCount | TUI 频道列表 |
| `RoomSummary` | roomId, roomName, memberCount, ownerUserId | TUI 房间列表 |
| `LogEntry` | timestamp ("HH:MM:SS"), message | TUI 事件日志 |

---

## 2) 服务端 FTXUI 多标签页界面

### 本次完成内容

将服务端 TUI 从 6 行静态文本重写为 5 个交互式标签页：

| 标签页 | 内容 | FTXUI 组件 |
|--------|------|------------|
| **Overview** | 运行状态、端口、连接数/频道数/房间数、收发流量 | vbox / separator / bold / border |
| **Users** | 表格：UserID、Username、Display Name、房间数 | separatorLight / dim |
| **Channels** | 频道列表 + 每频道用户数 | 同上 |
| **Rooms** | 表格：RoomID、Name、Members、Owner | 同上 |
| **Event Log** | `[HH:MM:SS] message` 最近 50 条事件 | 同上 |

- 标签切换：Tab 键 + 点击 Button
- 底部状态栏：快捷键提示 + 在线用户数 + 实时流量
- 网络轮询线程 30ms 间隔通过 `screen.PostEvent(Event::Custom)` 触发 TUI 刷新
- 数据原子快照传输（`std::atomic<uint64>` / `std::atomic<size_t>`）

### 默认验收方式

- 启动服务端容器，终端应显示 5 个标签按钮 + Overview 默认页
- 用客户端登录后，切换到 Users 标签页应看到已连接用户
- 客户端加入频道后，Channels 标签页应显示活跃频道
- 客户端创建房间后，Rooms 标签页应显示房间
- Event Log 标签页应有连接/登录/注册事件记录

---

## 3) 客户端基础架构改造

### 本次完成内容

- **Buffer 生命周期 bug 修复**：
  - `new_room_name[32]` 从 `if (ImGui::BeginTabItem("Rooms"))` 块内部移到 `main()` 函数作用域顶层
  - 删除未使用的 `invite_user_id_buf`

- **自定义主题**（新文件 `UITheme.h`）：
  - 暗色聊天应用配色：深蓝灰背景、蓝色系按钮、高对比度文字
  - 圆角样式：WindowRounding/FramRounding/GrabRounding/TabRounding/ChildRounding = 4~6px
  - 9 个语义颜色常量：`COLOR_ONLINE`(绿)、`COLOR_OFFLINE`(灰)、`COLOR_SPEAKING`(黄)、`COLOR_INVITED`(橙)、`COLOR_JOINED`(绿)、`COLOR_ERROR`(红)、`COLOR_ACCENT`(蓝)、`COLOR_HEADER`(浅蓝)

- **通知系统**（新文件 `UINotifications.h`）：
  - 时间衰减通知队列（最多 5 条，默认 5 秒生命周期）
  - 右上角独立 ImGui 浮窗渲染（NoTitleBar | NoInputs），不干扰主窗口
  - 全局函数：`PushNotification(text, color, lifetime)` / `RenderNotifications(deltaTime)`
  - 替换了原有的 `status_text = n` 单条覆盖逻辑

- **确认对话框**（新文件 `UIConfirmDialogs.h`）：
  - `ConfirmDialog(id, message)` — 模态 Yes/No 弹出确认
  - 用于：删除好友确认、离开房间确认

- **窗口自适应**：
  - `glfwSetWindowSizeLimits(window, 800, 500, GLFW_DONT_CARE, GLFW_DONT_CARE)`
  - 主窗口全屏填充：`SetNextWindowPos(0,0)` + `SetNextWindowSize(display_w, display_h)` + `NoResize | NoMove | NoCollapse | NoTitleBar`
  - `glfwGetFramebufferSize` 移到 `ImGui::NewFrame()` 之前，使窗口尺寸在渲染时可获取
  - 所有列表使用 `ImGui::BeginChild` 可滚动区域

---

## 4) 客户端 UI 模块化提取

### 本次完成内容

将 `main.cpp` 中约 430 行单一 render 循环拆分为 8 个 header-only 模块文件：

| 文件 | 函数 | 职责 |
|------|------|------|
| `UITheme.h` | `ApplyCustomTheme()` | 自定义 ImGui 样式 + 颜色常量 |
| `UINotifications.h` | `PushNotification()` / `RenderNotifications()` | 通知队列 + 浮窗 |
| `UIConfirmDialogs.h` | `ConfirmDialog()` | 模态确认弹窗 |
| `UIStatusBar.h` | `RenderStatusBar()` | 底部单行彩色状态栏 |
| `UIAuthPanel.h` | `RenderAuthPanel()` | 登录/注册左右分栏 |
| `UIVoiceTab.h` | `RenderVoiceTab()` | 语音标签（频道 + Start/Stop + VU 表） |
| `UIFriendsTab.h` | `RenderFriendsTab()` | 好友标签（搜索 + 列表 + 待处理 + 确认弹窗） |
| `UIRoomsTab.h` | `RenderRoomsTab()` | 房间标签（TreeNode + 邀请 + 确认弹窗） |

### main.cpp 精简结果

- 从 ~430 行缩减到 ~255 行
- 主循环结构清晰：`连接区 → 认证判断 → TabBar → RenderXxxTab → 状态栏 → 通知浮窗`
- 添加 `deltaTime` 计算（用于通知衰减）

### 各模块改动要点

**认证面板 (UIAuthPanel.h)**
- Login / Register 改为左右分栏（`BeginChild` + `SameLine`），各带独立标题色和圆角边框
- Register 客户端侧空输入校验，错误提示输出到 `auth_status_text`

**语音标签 (UIVoiceTab.h)**
- VU 电平表：`ImGui::ProgressBar` 显示 `AudioTools::GetPeakLevel()` 实时麦克风峰值
- Start/Stop Voice 按钮 + 彩色状态文字 `[Voice Active]`

**好友标签 (UIFriendsTab.h)**
- 搜索结果：`BeginChild` 可滚动（高 110px）
- 好友列表：`BeginChild` 填充剩余空间
- 待处理请求：`BeginChild` 可滚动（高 70px）+ 数量 badge
- 删除好友 → `ConfirmDialog` 确认弹出
- 通知从 `DrainNotifications()` 转入 `PushNotification()` 浮窗

**房间标签 (UIRoomsTab.h)**
- 房间列表：`BeginChild` + `TreeNodeEx` 展开/折叠
- 展开后垂直排列 Leave / Invite / Start Voice，不再横向挤压
- 离开 → 停止录音 → `ConfirmDialog` 确认 → 发送请求 → 刷新列表
- 邀请 → `BeginPopup` 显示在线好友 → 点击邀请
- 房间成员：底部 `BeginChild` 可滚动

---

## 5) 音频 VU 表增强

### 本次完成内容

- **AudioTools.h**：新增 `static float GetPeakLevel()` 方法
- **AudioTools.cpp**：
  - 新增 `static std::atomic<float> g_PeakLevel{0.0f}`
  - 在 `record()` 回调中计算输入 buffer 的归一化峰值（SINT16 → [-1, 1]）
  - `GetPeakLevel()` 返回峰值并清零（exchange with 0.0f），确保每帧独立
- **UIVoiceTab.h**：用 `AudioTools::GetPeakLevel()` 替代原来的 buffer 大小估算

---

## 6) 文件变更清单

### 新建文件（8 个）

| 文件 | 行数 | 说明 |
|------|------|------|
| `VoiceChatClient/UITheme.h` | 45 | 自定义 ImGui 暗色主题 + 9 个颜色常量 |
| `VoiceChatClient/UINotifications.h` | 40 | 通知队列系统（5 条上限，5s 衰减） |
| `VoiceChatClient/UIConfirmDialogs.h` | 20 | 模态 Yes/No 确认弹窗 |
| `VoiceChatClient/UIStatusBar.h` | 18 | 单行彩色状态栏 |
| `VoiceChatClient/UIAuthPanel.h` | 50 | 登录/注册左右分栏面板 |
| `VoiceChatClient/UIVoiceTab.h` | 57 | 语音标签 + VU 电平表 |
| `VoiceChatClient/UIFriendsTab.h` | 105 | 好友标签（可滚动 + 确认弹窗 + 通知） |
| `VoiceChatClient/UIRoomsTab.h` | 140 | 房间标签（TreeNode + 滚动 + 确认弹窗） |

### 修改文件（8 个）

| 文件 | 主要变更 |
|------|---------|
| `VoiceChatServer/Server.h` | +4 struct、+6 公开方法、`uint16`→`uint64`、事件日志成员 |
| `VoiceChatServer/Server.cpp` | +5 处埋点、+100 行实现（GetConnectedUsers/GetActiveChannels/GetAllRooms/日志） |
| `VoiceChatServer/RoomManager.h` | +`GetAllRooms()` 声明 |
| `VoiceChatServer/RoomManager.cpp` | +`GetAllRooms()` 实现 |
| `VoiceChatServer/main.cpp` | 完全重写：5 标签页 FTXUI（~270 行） |
| `VoiceChatClient/main.cpp` | 完全重写：模块化 UI 调用 + 自适应窗口 + 通知系统（~255 行） |
| `VoiceChatClient/AudioTools.h` | +`GetPeakLevel()` 静态方法 |
| `VoiceChatClient/AudioTools.cpp` | +峰值采集 +`GetPeakLevel()` 实现 |
| `VoiceChatClient/CMakeLists.txt` | +8 个新 header 文件 |
| `Dockerfile` | 移除冗余 `apt-get install libssl3`（已内置） |

---

## 7) 构建/运行

### 服务端 Docker 镜像

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main
docker build -t voicechat-server-ftxui:latest .
```

### 启动服务端

```
docker run --rm -it -p 27020:27020/udp --name voicechat_server_ui voicechat-server-ftxui:latest ./VoiceChatServer 27020
```

或使用脚本：

```powershell
.\start-server.ps1
```

### 客户端构建

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient\build
cmake --build . --config Release
```

### 客户端启动

```powershell
.\VoiceChatClient\build\Release\VoiceChatClient.exe 127.0.0.1 27020 0
```

或使用脚本：

```powershell
.\start-client.ps1 -ServerIp 127.0.0.1 -Port 27020 -Channel 0
```

---

## 8) 推荐验收顺序

### 服务端 TUI
1. 构建并启动服务端 Docker 容器
2. 验证终端显示 5 个标签按钮 + Overview 页面
3. 按 Tab 键切换标签页，确认 Users/Channels/Rooms/Event Log 均可切换
4. 客户端连接后检查 Users 标签页数据、Event Log 标签页日志

### 客户端 UI
1. 启动客户端，验证窗口可拖拽缩放（最小 800×500）
2. 验证暗色主题生效（深蓝灰背景、蓝色按钮、圆角）
3. 连接服务端，验证 Login/Register 左右分栏显示
4. 登录后验证 3 个标签页切换正常
5. 在 Voice Chat 标签验证 VU 表有活动指示
6. 在 Friends 标签验证好友列表可滚动 + 删除好友弹确认框
7. 在 Rooms 标签验证 TreeNode 展开/折叠
8. 做破坏性操作（删除好友、离开房间），验证确认弹窗 Yes/No 都正常
9. 验证右上角通知浮窗弹出和衰减

### 完整流程
1. 服务端启动 → 客户端 A 注册 → 登录
2. 客户端 B 用 demo 登录
3. A 搜索 demo → 添加好友 → B 接受
4. A 创建房间 → 邀请 B → B 加入
5. A/B 分别 Start Voice → 语音互通
6. A 离开房间（确认弹窗）→ 删除好友（确认弹窗）
7. 关闭客户端和服务端

---

## 9) 默认验收账号

- 用户名：`demo`，密码：`123456`
- 用户名：`tester`，密码：`123456`

---

## 10) 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 客户端模块拆分 | header-only（不新增 .cpp） | 保持 CMakeLists.txt 简洁，`#include` 即用 |
| 通知系统 | 独立 ImGui 浮窗 | 不干扰主窗口布局，可自由定位（右上角） |
| VU 表 | `atomic<float>` exchange | 峰值即读即清零，避免帧间累积 |
| 服务端 TUI | `Container::Tab` + Button | FTXUI 原生支持，键盘+鼠标双通道切换 |
| 事件日志 | 服务端内部 vector，TUI 只读 | 日志和 TUI 解耦，`AddLogEntry` static 方法可在任何 handler 中调用 |
| 服务端数据查询 | 主线程每帧调用 getter | FTXUI 轮询模式，mutex 保护，简单可靠 |
| 窗口自适应 | SetNextWindowSize 动态匹配 framebuffer | 保持 full-window 体验，min 800×500 防止内容断裂 |
| Docker libssl3 | 移除 apt-get 安装 | ubuntu:22.04 已内置 libssl3，移除后避免 archive.ubuntu.com 网络超时 |

---

## 11) 客户端 UI 布局对照

### 整体布局

```
┌─ Voice Chat Controls ───────────────────────────────────────────┐
│ Server IP: [127.0.0.1]   Port: [27020]     [Connect]           │
│────────────────────────────────────────────────────────────────│
│                                                                  │
│  [Tab: Voice Chat]  [Tab: Friends]  [Tab: Rooms]                │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  (当前标签页内容)                                           │ │
│  │                                                             │ │
│  └────────────────────────────────────────────────────────────┘ │
│────────────────────────────────────────────────────────────────│
│ CONNECTED  |  Auth: Yes  |  Mic: ON  |  status_text             │
└──────────────────────────────────────────────────────────────────┘
                                                  ┌─ ##Notifications ─┐
                                                  │ New friend added   │
                                                  │ Room created       │
                                                  └────────────────────┘
```

### 认证面板（未登录时）

```
┌─ LoginPanel ────────────┐  ┌─ RegisterPanel ────────────┐
│ Login                   │  │ Register                   │
│ ─────────────────────── │  │ ────────────────────────── │
│ Username: [        ]    │  │ New Username: [       ]    │
│ Password: [********]    │  │ New Password: [*******]    │
│ [   Login   ]           │  │ Display Name: [        ]   │
│                         │  │ [  Register  ]             │
└─────────────────────────┘  └────────────────────────────┘
```

### 语音标签页

```
│ Voice Chat                                       │
│ ──────────────────────────────────────────────── │
│ Channel: [0    ]                                  │
│ Mic Level: [███████░░░░░░░░░░░░░] 0.35            │
│ [   Start Voice   ]                               │
│ ──────────────────────────────────────────────── │
│ Channel 0                                         │
```

### 好友标签页

```
│ Search: [________]  [Search]  [Refresh List]     │
│ ╔══ Search Results ═════════════════════════════╗│
│ ║ Alice (alice)  [Add]  (Online)                ║│
│ ║ Bob (bob)      [Accept] [Reject]  (Online)    ║│
│ ╚════════════════════════════════════════════════╝│
│ ╔══ My Friends ═════════════════════════════════╗│
│ ║ Carol               Online   [Remove]          ║│
│ ║ Dave                Offline  [Remove]          ║│
│ ╚════════════════════════════════════════════════╝│
│ Pending Friend Requests (1):                      │
│ ╔══ Pending ════════════════════════════════════╗│
│ ║ Eve (eve)              [Accept] [Reject]       ║│
│ ╚════════════════════════════════════════════════╝│
```

### 房间标签页

```
│ [Refresh Rooms]                                  │
│ ════════════════════════════════════════════════ │
│ Create Room:                                     │
│ Room Name: [________]  [Create]                  │
│ ════════════════════════════════════════════════ │
│ My Rooms:                                        │
│ ╔══ RoomListScroll ═════════════════════════════╗│
│ ║ ▶ Squad (2 members)                           ║│
│ ║   [Joined]                                    ║│
│ ║   [Leave Room] [Invite Friend] [Start Voice]  ║│
│ ╚════════════════════════════════════════════════╝│
│ Room Members:                                    │
│ ╔══ RoomMembersScroll ══════════════════════════╗│
│ ║ • Alice                                        ║│
│ ║ • You (you)                                    ║│
│ ╚════════════════════════════════════════════════╝│
```

---

## 12) 构建问题记录

### Docker 构建：`cannot call member function 'AddLogEntry' without object`

- 原因：`AddLogEntry()` 声明为非静态，但在 `static` handler 方法中直接调用
- 修复：将 `AddLogEntry()` 改为 `static`（它只访问 `static` 成员 `eventLog` / `eventLogMutex`）

### Docker 构建：`apt-get update` 连接 `archive.ubuntu.com` 超时

- 原因：国内网络无法访问 Ubuntu 官方源
- 修复：移除 `apt-get install libssl3` 步骤（`libssl3` 已内置在 `ubuntu:22.04` 基础镜像中）

---

## 13) 客户端连接/登录问题修复（2026-05-05 下午）

### 修复的 Bug

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 1 | `VoiceChatServer/Server.cpp` | **`HandleLoginRequest` 中 `sessionMutex` 自死锁** — `std::lock_guard<std::mutex> lock(sessionMutex)` 作用域过大，把 `onlineUsersMutex` 和 `NotifyFriendsStatusChange()` 都包在里面。`NotifyFriendsStatusChange` → `SendMessageToUser` → `FindConnectionByUserId` 再次尝试获取同一个非递归 `std::mutex`，导致自己把自己死锁。服务端 TUI 也因 `GetConnectedUsers()` 等待同一把锁而卡死。 | 用 `{}` 缩小 `lock` 作用域，确保 `sessionMutex` 在调用 `NotifyFriendsStatusChange` 前已释放 |
| 2 | `VoiceChatClient/UIAuthPanel.h` + `SocketClient.h` | 登录/注册表单空输入客户端校验错误被 `auth_status_text = client->GetAuthMessage()` 每帧覆盖，错误提示一闪而过 | `SetAuthMessage` / `SetStatusMessage` 从 private 移到 public，UI 层直接写入客户端状态 |
| 3 | `VoiceChatClient/SocketClient.cpp` | `Disconnect()` 未清理 `notificationQueue` 和 `speakingUsers`，重连后残留旧数据 | 在 `Disconnect()` 中加锁清理这两个容器 |
| 4 | `VoiceChatServer/Server.cpp` | `PollIncomingMessages` 收到空数据时 `break` 退出整个循环，跳过同批次后续消息 | 改为 `pIncomingMsg->Release(); continue;` |
| 5 | `VoiceChatClient/SocketClient.cpp` | `HSteamNetConnection connection` 依赖零初始化等于 `k_HSteamNetConnection_Invalid`，不够明确 | 显式初始化为 `= k_HSteamNetConnection_Invalid` |

### 死锁根因详解

**原始代码**（`Server.cpp:337-349`）:
```cpp
std::lock_guard<std::mutex> lock(sessionMutex);  // 获取锁
SessionInfo& session = connectionSessions[connection];
session.authenticated = true;
...
{
    std::lock_guard<std::mutex> ol(onlineUsersMutex);  // 内层锁
    onlineUsers.insert(response.userId);
}  // 释放 onlineUsersMutex，但 sessionMutex 仍被 lock 持有！
NotifyFriendsStatusChange(response.userId, true);  // → FindConnectionByUserId → 尝试获取 sessionMutex → 死锁！
```

**修复后**:
```cpp
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    SessionInfo& session = connectionSessions[connection];
    ...
}  // sessionMutex 在此释放

{
    std::lock_guard<std::mutex> ol(onlineUsersMutex);
    onlineUsers.insert(response.userId);
}

NotifyFriendsStatusChange(response.userId, true);  // sessionMutex 已释放，可正常获取
```

### 调试过程

- 客户端日志确认连接成功、`SendLoginRequest` 发出 97 字节
- 服务端通过文件日志（`/app/data/debug.log`）精确定位卡死在 `FCBUI:lock`（`FindConnectionByUserId` 尝试获取 `sessionMutex`）
- 日志序列：`5:before_notify` → `NFSC:1/2/3_loop` → `SMTU:1` → `FCBUI:lock`（死锁点），`FCBUI:got_lock` 从未出现
- 日志还显示了多轮 `NFSC:3_loop` 说明数据库中有之前测试留下的好友记录，好友列表非空时才会触发该路径
