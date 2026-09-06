param(
    [string]$VitaIp = "",
    [int]$Port = 1337,
    [switch]$EbootOnly
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$OutputDir = Join-Path $RepoRoot "outputs"
$BuildScript = Join-Path $RepoRoot "work\build-romm-vita.ps1"
$TitleId = "ROMM00001"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

& $BuildScript

if (-not $VitaIp) {
    Write-Host "Built VPK: $OutputDir\vita_romm.vpk"
    Write-Host "Pass -VitaIp x.x.x.x to upload through VitaShell FTP."
    exit 0
}

if ($EbootOnly) {
    curl.exe --ftp-create-dirs -T "$OutputDir\vita_romm_eboot.bin" "ftp://$VitaIp`:$Port/ux0:app/$TitleId/eboot.bin"
    Write-Host "Uploaded eboot.bin to ux0:app/$TitleId/eboot.bin"
} else {
    curl.exe -T "$OutputDir\vita_romm.vpk" "ftp://$VitaIp`:$Port/ux0:data/"
    Write-Host "Uploaded vita_romm.vpk to ux0:data/"
}
