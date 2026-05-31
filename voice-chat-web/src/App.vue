<script setup lang="ts">
import { ref, watch } from 'vue'

// ==========================================
// 1. 全局状态
// ==========================================
const currentScreen = ref('connect') 
const activeTab = ref('friends')

const serverIp = ref('127.0.0.1')
const serverPort = ref('27020')
const isLoginMode = ref(true)
const username = ref('demo')
const password = ref('123456')
const displayName = ref('')

const currentUser = ref({ id: 0, username: '', displayName: '' })
let ws: WebSocket | null = null

// ==========================================
// 2. 好友与房间状态
// ==========================================
const friendsSubTab = ref('list')
const myFriends = ref<any[]>([])
const pendingRequests = ref<any[]>([])
const searchQuery = ref('')
const searchResults = ref<any[]>([])

const roomsSubTab = ref('lobby')
const allRooms = ref<any[]>([])
const newRoomName = ref('')
const newRoomPwd = ref('')

const activeRoom = ref<{id: number, name: string} | null>(null)
const roomMembers = ref<any[]>([])
const isMicOn = ref(false)

// ==========================================
// 3. WebRTC P2P 核心逻辑
// ==========================================
const peerConnections = new Map<number, RTCPeerConnection>()
let localStream: MediaStream | null = null
const rtcConfig = { iceServers: [{ urls: 'stun:stun.l.google.com:19302' }] }

const startMicrophone = async () => {
  try {
    localStream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false })
    localStream.getAudioTracks()[0].enabled = false // 默认进房闭麦
    isMicOn.value = false
  } catch (err) {
    console.error("获取麦克风失败:", err)
    alert("无法访问麦克风，请允许网页使用麦克风！")
  }
}

const stopAllWebRTC = () => {
  peerConnections.forEach(pc => pc.close())
  peerConnections.clear()
  if (localStream) {
    localStream.getTracks().forEach(track => track.stop())
    localStream = null
  }
}

const getOrCreatePeerConnection = (targetId: number) => {
  if (peerConnections.has(targetId)) return peerConnections.get(targetId)!

  const pc = new RTCPeerConnection(rtcConfig)
  peerConnections.set(targetId, pc)

  if (localStream) {
    localStream.getTracks().forEach(track => pc.addTrack(track, localStream!))
  }

  pc.ontrack = (event) => {
    const audioEl = document.getElementById('audio-' + targetId) as HTMLAudioElement
    if (audioEl) audioEl.srcObject = event.streams[0]
  }

  pc.onicecandidate = (event) => {
    if (event.candidate) {
      ws?.send(JSON.stringify({ type: 'webrtc_ice', targetId, candidate: event.candidate }))
    }
  }
  return pc
}

// ==========================================
// 4. WebSocket 核心通信逻辑
// ==========================================
const handleLogout = () => {
  if (confirm('确定要退出登录吗？')) {
    ws?.send(JSON.stringify({ type: 'logout' }))
    stopAllWebRTC()
    currentScreen.value = 'auth'
    currentUser.value = { id: 0, username: '', displayName: '' }
    activeRoom.value = null; myFriends.value = []; allRooms.value = []; roomMembers.value = []
  }
}

