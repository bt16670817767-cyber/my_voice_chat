# SimpleVoiceChat 今日配置记录（2026-04-27）

本文档记录今天完成的三项工作：

1. 注册系统打通：协议扩展、服务端落盘、客户端 ImGui 注册表单
2. 好友系统一期：搜索/添加/删除好友、在线状态实时感知、客户端好友界面
3. 多人语音房间：创建房间、邀请好友、加入房间、房间内多人语音聊天

---

## 1) 注册系统

### 本次完成内容

- **协议扩展**：新增注册请求/响应类型
  - `REGISTER_REQ`(6)：客户端发送用户名/密码/显示名
  - `REGISTER_RES`(7)：服务端返回成功/失败、错误码、userId
  - 新增错误码 `ERR_USERNAME_TAKEN`(4)：用户名已存在

- **服务端**：
  - `UserRepository` 新增 `UsernameExists()` / `CreateUser()` 方法
  - `HandleRegisterRequest()`：校验非空 → 查重 → Argon2id 哈希 → 写入 SQLite
  - 注册成功后不自动登录，需用户手动登录
  - 已登录状态下拒绝注册（返回 `ERR_ALREADY_AUTHENTICATED`）

- **客户端 ImGui 注册 UI**：
  - 登录区域下方添加注册表单（新用户名 / 新密码 / 显示名）
  - 空输入时客户端侧校验，避免无效请求

### 默认验收方式

- 启动服务端和客户端，在未登录状态下填写注册表单 → 点击 Register
- 提示 "registration successful; please login" 后用新账号登录
- 重复注册同名用户 → 提示 "username already taken"

---

## 2) 好友系统

### 本次完成内容

- **协议扩展**：新增 8 组（14 个）好友消息类型

| 消息 | 方向 | 作用 |
|------|------|------|
| `FRIEND_SEARCH_REQ(8)` / `RES(9)` | C→S / S→C | 按用户名模糊搜索，返回批结果（最多5条） |
| `FRIEND_ADD_REQ(10)` / `RES(11)` | C→S / S→C | 发送好友申请 |
| `FRIEND_ACCEPT_REQ(12)` / `RES(13)` | C→S / S→C | 接受好友申请 |
| `FRIEND_REJECT_REQ(14)` / `RES(15)` | C→S / S→C | 拒绝好友申请 |
| `FRIEND_LIST_REQ(16)` / `RES(17)` | C→S / S→C | 获取好友列表（含在线状态，最多20人） |
| `FRIEND_REMOVE_REQ(18)` / `RES(19)` | C→S / S→C | 删除好友 |
| `FRIEND_REQUEST_NOTIFY(20)` | S→C | 推送：有人申请加你为好友 |
| `FRIEND_ONLINE_NOTIFY(21)` | S→C | 推送：好友上线/下线 |

- **新增错误码**：
  - `ERR_USER_NOT_FOUND`(5)：用户不存在
  - `ERR_ALREADY_FRIENDS`(6)：已是好友
  - `ERR_ALREADY_REQUESTED`(7)：已发送过申请
  - `ERR_CANNOT_FRIEND_SELF`(8)：不能添加自己

- **服务端 DB 层**：
  - 新文件 `Auth/FriendRepository.h/.cpp`：封装 `friendships` 表 CRUD
  - 建表：`friendships(id, user_id, friend_id, status, created_at)`
  - 双向查询好友列表，搜索时返回关系状态（无关系/已是好友/已发出申请/收到申请）

- **服务端逻辑**：
  - 好友 Handler：搜索、添加、接受、拒绝、列表、删除
  - 双向申请时自动转为接受（免去重复操作）
  - 上线/下线时通过 `NotifyFriendsStatusChange()` 向所有在线好友推送通知
  - 连接断开时通过 `HandleUserDisconnect()` 标记下线并通知好友
  - 辅助方法：`FindConnectionByUserId()` 按 userId 定位连接、`SendMessageToUser()` 定向推送

