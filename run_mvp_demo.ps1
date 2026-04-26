Param(
    [string]$ServerImage = "voicechat-server-ftxui:latest",
    [string]$ServerIp = "127.0.0.1",
    [int]$Port = 27020,
    [int]$Channel = 0,
    [switch]$DetachedServer
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$clientExe = Join-Path $projectRoot "VoiceChatClient\build\Release\VoiceChatClient.exe"
$serverContainerName = "voicechat_server_ui"

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "docker not found. Please install Docker Desktop first."
}

if (-not (Test-Path $clientExe)) {
    throw "Client executable not found: $clientExe. Build client first."
}

Write-Host "Stopping old server container if exists..."
try {
    docker rm -f $serverContainerName *>&1 | Out-Null
} catch {
    
}

Write-Host "Starting server container..."
if ($DetachedServer) {
    docker run --rm -d -p "$Port`:$Port/udp" --name $serverContainerName $ServerImage "./VoiceChatServer" "$Port" | Out-Null
    Write-Host "Server running in detached mode."
} else {
    $dockerCmd = "docker run --rm -it -p $Port`:$Port/udp --name $serverContainerName $ServerImage ./VoiceChatServer $Port"
    Start-Process -FilePath "powershell.exe" -ArgumentList @("-NoExit", "-Command", $dockerCmd)
    Write-Host "Server running in interactive window (press q/Esc there to stop)."
}

Write-Host "Launching client A..."
Start-Process -FilePath $clientExe -ArgumentList @($ServerIp, "$Port", "$Channel")

Write-Host "Launching client B..."
Start-Process -FilePath $clientExe -ArgumentList @($ServerIp, "$Port", "$Channel")

Write-Host ""
Write-Host "MVP demo processes are started."
Write-Host "Server: docker logs -f $serverContainerName"
Write-Host "Stop server: press q/Esc in server window (interactive mode)"
Write-Host "Fallback stop: docker stop $serverContainerName"