const handleConnect = () => {
  ws = new WebSocket(`ws://${serverIp.value}:${serverPort.value}`)
  ws.onopen = () => { currentScreen.value = 'auth' }
  ws.onclose = () => {
    alert('与服务器断开连接')
    stopAllWebRTC()
    currentScreen.value = 'connect'
  }

  ws.onmessage = (event) => {
    const res = JSON.parse(event.data)
    
    if (res.type === 'login_result' || res.type === 'register_result') {
      if (res.success) {
        currentUser.value = { id: res.userId, username: username.value, displayName: res.displayName }
        currentScreen.value = 'main'
        fetchFriendList()
        fetchRoomList()
      } else { alert(res.message) }
    }
    else if (res.type === 'friend_list_result') {
      myFriends.value = res.friends || []; pendingRequests.value = res.pending || []
    }
    else if (res.type === 'friend_search_result') { searchResults.value = res.results || [] }
    else if (res.type === 'friend_add_result') { if (res.success) alert('好友请求发送成功！') }
    else if (['friend_request_notify', 'friend_accept_notify', 'friend_remove_notify', 'friend_online_notify'].includes(res.type)) {
      fetchFriendList() 
    }
    else if (res.type === 'room_list_result') { allRooms.value = res.rooms || [] }
    else if (res.type === 'room_create_result' || res.type === 'room_join_result') {
      if (res.success) {
        activeRoom.value = { id: res.roomId, name: res.roomName }
        roomsSubTab.value = 'lobby'
        startMicrophone()
      } else { alert(res.message) }
    }
    else if (res.type === 'room_member_update') {
      if (activeRoom.value && activeRoom.value.id === res.roomId) {
        const currentMembers = res.members || []
        roomMembers.value = currentMembers.map((m: any) => ({ ...m, isSpeaking: false }))

        // 修复 TS 严格模式报错：明确告诉 TS 这是一个由 number 组成的 Set
        const currentMemberIds = new Set<number>(currentMembers.map((m: any) => Number(m.id)))
        
        for (const [targetId, pc] of peerConnections.entries()) {
          if (!currentMemberIds.has(targetId)) {
            pc.close()
            peerConnections.delete(targetId)
          }
        }
        
        for (const targetId of currentMemberIds) {
          if (targetId !== currentUser.value.id) {
            const pc = getOrCreatePeerConnection(targetId)
            if (currentUser.value.id > targetId && pc.signalingState === 'stable') {
               pc.createOffer().then(offer => {
                 pc.setLocalDescription(offer)
                 ws?.send(JSON.stringify({ type: 'webrtc_offer', targetId, sdp: offer }))
               })
            }
          }
        }
      }
    }
    else if (res.type === 'room_leave_result') {
      if (res.success) {
        activeRoom.value = null; roomMembers.value = []
        fetchRoomList()
        stopAllWebRTC()
      }
    }
    
    // 接收 WebRTC 信令
    else if (res.type === 'webrtc_offer') {
      const pc = getOrCreatePeerConnection(Number(res.fromId))
      pc.setRemoteDescription(new RTCSessionDescription(res.sdp))
        .then(() => pc.createAnswer())
        .then(answer => {
          pc.setLocalDescription(answer)
          ws?.send(JSON.stringify({ type: 'webrtc_answer', targetId: res.fromId, sdp: answer }))
        })
    }
    else if (res.type === 'webrtc_answer') {
      const pc = getOrCreatePeerConnection(Number(res.fromId))
      pc.setRemoteDescription(new RTCSessionDescription(res.sdp))
    }
    else if (res.type === 'webrtc_ice') {
      const pc = getOrCreatePeerConnection(Number(res.fromId))
      pc.addIceCandidate(new RTCIceCandidate(res.candidate))
    }
  }
}

const handleAuth = () => {
  ws?.send(JSON.stringify({
    type: isLoginMode.value ? 'login' : 'register',
    username: username.value, password: password.value, displayName: displayName.value
  }))
}

// ==========================================
// 5. 业务触发函数 (封装 ws.send)
// ==========================================
const fetchFriendList = () => ws?.send(JSON.stringify({ type: 'friend_list' }))
const searchUser = () => ws?.send(JSON.stringify({ type: 'friend_search', query: searchQuery.value }))
const sendFriendRequest = (targetId: number) => ws?.send(JSON.stringify({ type: 'friend_add', targetId }))
const acceptFriend = (targetId: number) => ws?.send(JSON.stringify({ type: 'friend_accept', targetId }))
const rejectFriend = (targetId: number) => ws?.send(JSON.stringify({ type: 'friend_reject', targetId }))
const removeFriend = (targetId: number) => {
  if(confirm('确定删除此好友？')) ws?.send(JSON.stringify({ type: 'friend_remove', targetId }))
}
const fetchRoomList = () => ws?.send(JSON.stringify({ type: 'room_list' }))

watch(activeTab, (val) => { if (val === 'rooms' && !activeRoom.value) fetchRoomList() })
watch(roomsSubTab, (val) => { if (val === 'lobby' && !activeRoom.value) fetchRoomList() })