- **客户端 ImGui 好友 UI**（新增 Friends 标签页）：
  - 搜索栏：输入用户名 → 显示结果（含在线状态和关系状态）
  - 搜索结果：已是好友/已发送申请/收到申请（可接受/拒绝）→ 一键操作
  - 好友列表：显示所有好友 + 在线/离线状态（绿色/灰色文字）
  - 删除好友按钮
  - 待处理申请列表：接受/拒绝按钮
  - 刷新按钮：手动同步好友列表

---

## 3) 多人语音房间

### 本次完成内容

- **协议扩展**：新增 6 组（12 个）房间消息类型

| 消息 | 方向 | 作用 |
|------|------|------|
| `ROOM_CREATE_REQ(22)` / `RES(23)` | C→S / S→C | 创建房间（返回频道号） |
| `ROOM_INVITE_REQ(24)` / `RES(25)` | C→S / S→C | 邀请好友到房间 |
| `ROOM_INVITE_NOTIFY(26)` | S→C | 推送：被邀请到房间 |
| `ROOM_JOIN_REQ(27)` / `RES(28)` | C→S / S→C | 加入房间（返回频道号） |
| `ROOM_LEAVE_REQ(29)` / `RES(30)` | C→S / S→C | 离开房间 |
| `ROOM_LIST_REQ(31)` / `RES(32)` | C→S / S→C | 获取自己的房间列表 |
| `ROOM_MEMBER_UPDATE(33)` | S→C | 推送：成员变化（加入/离开） |

- **新增错误码**：
  - `ERR_ROOM_NOT_FOUND`(9)：房间不存在
  - `ERR_NOT_ROOM_MEMBER`(10)：不是该房间成员
  - `ERR_NOT_INVITED`(11)：没有被邀请

- **服务端 RoomManager**（新文件 `RoomManager.h/.cpp`）：
  - 纯内存房间管理，无 DB 持久化
  - 房间结构：`{roomId, roomName, channel, ownerUserId, members, invited}`
  - 频道号自动分配（>=1000000），与手动频道（0~999999）不冲突
  - 最后一人离开时自动销毁房间
  - 线程安全：所有操作由内部 mutex 保护

- **服务端逻辑**：
  - 创建房间 → 创建者自动加入 → 自动切换到房间音频频道
  - 邀请好友 → 目标在线则推送 `ROOM_INVITE_NOTIFY`
  - 加入房间 → 自动切换到房间频道 → 通知所有成员 `ROOM_MEMBER_UPDATE`
  - 离开房间 → 清除频道 → 通知剩余成员 → 空房则销毁
  - 断连清理：`HandleUserDisconnect()` 中遍历所有房间移除用户 + 通知成员

- **音频集成（关键链路）**：
  1. 用户创建/加入房间 → 服务端返回房间对应的 `channel`
  2. 客户端收到 `ROOM_JOIN_RES` 后自动发送 `SetChannel{channel}`
  3. 房间内所有成员处于同一频道 → 复用现有 `AUDIO` 转发逻辑实现多人语音

- **客户端 ImGui 房间 UI**（新增 Rooms 标签页）：
  - 创建房间：输入房间名 → Create
  - 房间列表：显示已加入/被邀请的房间、成员数
  - 已加入的房间：Leave 按钮、Invite 弹窗（显示在线好友）、Start Voice 按钮
  - 被邀请的房间：Join 按钮
  - 成员列表：显示当前选中房间的所有成员
  - 停止语音时自动恢复到手动频道

---

## 4) UI 重组

### 本次完成内容

- 将客户端单一窗口改为标签页结构：
  - **Voice Chat** 标签：手动频道输入 + Start/Stop Audio
  - **Friends** 标签：搜索、好友列表、待处理申请
  - **Rooms** 标签：房间创建、列表、邀请、语音
- 窗口尺寸扩大为 960×680（适应新增内容）
- 登录后自动拉取好友列表
- 状态栏合并为一行：`Connection | Auth | Audio`

---

## 5) 文件变更清单

### 新建文件（30 个）

**Common/Messages/（26 个消息头文件）**

