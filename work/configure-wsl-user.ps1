$ErrorActionPreference = "Stop"

$DistroName = "Ubuntu-24.04-H"
$LinuxUser = ($env:USERNAME -replace '[^a-zA-Z0-9_-]', '').ToLowerInvariant()
$LogPath = Join-Path $PSScriptRoot "configure-wsl-user.log"

if ([string]::IsNullOrWhiteSpace($LinuxUser)) {
    $LinuxUser = "vita"
}

Start-Transcript -Path $LogPath -Force | Out-Null
try {
    Write-Host "== Configuring $DistroName default user: $LinuxUser =="

    wsl -d $DistroName -u root -- bash -lc "id -u '$LinuxUser' >/dev/null 2>&1 || useradd -m -s /bin/bash '$LinuxUser'"
    if ($LASTEXITCODE -ne 0) { throw "Failed to create Linux user" }

    wsl -d $DistroName -u root -- bash -lc "usermod -aG sudo '$LinuxUser'"
    if ($LASTEXITCODE -ne 0) { throw "Failed to add Linux user to sudo group" }

    wsl -d $DistroName -u root -- bash -lc "printf '[user]\ndefault=$LinuxUser\n' > /etc/wsl.conf"
    if ($LASTEXITCODE -ne 0) { throw "Failed to configure /etc/wsl.conf" }

    wsl --terminate $DistroName
    wsl --set-default $DistroName

    Write-Host "== Verification =="
    wsl -d $DistroName -- whoami
    wsl -d $DistroName -- pwd
    wsl -d $DistroName -- uname -a

    Write-Host "== Done =="
} finally {
    Stop-Transcript | Out-Null
}