const createRoom = () => {
  if (!newRoomName.value) return alert('请输入房间名')
  ws?.send(JSON.stringify({ type: 'room_create', roomName: newRoomName.value, password: newRoomPwd.value }))
  newRoomName.value = ''; newRoomPwd.value = ''
}
const joinRoom = (roomId: number, hasPwd = false) => {
  let pwd = ''
  if (hasPwd) {
    const input = prompt('该房间需要密码：'); if (input === null) return; pwd = input
  }
  ws?.send(JSON.stringify({ type: 'room_join', roomId, password: pwd }))
}
const leaveRoom = () => {
  if (activeRoom.value) ws?.send(JSON.stringify({ type: 'room_leave', roomId: activeRoom.value.id }))
}

const toggleMic = () => {
  isMicOn.value = !isMicOn.value
  const me = roomMembers.value.find(m => m.id === currentUser.value.id)
  if (me) me.isSpeaking = isMicOn.value
  
  if (localStream && localStream.getAudioTracks().length > 0) {
    localStream.getAudioTracks()[0].enabled = isMicOn.value
  }
}
</script>

<template>
  <div class="app-container">
    <div v-if="currentScreen === 'connect'" class="modal-card fade-in">
      <h2>🌐 连接至服务器</h2>
      <div class="input-group"><label>IP</label><input v-model="serverIp" type="text" /></div>
      <div class="input-group"><label>Port</label><input v-model="serverPort" type="text" /></div>
      <button class="btn-primary" @click="handleConnect">连接</button>
    </div>

    <div v-else-if="currentScreen === 'auth'" class="modal-card fade-in">
      <h2>{{ isLoginMode ? '🔐 用户登录' : '📝 注册账号' }}</h2>
      <div class="input-group"><label>账号</label><input v-model="username" type="text" /></div>
      <div v-if="!isLoginMode" class="input-group"><label>昵称</label><input v-model="displayName" type="text" /></div>
      <div class="input-group"><label>密码</label><input v-model="password" type="password" /></div>
      <button class="btn-primary" @click="handleAuth">{{ isLoginMode ? '登录' : '注册' }}</button>
      <p class="toggle-mode" @click="isLoginMode = !isLoginMode">{{ isLoginMode ? '没有账号？点击注册 🚀' : '⬅️ 返回登录' }}</p>
    </div>

    <div v-else class="main-layout fade-in">
      <aside class="sidebar">
        <div class="user-profile">
          <div class="avatar">{{ currentUser.displayName.charAt(0).toUpperCase() }}</div>
          <div class="user-info">
            <div class="username">{{ currentUser.displayName }}</div>
            <div class="user-id">#{{ currentUser.id }}</div>
            <div class="status-online">● 在线</div>
          </div>
        </div>
        <nav class="nav-menu">
          <button :class="{ active: activeTab === 'friends' }" @click="activeTab = 'friends'">👥 好友系统</button>
          <button :class="{ active: activeTab === 'rooms' }" @click="activeTab = 'rooms'">🏠 语音房间</button>
        </nav>
        <button class="btn-logout" @click="handleLogout">退出登录</button>
      </aside>

      <main class="content-area">
        <div v-if="activeTab === 'friends'" class="panel">
          <div class="sub-nav">
            <span :class="{ active: friendsSubTab === 'list' }" @click="friendsSubTab = 'list'">我的好友</span>
            <span :class="{ active: friendsSubTab === 'add' }" @click="friendsSubTab = 'add'">添加好友</span>
            <span :class="{ active: friendsSubTab === 'requests' }" @click="friendsSubTab = 'requests'">
              好友请求 <b v-if="pendingRequests.length" class="badge">{{ pendingRequests.length }}</b>
            </span>
          </div>

          <div v-if="friendsSubTab === 'list'" class="sub-content">
            <ul class="list">
              <li v-for="friend in myFriends" :key="friend.id" class="list-item">
                <div class="item-left"><span class="name">{{ friend.displayName }} (@{{ friend.username }})</span></div>
                <div class="item-right">
                  <span :class="friend.isOnline ? 'tag-online' : 'tag-offline'">{{ friend.isOnline ? '在线' : '离线' }}</span>
                  <button class="btn-danger-text" @click="removeFriend(friend.id)">删除</button>
                </div>
              </li>
            </ul>
          </div>

          <div v-if="friendsSubTab === 'add'" class="sub-content">
            <div class="search-box">
              <input v-model="searchQuery" type="text" placeholder="搜账号或昵称" @keyup.enter="searchUser" />
              <button class="btn-primary" style="width: auto" @click="searchUser">搜索</button>
            </div>
            <ul class="list" style="margin-top: 20px">
              <li v-for="user in searchResults" :key="user.id" class="list-item">
                <div class="item-left"><span class="name">{{ user.displayName }} (@{{ user.username }})</span></div>
                <button v-if="user.relationshipStatus === 0" class="btn-primary-small" @click="sendFriendRequest(user.id)">加好友</button>
                <span v-else class="sub-text">状态: {{ ['已是好友','我发出的','待我同意'][user.relationshipStatus-1] }}</span>
              </li>
            </ul>
          </div>

          <div v-if="friendsSubTab === 'requests'" class="sub-content">
             <ul class="list">
              <li v-for="req in pendingRequests" :key="req.id" class="list-item">
                <div class="item-left"><span class="name">{{ req.displayName }}</span> 请求添加你</div>
                <div class="item-right">
                  <button class="btn-primary-small" @click="acceptFriend(req.id)">同意</button>
                  <button class="btn-danger-small" @click="rejectFriend(req.id)">拒绝</button>
                </div>
              </li>
            </ul>
          </div>
        </div>

        <div v-if="activeTab === 'rooms'" class="panel">
          <div v-if="!activeRoom">
            <div class="sub-nav">
              <span :class="{ active: roomsSubTab === 'lobby' }" @click="roomsSubTab = 'lobby'; fetchRoomList()">大厅列表</span>
              <span :class="{ active: roomsSubTab === 'create' }" @click="roomsSubTab = 'create'">创建房间</span>
              <button class="btn-primary-small" style="float: right;" @click="fetchRoomList">刷新列表</button>
            </div>

            <div v-if="roomsSubTab === 'lobby'" class="sub-content room-grid">
              <div v-for="room in allRooms" :key="room.id" class="room-card">
                <div class="room-header">
                  <h3>{{ room.name }}</h3>
                  <span v-if="room.hasPassword" class="lock-icon">🔒</span>
                </div>
                <p class="sub-text">ID: {{ room.id }} | 人数: {{ room.count }}</p>
                <button class="btn-primary" @click="joinRoom(room.id, room.hasPassword)">加入房间</button>
              </div>
            </div>

            <div v-if="roomsSubTab === 'create'" class="sub-content create-room-form">
              <div class="input-group"><label>房间名称</label><input v-model="newRoomName" type="text" /></div>
              <div class="input-group"><label>密码 (可选)</label><input v-model="newRoomPwd" type="password" /></div>
              <button class="btn-primary" @click="createRoom">创建并进入</button>
            </div>
          </div>
          
          <div v-else class="active-room-view">
            <div class="room-top-bar">
              <h2>🎙️ {{ activeRoom.name }}</h2>
              <button class="btn-leave" @click="leaveRoom">退出房间</button>
            </div>
            
            <div class="members-grid">
              <div v-for="member in roomMembers" :key="member.id" class="member-card" :class="{ speaking: member.isSpeaking }">
                <div class="avatar-large">{{ member.displayName.charAt(0) }}</div>
                <div class="name">{{ member.displayName }} {{ member.id === currentUser.id ? '(我)' : '' }}</div>
                <div class="mic-visualizer" :class="{ active: member.isSpeaking }">
                  <div class="wave"></div><div class="wave"></div><div class="wave"></div>
                </div>
                
                <audio v-if="member.id !== currentUser.id" :id="'audio-' + member.id" autoplay></audio>
              </div>
            </div>

            <div class="room-controls">
              <button class="btn-mic" :class="isMicOn ? 'mic-on' : 'mic-off'" @click="toggleMic">
                {{ isMicOn ? '🔴 停止推流 (Mute)' : '🟢 开启麦克风 (Speak)' }}
              </button>
            </div>
          </div>
        </div>
      </main>
    </div>
  </div>