| 类别 | 文件 |
|------|------|
| 注册 | `RegisterRequestMessage.h`, `RegisterResponseMessage.h` |
| 好友-搜索 | `FriendSearchRequestMessage.h`, `FriendSearchResultMessage.h` |
| 好友-添加 | `FriendAddRequestMessage.h`, `FriendAddResponseMessage.h` |
| 好友-接受 | `FriendAcceptRequestMessage.h`, `FriendAcceptResponseMessage.h` |
| 好友-拒绝 | `FriendRejectRequestMessage.h`, `FriendRejectResponseMessage.h` |
| 好友-列表 | `FriendListRequestMessage.h`, `FriendListResponseMessage.h` |
| 好友-删除 | `FriendRemoveRequestMessage.h`, `FriendRemoveResponseMessage.h` |
| 好友-推送 | `FriendRequestNotifyMessage.h`, `FriendOnlineNotifyMessage.h` |
| 房间-创建 | `RoomCreateRequestMessage.h`, `RoomCreateResponseMessage.h` |
| 房间-邀请 | `RoomInviteRequestMessage.h`, `RoomInviteResponseMessage.h`, `RoomInviteNotifyMessage.h` |
| 房间-加入 | `RoomJoinRequestMessage.h`, `RoomJoinResponseMessage.h` |
| 房间-离开 | `RoomLeaveRequestMessage.h`, `RoomLeaveResponseMessage.h` |
| 房间-列表 | `RoomListRequestMessage.h`, `RoomListResponseMessage.h` |
| 房间-成员 | `RoomMemberUpdateMessage.h` |

**服务端（4 个）**
- `VoiceChatServer/Auth/FriendRepository.h` / `.cpp` — 好友 DB CRUD
- `VoiceChatServer/RoomManager.h` / `.cpp` — 内存房间管理

### 修改文件（11 个）

| 文件 | 主要变更 |
|------|---------|
| `Common/Messages/MessageTypes.h` | +30 消息类型枚举值，+8 错误码 |
| `VoiceChatServer/Server.h` | +14 个 handler 声明、RoomManager、onlineUsers、friendRepo、新 include |
| `VoiceChatServer/Server.cpp` | +500 行：注册/好友/房间 handler、在线追踪、断连清理 |
| `VoiceChatServer/Auth/UserRepository.h/.cpp` | `UsernameExists()`、`CreateUser()` |
| `VoiceChatServer/main.cpp` | （可后续扩展 TUI 在线人数/房间数统计） |
| `VoiceChatServer/CMakeLists.txt` | 新增 FriendRepository、RoomManager、所有消息头文件 |
| `VoiceChatClient/main.cpp` | 完全重写：标签页 UI（Voice Chat / Friends / Rooms） |
| `VoiceChatClient/SocketClient.h` | 新增好友/房间状态结构、发送方法、访问器 |
| `VoiceChatClient/SocketClient.cpp` | +400 行：20+ 种消息处理、发送方法、状态缓存 |
| `VoiceChatClient/CMakeLists.txt` | 新增所有消息头文件 |

---

## 6) 构建/运行

### 构建服务端镜像

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main
docker build -t voicechat-server-ftxui:latest .
```

### 运行服务端容器

```powershell
docker run --rm -it -p 27020:27020/udp --name voicechat_server_ui voicechat-server-ftxui:latest
```

### （可选）DB 持久化

```powershell
mkdir e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatServer\data -Force

docker run --rm -it `
  -p 27020:27020/udp `
  -v e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatServer\data:/app/data `
  --name voicechat_server_ui `
  voicechat-server-ftxui:latest
```

### 客户端构建

```powershell
cd e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="e:/my_voice_chat/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

### 客户端启动

```powershell
e:\my_voice_chat\SimpleVoiceChat-main\VoiceChatClient\build\Release\VoiceChatClient.exe 127.0.0.1 27020 0
```

---

## 7) 推荐验收顺序

### 注册 + 登录
1. 启动服务端容器
2. 启动客户端 A，点击 Connect
3. 在注册表单填写新用户名/密码/显示名 → Register
4. 提示成功后，在登录表单用新账号登录
5. 验证登录成功后进入标签页 UI

### 好友功能
1. 启动客户端 B（用另一个账号如 `demo`/`123456` 登录）
2. 客户端 A 在 Friends 标签搜索 `demo`
3. 点击 Add 发送好友申请
4. 客户端 B 在 Friends 标签看到待处理申请 → Accept
5. 双方好友列表互相可见在线状态

