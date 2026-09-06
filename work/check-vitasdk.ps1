$ErrorActionPreference = "Stop"

$LogPath = Join-Path $PSScriptRoot "vitasdk-check.log"

wsl -d Ubuntu-24.04-H -u root -- bash -lc @'
export VITASDK=/usr/local/vitasdk
export PATH=/usr/local/vitasdk/bin:$PATH
echo TOOLS
which arm-vita-eabi-gcc
arm-vita-eabi-gcc --version | head -n 1
which vita-mksfoex
which vita-pack-vpk
echo LIBS
find /usr/local/vitasdk/arm-vita-eabi \( -iname "*curl*" -o -iname "*jansson*" -o -iname "*SDL2*" -o -iname "*vita2d*" \) | head -n 120
'@ *> $LogPath
