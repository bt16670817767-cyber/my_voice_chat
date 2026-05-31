Param(
    [string]$ServerImage = "voicechat-server-ftxui:latest",
    [int]$Port = 27020,
    [switch]$Detached
)

$ErrorActionPreference = "Stop"

$serverContainerName = "voicechat_server_ui"
$dataVolume = "voicechat_server_data"

# Check Docker
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "Docker not found. Please install Docker Desktop and ensure it's running."
}

# Create persistent volume if not exists
docker volume create $dataVolume *>&1 | Out-Null

# Stop old container
Write-Host "Stopping old server container if it exists..."
try {
    docker rm -f $serverContainerName *>&1 | Out-Null
} catch {
    # ignore
}

# Start new container
Write-Host "Starting server container '$serverContainerName'..."
if ($Detached) {
    docker run --rm -dit -p "${Port}:${Port}/udp" -v "${dataVolume}:/app/data" --name $serverContainerName $ServerImage "./VoiceChatServer" "$Port" | Out-Null
    Write-Host "Server is running in detached mode."
    Write-Host "   - To view logs: docker logs -f $serverContainerName"
    Write-Host "   - To stop server: docker stop $serverContainerName"
} else {
    $dockerCmd = "docker run --rm -it -p ${Port}:${Port}/udp -v ${dataVolume}:/app/data --name $serverContainerName $ServerImage ./VoiceChatServer $Port"
    Start-Process -FilePath "powershell.exe" -ArgumentList @("-NoExit", "-Command", $dockerCmd)
    Write-Host "Server is running in a new interactive window."
    Write-Host "   - To stop server: Press q or Esc in the server window."
}