### 房间 + 多人语音
1. 客户端 A 切换到 Rooms 标签 → 输入房间名 → Create
2. 客户端 A 点击 Invite → 从好友列表邀请客户端 B
3. 客户端 B 收到邀请 → Rooms 标签出现被邀请房间 → Join
4. 客户端 A 点击 Start Voice → 开始语音
5. 客户端 B 也点击 Start Voice → 双方语音互通
6. （可选）启动客户端 C 加入同一房间 → 三人语音

---

## 8) 默认验收账号

- 用户名：`demo`，密码：`123456`
- 用户名：`tester`，密码：`123456`

> 新注册的账号密码经过 Argon2id 哈希后存入 `users.sqlite`，不会存储明文。

---

## 9) 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 好友模型 | 申请/接受制 | 更接近真实社交应用体验 |
| 房间持久化 | 纯内存（空即销毁） | 语音房间生命周期短，无需 DB |
| 房间音频 | 频道号 >= 1000000 | 完全复用现有频道转发逻辑 |
| 在线感知 | 服务端主动推送 | 好友上下线秒级可见 |
| 用户定位 | 线性遍历 connectionSessions | 用户量小（<100），O(n) 足够 |
| UI 布局 | ImGui 标签页 | 单窗口三标签，比多窗口整洁 |
| 双向申请 | 自动转为接受 | 免去"收到申请还要再发申请"的冗余操作 |

---

## 10) 已知限制

- 好友列表上限 20 人（单条消息容量，后续可扩展分页）
- 搜索结果最多 5 条
- 房间成员最多 10 人显示（消息结构限制）
- 同一时间用户只能处于一个音频频道（一个房间）
- 服务端重启后所有房间消失（内存管理）

---

## 11) 客户端 UI 与功能对应

客户端登录后显示**三个标签页**（Voice Chat / Friends / Rooms），底部统一状态栏。

### 登录前界面（未认证状态）

窗口上部显示连接配置（Server IP + Port + Connect 按钮），连接成功后显示登录/注册区域：

```
┌─ Voice Chat Controls ────────────────────────────────────┐
│ Server IP: [127.0.0.1]   Port: [27020]                   │
│ Connected                                                │
│ Please login or register to continue.                    │
│──────────────────────────────────────────────────────────│
│ --- Login ---                                            │
│ Username: [demo          ]   Password: [***       ]      │
│ [Login]                                                  │
│──────────────────────────────────────────────────────────│
│ --- Register ---                                         │
│ New Username:  [                ]                        │
│ New Password:  [                ]  (掩码输入)              │
│ Display Name:  [                ]                        │
│ [Register]                                               │
│──────────────────────────────────────────────────────────│
│ Connection: Connected | Auth: Unauthenticated | ...       │
└──────────────────────────────────────────────────────────┘
```

### 标签页一：Voice Chat（语音聊天）

手动频道模式，输入频道号后点击 Start Audio 即可开始语音。

| UI 控件 | 对应功能 | 说明 |
|---------|---------|------|
| `Channel` 输入框 | 手动频道号 (0~999999) | 输入后点 Start Audio 加入该频道 |
| `Start Audio` 按钮 | 设置频道 + 开始录音 | 发送 SetChannel → 启动麦克风采集 |
| `Stop Audio` 按钮 | 停止录音 | 停止麦克风，切换到非语音状态 |

> 房间内的语音入口在 Rooms 标签页，不在本页。

### 标签页二：Friends（好友）

**功能区域一：搜索**

| UI 控件 | 对应功能 | 对应协议 |
|---------|---------|---------|
| `Search` 输入框 | 输入用户名关键词 | — |
| `Search` 按钮 | 触发好友搜索 | `FRIEND_SEARCH_REQ(8)` / `RES(9)` |
| `Refresh List` 按钮 | 手动刷新好友列表 | `FRIEND_LIST_REQ(16)` / `RES(17)` |

**功能区域二：搜索结果**

每条结果显示：`DisplayName (username)` + 在线状态 + 操作按钮

