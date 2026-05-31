Param(
    [string]$ServerIp = "127.0.0.1",
    [int]$Port = 27020
)

$ErrorActionPreference = "Stop"

# 获取项目根目录并构建客户端可执行文件路径
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$clientExe = Join-Path $projectRoot "VoiceChatClient\build\Release\VoiceChatClient.exe"

# 检查客户端可执行文件是否存在
if (-not (Test-Path $clientExe)) {
    throw "Client executable not found at: $clientExe. Please build the client project first."
}

# 启动客户端进程
Write-Host "Launching a client to connect to $ServerIp`:$Port..."
Start-Process -FilePath $clientExe -ArgumentList @($ServerIp, "$Port")

Write-Host "✅ Client process started."
