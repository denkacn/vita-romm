$ErrorActionPreference = "Stop"

$DistroName = "Ubuntu-24.04-H"
$InstallDir = "H:\WSL\Ubuntu-24.04"
$DownloadDir = "H:\WSL\Downloads"
$LogPath = "C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\work\install-wsl2-h.log"
$RootfsUrl = "https://cloud-images.ubuntu.com/wsl/releases/24.04/current/ubuntu-noble-wsl-amd64-wsl.rootfs.tar.gz"
$RootfsPath = Join-Path $DownloadDir "ubuntu-noble-wsl-amd64-wsl.rootfs.tar.gz"

Start-Transcript -Path $LogPath -Force | Out-Null

try {

function Test-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Admin)) {
    throw "Run this script from an elevated PowerShell window."
}

Write-Host "== Enabling Windows features for WSL2 =="
$features = @(
    "Microsoft-Windows-Subsystem-Linux",
    "VirtualMachinePlatform"
)

foreach ($feature in $features) {
    dism.exe /online /enable-feature /featurename:$feature /all /norestart
    if (($LASTEXITCODE -ne 0) -and ($LASTEXITCODE -ne 3010)) {
        throw "DISM failed while enabling $feature"
    }
    if ($LASTEXITCODE -eq 3010) {
        Write-Warning "$feature was enabled, but Windows reports that a reboot is required."
    }
}

New-Item -ItemType Directory -Force -Path $InstallDir, $DownloadDir | Out-Null

Write-Host "== Setting WSL default version to 2 =="
try {
    wsl --set-default-version 2
} catch {
    Write-Warning "Could not set default WSL version yet. A reboot or WSL kernel update may be required."
}

Write-Host "== Updating WSL kernel/app if supported =="
try {
    wsl --update --web-download
} catch {
    try {
        wsl --update
    } catch {
        Write-Warning "WSL update command is unavailable on this system. Continuing with the inbox WSL."
    }
}

if (-not (Test-Path $RootfsPath)) {
    Write-Host "== Downloading Ubuntu 24.04 WSL rootfs =="
    Invoke-WebRequest -Uri $RootfsUrl -OutFile $RootfsPath
} else {
    Write-Host "== Ubuntu rootfs already downloaded =="
}

$existingDistros = @(wsl --list --quiet 2>$null)
if ($existingDistros -contains $DistroName) {
    Write-Host "== Distro $DistroName already exists; skipping import =="
} else {
    Write-Host "== Importing Ubuntu into $InstallDir =="
    wsl --import $DistroName $InstallDir $RootfsPath --version 2
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Import failed. If Windows just enabled WSL features, reboot and run this script again."
        exit $LASTEXITCODE
    }
}

wsl --set-default $DistroName

$linuxUser = ($env:USERNAME -replace '[^a-zA-Z0-9_-]', '').ToLowerInvariant()
if ([string]::IsNullOrWhiteSpace($linuxUser)) {
    $linuxUser = "vita"
}

Write-Host "== Creating default Linux user: $linuxUser =="
$setupUserScript = @"
set -e
if ! id -u "$linuxUser" >/dev/null 2>&1; then
    useradd -m -s /bin/bash "$linuxUser"
fi
usermod -aG sudo "$linuxUser"
echo "$linuxUser ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/$linuxUser"
chmod 0440 "/etc/sudoers.d/$linuxUser"
printf "[user]\ndefault=%s\n" "$linuxUser" > /etc/wsl.conf
"@

wsl -d $DistroName -u root -- bash -lc $setupUserScript
wsl --terminate $DistroName

Write-Host "== Done =="
Write-Host "Distro: $DistroName"
Write-Host "Path:   $InstallDir"
Write-Host "Run:    wsl -d $DistroName"
} finally {
    Stop-Transcript | Out-Null
}