</template>

<style scoped>
.app-container { 
  display: flex; justify-content: center; align-items: center; 
  min-height: 100vh; width: 100vw; background-color: #0f1115; color: #fff;
}
.fade-in { animation: fadeIn 0.3s ease; }
@keyframes fadeIn { 
  from { opacity: 0; transform: translateY(10px); } 
  to { opacity: 1; transform: translateY(0); } 
}
.modal-card { 
  background: #1e2128; padding: 40px; border-radius: 12px; width: 420px; 
  box-shadow: 0 8px 32px rgba(0,0,0,0.6); border: 1px solid #2d313a; 
}
.modal-card h2 { margin-top: 0; color: #61dafb; margin-bottom: 25px; }
.toggle-mode { text-align: center; margin-top: 20px; color: #61dafb; font-size: 14px; cursor: pointer; transition: 0.2s; }
.toggle-mode:hover { opacity: 0.7; }
.input-group { margin-bottom: 20px; }
.input-group label { display: block; margin-bottom: 8px; color: #a1a6b4; font-size: 14px; }
.input-group input { width: 100%; padding: 12px; border-radius: 6px; border: 1px solid #3a3f4b; background: #14151a; color: white; outline: none; font-size: 15px; }
.input-group input:focus { border-color: #61dafb; box-shadow: 0 0 0 2px rgba(97,218,251,0.2); }
.btn-primary { width: 100%; padding: 12px; border: none; border-radius: 6px; background: #61dafb; color: #121212; font-weight: bold; cursor: pointer; transition: 0.2s; font-size: 15px; }
.btn-primary:hover { background: #4fa8c7; }
.btn-primary-small { padding: 8px 16px; border: none; border-radius: 4px; background: #61dafb; color: #121212; cursor: pointer; font-weight: bold; }
.btn-danger-small { padding: 8px 16px; border: none; border-radius: 4px; background: #ff4757; color: white; cursor: pointer; font-weight: bold; }
.btn-danger-text { background: transparent; border: none; color: #ff4757; cursor: pointer; }
.main-layout { display: flex; width: 100%; height: 100vh; }
.sidebar { width: 280px; background: #16181d; border-right: 1px solid #2d313a; display: flex; flex-direction: column; padding: 25px; }
.content-area { flex-grow: 1; padding: 30px 40px; background: #0f1115; overflow-y: auto; }
.user-profile { display: flex; align-items: center; gap: 15px; margin-bottom: 40px; padding-bottom: 20px; border-bottom: 1px solid #2d313a; }
.avatar { width: 48px; height: 48px; border-radius: 50%; background: linear-gradient(135deg, #61dafb, #d361fb); color: #fff; display: flex; align-items: center; justify-content: center; font-weight: bold; font-size: 20px; }
.user-info { display: flex; flex-direction: column; gap: 4px; }
.username { font-weight: bold; font-size: 16px; }
.user-id { color: #8b92a5; font-size: 12px; }
.status-online { color: #4caf50; font-size: 12px; }
.btn-logout { background: transparent; border: 1px solid #ff4757; color: #ff4757; padding: 12px; border-radius: 6px; cursor: pointer; transition: 0.2s; margin-top: 20px; }
.btn-logout:hover { background: #ff4757; color: white; }
.nav-menu { display: flex; flex-direction: column; gap: 10px; flex-grow: 1; }
.nav-menu button { padding: 14px; background: transparent; border: none; color: #a1a6b4; text-align: left; border-radius: 8px; cursor: pointer; font-size: 16px; transition: 0.2s; }
.nav-menu button:hover { background: #2d313a; color: white; }
.nav-menu button.active { background: rgba(97,218,251,0.1); color: #61dafb; font-weight: bold; }
.sub-nav { display: flex; gap: 30px; border-bottom: 1px solid #2d313a; padding-bottom: 15px; margin-bottom: 20px; align-items: center; }
.sub-nav span { color: #8b92a5; cursor: pointer; padding-bottom: 14px; position: relative; font-size: 16px; }
.sub-nav span.active { color: #61dafb; font-weight: bold; }
.sub-nav span.active::after { content: ''; position: absolute; bottom: -1px; left: 0; width: 100%; height: 3px; background: #61dafb; border-radius: 3px 3px 0 0; }
.badge { background: #ff4757; color: white; padding: 2px 8px; border-radius: 12px; font-size: 12px; margin-left: 5px; }
.sub-content { animation: fadeIn 0.2s ease; }
.list { list-style: none; padding: 0; margin: 0; }
.list-item { display: flex; justify-content: space-between; align-items: center; padding: 16px 20px; background: #1e2128; margin-bottom: 12px; border-radius: 8px; border: 1px solid #2d313a; }
.item-left { display: flex; align-items: center; gap: 15px; }
.item-right { display: flex; align-items: center; gap: 15px; }
.name { font-weight: bold; font-size: 15px; }
.sub-text { color: #8b92a5; font-size: 13px; }
.tag-online { color: #4caf50; font-size: 12px; background: rgba(76,175,80,0.15); padding: 4px 8px; border-radius: 4px; }
.tag-offline { color: #8b92a5; font-size: 12px; background: rgba(139,146,165,0.15); padding: 4px 8px; border-radius: 4px; }
.search-box { display: flex; gap: 10px; }
.search-box input { flex-grow: 1; padding: 12px; border-radius: 6px; border: 1px solid #3a3f4b; background: #14151a; color: white; outline: none; }
.room-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 20px; }
.room-card { background: #1e2128; padding: 24px; border-radius: 12px; border: 1px solid #2d313a; transition: transform 0.2s; }
.room-card:hover { transform: translateY(-3px); border-color: #61dafb; }
.room-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
.room-header h3 { margin: 0; color: #fff; font-size: 18px; }
.create-room-form { max-width: 500px; padding: 30px; background: #1e2128; border-radius: 12px; border: 1px solid #2d313a; }
.active-room-view { display: flex; flex-direction: column; height: 100%; }
.room-top-bar { display: flex; justify-content: space-between; align-items: center; padding-bottom: 20px; border-bottom: 1px solid #2d313a; margin-bottom: 30px; }
.room-top-bar h2 { margin: 0; color: #61dafb; }
.btn-leave { padding: 10px 20px; border: 1px solid #ff4757; background: transparent; color: #ff4757; border-radius: 6px; cursor: pointer; transition: 0.2s; }
.btn-leave:hover { background: #ff4757; color: white; }
.members-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(160px, 1fr)); gap: 20px; flex-grow: 1; align-content: flex-start; }
.member-card { background: #1e2128; padding: 30px 20px; border-radius: 12px; border: 2px solid transparent; text-align: center; position: relative; transition: 0.3s; }
.member-card.speaking { border-color: #4caf50; box-shadow: 0 0 15px rgba(76,175,80,0.2); }
.avatar-large { width: 64px; height: 64px; border-radius: 50%; background: #3a3f4b; font-size: 28px; display: flex; align-items: center; justify-content: center; margin: 0 auto 15px; }
.mic-visualizer { display: flex; gap: 4px; justify-content: center; height: 20px; align-items: flex-end; opacity: 0; margin-top: 10px; }
.mic-visualizer.active { opacity: 1; }
.wave { width: 4px; height: 4px; background: #4caf50; border-radius: 2px; animation: bounce 0.4s infinite alternate; }
.wave:nth-child(2) { animation-delay: 0.15s; }
.wave:nth-child(3) { animation-delay: 0.3s; }
@keyframes bounce { 
  to { height: 16px; } 
}
.room-controls { padding: 30px 0; display: flex; justify-content: center; border-top: 1px solid #2d313a; margin-top: auto; }
.btn-mic { padding: 16px 40px; border: none; border-radius: 30px; font-weight: bold; font-size: 16px; cursor: pointer; color: white; transition: 0.2s; box-shadow: 0 4px 15px rgba(0,0,0,0.3); }
.mic-off { background: #4caf50; }
.mic-off:hover { background: #45a049; }
.mic-on { background: #ff4757; }
.mic-on:hover { background: #ff3344; }
</style>