| 关系状态 | UI 显示 | 操作按钮 |
|---------|---------|---------|
| 无关系 | — | `Add` → 发送 `FRIEND_ADD_REQ(10)` |
| 已是好友 | `[Already friends]` | 无 |
| 已发出申请 | `[Request sent]` | 无 |
| 收到申请 | `Accept` + `Reject` | `FRIEND_ACCEPT_REQ(12)` / `FRIEND_REJECT_REQ(14)` |

在线状态颜色：
- 绿色 `(Online)` = 在线
- 灰色 `(Offline)` = 离线

**功能区域三：我的好友列表**

每条显示：好友 DisplayName + 在线状态 + `Remove` 按钮

| UI 控件 | 对应功能 | 对应协议 |
|---------|---------|---------|
| `Remove` 按钮 | 删除该好友 | `FRIEND_REMOVE_REQ(18)` |

**功能区域四：待处理申请**

显示收到的好友申请，每条有 `Accept` / `Reject` 按钮。

**后台推送（不在 UI 显示，触发通知）：**

| 协议 | 效果 |
|------|------|
| `FRIEND_REQUEST_NOTIFY(20)` | 有人申请加你好友，底部状态栏提示 |
| `FRIEND_ONLINE_NOTIFY(21)` | 好友上线/下线，好友列表在线状态实时更新 |

### 标签页三：Rooms（房间）

> 房间语音与手动频道语音**互斥**，同一时间只能处于一个语音频道。

**功能区域一：创建房间**

| UI 控件 | 对应功能 | 对应协议 |
|---------|---------|---------|
| `Room Name` 输入框 | 房间名称 | — |
| `Create` 按钮 | 创建房间（创建者自动加入，频道号 >=1000000） | `ROOM_CREATE_REQ(22)` / `RES(23)` |

**功能区域二：我的房间列表**

刷新方式：切换到 Rooms 标签时自动加载，也可点 `Refresh Rooms` 按钮。

每条显示：`房间名 (N members)` + 状态标签 + 操作按钮

| 状态 | UI 标签 | 操作按钮 |
|------|--------|---------|
| 已加入 | `[Joined]`（绿色） | `Leave` / `Invite` / `Start Voice` |
| 被邀请 | `[Invited]`（黄色） | `Join` |
| 正在发言 | `[Speaking]`（黄色） | 无（已在通话中） |

**已加入房间的操作按钮：**

| 按钮 | 功能 | 对应协议 |
|------|------|---------|
| `Leave` | 离开房间（停止语音→发送离开→刷新列表） | `ROOM_LEAVE_REQ(29)` |
| `Invite` | 弹出好友选择窗口，选在线好友发送邀请 | `ROOM_INVITE_REQ(24)` |
| `Start Voice` | 切换到房间频道号 + 开始录音 | SetChannel + 麦克风启动 |

**被邀请房间的操作按钮：**

| 按钮 | 功能 | 对应协议 |
|------|------|---------|
| `Join` | 加入房间（自动切换到房间频道） | `ROOM_JOIN_REQ(27)` / `RES(28)` |

**功能区域三：房间成员列表**

选中房间后显示该房间所有成员，自己名字后面标注 `(you)`。

**后台推送：**

| 协议 | UI 效果 |
|------|---------|
| `ROOM_INVITE_NOTIFY(26)` | 被邀请后 Rooms 标签出现 `[Invited]` 条目 |
| `ROOM_MEMBER_UPDATE(33)` | 成员列表实时刷新（有人加入/离开时） |

### 状态栏（所有界面底部）

```
Connection: Disconnected/Connected/Connecting | Auth: Unauthenticated/Authenticated | Audio: Stopped/Running
```

第二行显示最近一条登录/操作提示信息（如 `registration successful; please login`）。

---

## 12) 常见问题

### Docker 构建失败：`int64 does not name a type`

- 原因：Linux GCC 只识别 `int64_t`（来自 `<cstdint>`），MSVC 通过 Steam 头文件额外提供了 `int64` typedef
- 已修复：`RoomManager.h` 中 `int64 channel` → `int64_t channel`

### 客户端编译 VS 服务端编译类型不一致

- 客户端在 Windows 用 MSVC 编译，服务端在 Docker Linux 用 GCC 编译
- 两者对 `int64` 等非标准类型的支持不同，建议统一使用 `int64_t`、`uint8_t` 等标准类型
