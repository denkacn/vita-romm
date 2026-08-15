$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ScriptPath = (Resolve-Path (Join-Path $ScriptDir "build-romm-vita-linuxfs.sh")).Path
$LogPath = Join-Path $ScriptDir "romm-vita-build.log"
$Drive = $ScriptPath.Substring(0, 1).ToLowerInvariant()
$Rest = $ScriptPath.Substring(2).Replace('\', '/')
$WslScriptPath = "/mnt/$Drive$Rest"

wsl -d Ubuntu-24.04-H -u root -- bash "$WslScriptPath" *> $LogPath
