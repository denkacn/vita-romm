# RomM Vita Skeleton

Small PS Vita homebrew skeleton for a future RomM client.

## Features

- LiveArea icon.
- Loading screen that fetches RomM platforms on startup.
- Platform picker for PS Vita and PSP.
- Game carousel with name and cached poster preview when RomM exposes a cover URL.
- `X` opens a platform or downloads/installs the selected game.
- `O` returns from games to platform selection.
- Server URL, token, and install paths are read from `ux0:data/romm-vita/config.txt`.

## Build

From Windows PowerShell:

```powershell
C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\work\build-romm-vita.ps1
```

This builds inside the WSL ext4 filesystem and copies artifacts to:

```text
C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\outputs\romm_vita.vpk
C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\outputs\romm_vita_eboot.bin
```

Direct WSL build also works:

```bash
cd /mnt/c/Users/Denkacn/Documents/Codex/2026-08-14/xn/work/romm-vita-skeleton
./scripts/build.sh
```

The direct WSL VPK is created at:

```text
build/romm_vita.vpk
```

## Upload Through VitaShell FTP

On Vita:

1. Open VitaShell.
2. Press `SELECT`.
3. Note the IP address and port, usually `1337`.

From Windows PowerShell:

```powershell
C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\work\romm-vita-skeleton\scripts\deploy.ps1 -VitaIp 192.168.1.50
```

This uploads `romm_vita.vpk` to:

```text
ux0:data/
```

Install it from VitaShell with `X`.

## Faster EBOOT Deploy

After installing the VPK once, you can replace only `eboot.bin`:

```powershell
C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\work\romm-vita-skeleton\scripts\deploy.ps1 -VitaIp 192.168.1.50 -EbootOnly
```

Then relaunch the bubble.

## Config

On Vita:

```text
ux0:data/romm-vita/config.txt
```

Example:

```text
server_url=http://romm.bypuziki.com/
api_token=rmm_your_new_token
psvita_download_dir=ux0:data/romm-vita/downloads/psvita
psvita_app_dir=ux0:app
psvita_patch_dir=ux0:patch
psvita_repatch_dir=ux0:rePatch
psvita_addcont_dir=ux0:addcont
psvita_license_dir=ux0:license
psp_install_dir=ux0:pspemu/ISO
psp_game_dir=ux0:pspemu/PSP/GAME
psp_download_dir=ux0:data/romm-vita/downloads/psp
auto_refresh_livearea=0
```

PS Vita ZIP archives are auto-routed when they contain paths like `app/PCSB00079/eboot.bin`, `patch/...`, `repatch/...`, `addcont/...`, or `license/...`.
PSP `.iso` and `.cso` files go to `psp_install_dir`. PSP `.pbp` files go to `psp_game_dir/<name>/EBOOT.PBP`.
PSP ZIP files are scanned for `.iso`, `.cso`, or `EBOOT.PBP`; archives with `EBOOT.PBP` are extracted as `psp_game_dir/<folder>/...`.

After a PS Vita ZIP install, the app leaves files in place and asks you to run VitaShell Refresh LiveArea.
Set `auto_refresh_livearea=1` to try the experimental `ScePromoterUtil` refresh path.

## VitaCompanion

VitaCompanion requires a plugin/app running on the Vita side. The PC-side CLI often used with it is:

```bash
npm install -g vitacompanion-cli
vita ip 192.168.1.50
vita p
```

I did not install the npm CLI automatically because global npm installs run third-party scripts. The FTP deploy path above is safer and already works with VitaShell.
