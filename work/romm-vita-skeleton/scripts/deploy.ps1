param(
    [string]$VitaIp = "",
    [int]$Port = 1337,
    [switch]$EbootOnly
)

$ErrorActionPreference = "Stop"

$OutputDir = "C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\outputs"
$BuildScript = "C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\work\build-romm-vita.ps1"
$TitleId = "ROMM00001"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

& $BuildScript

if (-not $VitaIp) {
    Write-Host "Built VPK: $OutputDir\romm_vita.vpk"
    Write-Host "Pass -VitaIp x.x.x.x to upload through VitaShell FTP."
    exit 0
}

if ($EbootOnly) {
    curl.exe --ftp-create-dirs -T "$OutputDir\romm_vita_eboot.bin" "ftp://$VitaIp`:$Port/ux0:app/$TitleId/eboot.bin"
    Write-Host "Uploaded eboot.bin to ux0:app/$TitleId/eboot.bin"
} else {
    curl.exe -T "$OutputDir\romm_vita.vpk" "ftp://$VitaIp`:$Port/ux0:data/"
    Write-Host "Uploaded romm_vita.vpk to ux0:data/"
}
