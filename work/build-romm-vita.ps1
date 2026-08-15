$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ScriptPath = Join-Path $ScriptDir "build-romm-vita-linuxfs.sh"
$LogPath = Join-Path $ScriptDir "romm-vita-build.log"
$WslScriptPath = (wsl -d Ubuntu-24.04-H -u root -- wslpath -a "$ScriptPath").Trim()

wsl -d Ubuntu-24.04-H -u root -- bash "$WslScriptPath" *> $LogPath
