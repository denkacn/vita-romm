# Vita RomM

Vita RomM is a PS Vita homebrew client for browsing a self-hosted [RomM](https://github.com/rommapp/romm) library, installing owned PS Vita/PSP backups, and downloading retro ROM files into configured emulator folders.

The app is built for a personal homebrew workflow: open Vita RomM, choose a platform, browse games with cover art, download/install a selected title, and refresh LiveArea for installed Vita games.

## Disclaimer

This project is provided for educational, research, preservation, and personal backup purposes only.

Vita RomM does not include, host, distribute, decrypt, bypass, or provide any copyrighted games, firmware, keys, licenses, tickets, or other proprietary Sony Interactive Entertainment material. It is only a client that talks to a RomM server that you configure yourself.

You are responsible for complying with all laws in your country and for using this software only with content that you legally own and are allowed to back up. Do not use this project to download, install, share, or distribute copyrighted content without permission.

PlayStation, PS Vita, PSP, LiveArea, and related names are trademarks or registered trademarks of Sony Interactive Entertainment Inc. This project is independent homebrew software and is not affiliated with, endorsed by, sponsored by, or approved by Sony Interactive Entertainment.

## Features

- RomM server connection using `server_url` and `api_token`.
- Dynamic platform picker for RomM platforms that contain games.
- Game browser with cover/poster cache.
- Full game list overlay for quick selection.
- Download progress while fetching game files.
- ZIP extraction/install flow for PS Vita packages.
- PSP ISO/CSO/PBP install flow.
- PSX `.bin/.cue` folder download flow and PSX `EBOOT.PBP` support.
- Folder download flow for verified and custom retro platforms.
- Downloaded/installed status badges.
- Manual Refresh LiveArea action from the main screen.
- Optional experimental automatic LiveArea refresh after Vita installs.
- App data, downloads, covers, logs, and config stored under `ux0:data/vita-romm` by default.

## Supported Platforms

Managed install platforms:

- PlayStation Vita
- PlayStation Portable

Verified folder-download platforms:

- PlayStation / PSX
- Game Boy Advance
- Game Boy
- Game Boy Color
- NES
- SNES
- Sega Mega Drive / Genesis

Untested folder-download platforms:

- Nintendo 64
- Any custom RomM platform with `platform.<slug>.path` configured

For folder-download platforms, Vita RomM only downloads files to the configured folder. You run those games with RetroArch or another emulator on your Vita.

## Requirements

On the PS Vita:

- A homebrew-enabled PS Vita or PSTV.
- VitaShell installed.
- Network access to your RomM server.
- Enough free space on `ux0:` for downloads and installed games.
- Adrenaline installed if you want to play PSP or PSX `EBOOT.PBP` games.
- RetroArch or another emulator installed if you want to use folder-download retro platforms.

On the server side:

- A working RomM instance.
- A RomM API token.
- RomM platforms mapped for the systems you want to use.
- Your own legally backed up game files in RomM.

## Installing The VPK

1. Copy `vita_romm.vpk` to your PS Vita.

   Common location:

   ```text
   ux0:data/vita_romm.vpk
   ```

2. Open VitaShell.
3. Navigate to the copied VPK.
4. Press `X` to install it.
5. Launch `Vita RomM` from LiveArea.

The app Title ID is:

```text
ROMM00001
```

Updating the app with a newer VPK should update the same bubble.

## Configuration File

Vita RomM reads its config from:

```text
ux0:data/vita-romm/config.txt
```

If the file does not exist, the app creates a default one on first launch. After the first launch, edit it in VitaShell or copy your prepared config to that path.

### Minimal Config

```text
server_url=http://your-romm-server.local/
api_token=rmm_your_token_here
```

For local/private networks, plain HTTP is usually easier on Vita than HTTPS because old Vita SSL/CA handling can be painful. HTTPS may work if your setup and certificates are compatible, but HTTP on a trusted LAN is the simplest path.

### Full Config Example

```text
server_url=http://your-romm-server.local/
api_token=rmm_your_token_here
psvita_download_dir=ux0:data/vita-romm/downloads/psvita
psvita_app_dir=ux0:app
psvita_patch_dir=ux0:patch
psvita_repatch_dir=ux0:rePatch
psvita_addcont_dir=ux0:addcont
psvita_license_dir=ux0:license
psp_install_dir=ux0:pspemu/ISO
psp_game_dir=ux0:pspemu/PSP/GAME
psp_download_dir=ux0:data/vita-romm/downloads/psp
platform.psx.path=ux0:data/vita-romm/roms/psx
platform.psx.pbp_path=ux0:pspemu/PSP/GAME
platform.gba.path=ux0:data/vita-romm/roms/gba
platform.gb.path=ux0:data/vita-romm/roms/gb
platform.gbc.path=ux0:data/vita-romm/roms/gbc
platform.nes.path=ux0:data/vita-romm/roms/nes
platform.snes.path=ux0:data/vita-romm/roms/snes
platform.genesis.path=ux0:data/vita-romm/roms/genesis
platform.n64.path=ux0:data/vita-romm/roms/n64
auto_refresh_livearea=0
```

Do not commit your real `api_token` to a public repository.

## Config Keys

| Key | Description |
| --- | --- |
| `server_url` | Base URL of your RomM server. Example: `http://192.168.0.10:8080/` |
| `api_token` | RomM API token. Usually starts with `rmm_`. |
| `psvita_download_dir` | Where PS Vita archives are downloaded before extraction. |
| `psvita_app_dir` | Target directory for Vita `app/<TITLEID>` content. Usually `ux0:app`. |
| `psvita_patch_dir` | Target directory for Vita `patch/<TITLEID>` content. Usually `ux0:patch`. |
| `psvita_repatch_dir` | Target directory for Vita `rePatch/<TITLEID>` content. Usually `ux0:rePatch`. |
| `psvita_addcont_dir` | Target directory for Vita DLC/addcont content. Usually `ux0:addcont`. |
| `psvita_license_dir` | Target directory for Vita licenses. Usually `ux0:license`. |
| `psp_install_dir` | Target directory for PSP `.iso` and `.cso` files. Usually `ux0:pspemu/ISO`. |
| `psp_game_dir` | Target directory for PSP homebrew/PSN-style `EBOOT.PBP` games. Usually `ux0:pspemu/PSP/GAME`. |
| `psp_download_dir` | Where PSP ZIP or temporary downloads are stored. |
| `platform.psx.path` | Target folder for PSX `.bin/.cue` file sets. |
| `platform.psx.pbp_path` | Target folder for PSX `EBOOT.PBP` games used by Adrenaline. Defaults to `ux0:pspemu/PSP/GAME`. |
| `platform.<slug>.path` | Target folder for folder-download platforms. Example: `platform.gba.path=ux0:data/retroarch/roms/gba`. |
| `auto_refresh_livearea` | `0` disables automatic LiveArea refresh. `1` enables experimental automatic refresh. |

## RomM Folder / Platform Setup

The client shows every RomM platform that has at least one game.

Managed platforms use special install logic:

```text
psvita
psp
```

Verified folder-download slugs:

```text
psx
gba
gb
gbc
nes
snes
genesis
```

Experimental/untested:

```text
n64
```

Custom platforms can be enabled by adding:

```text
platform.<slug>.path=ux0:data/retroarch/roms/<slug>
```

If a RomM platform has games but no configured path, Vita RomM shows it as `Path required` and displays the config line you need to add.

Your exact folder names on the RomM server can be different, but they should be mapped in RomM to the correct platform:

- `psvita` should map to PlayStation Vita.
- `psp` should map to PlayStation Portable.

The app reads the RomM platform IDs from the API and then uses those IDs to fetch counts and game lists.

## PS Vita Install Formats

Vita RomM supports Vita ZIP archives that contain Vita game folders such as:

```text
app/PCSB00079/eboot.bin
patch/PCSB00079/...
rePatch/PCSB00079/...
addcont/PCSB00079/...
license/...
```

It also handles archives that have one extra root folder before the Vita layout, for example:

```text
Super Meat Boy/app/PCSE00769/eboot.bin
Super Meat Boy/patch/PCSE00769/...
Super Meat Boy/repatch/PCSE00769/...
```

During install, files are extracted to the configured Vita directories:

```text
ux0:app
ux0:patch
ux0:rePatch
ux0:addcont
ux0:license
```

After installing a Vita game, run Refresh LiveArea from Vita RomM or VitaShell so the game bubble appears.

## PSP Install Formats

PSP games are installed for Adrenaline.

Supported direct files:

```text
.iso
.cso
.pbp
```

Default install paths:

```text
ux0:pspemu/ISO
ux0:pspemu/PSP/GAME
```

Expected behavior:

- `.iso` and `.cso` files are copied to `psp_install_dir`.
- `.pbp` files are installed as `psp_game_dir/<game-folder>/EBOOT.PBP`.
- ZIP archives are scanned for `.iso`, `.cso`, or `EBOOT.PBP` and extracted to the matching PSP location.

Adrenaline is responsible for showing and running PSP content. Vita RomM only places the files in the configured PSP/Adrenaline folders.

## PSX Formats

PSX has two supported flows:

```text
EBOOT.PBP -> platform.psx.pbp_path/<game-folder>/EBOOT.PBP
.bin/.cue -> platform.psx.path/<game-folder>/
```

For `.bin/.cue` games, Vita RomM downloads the whole file set into a dedicated game folder and preserves file names because `.cue` files reference the exact `.bin` names.

If a PSX game is stored in a ZIP archive:

- ZIP with `EBOOT.PBP` is extracted to `platform.psx.pbp_path/<game-folder>/EBOOT.PBP`.
- ZIP with `.cue` and `.bin` files is extracted to `platform.psx.path/<game-folder>/`.

## Retro Folder Platforms

Folder-download platforms copy ROM files into the configured folder:

```text
gba: .gba
gb: .gb
gbc: .gbc
nes: .nes
snes: .sfc .smc
genesis: .md .gen .bin
n64: .z64 .n64 .v64
```

ZIP archives are scanned for a matching ROM file. For custom platforms, Vita RomM downloads to the configured path but marks the platform as untested.

## Cover Art

The app downloads cover/poster images exposed by RomM and caches them on the Vita under:

```text
ux0:data/vita-romm/covers
```

If a cover does not show:

- Make sure the cover exists in RomM.
- Make sure the Vita can access the RomM server URL.
- Try HTTP if HTTPS/certificates are causing problems.
- Delete the cached cover file and reopen the game if you suspect a bad cached download.

## LiveArea Refresh

Vita RomM has a manual Refresh LiveArea button on the platform screen.

Recommended workflow:

1. Install/download a Vita game.
2. Return to the platform screen.
3. Press `Triangle` to run Refresh LiveArea.
4. Wait for the progress to finish.
5. Return to LiveArea and check for the new bubble.

If the built-in refresh fails, open VitaShell and use its Refresh LiveArea option.

The `auto_refresh_livearea=1` setting enables an experimental automatic refresh after install. Keep it disabled if you prefer the safer manual flow.

## Controls

Platform screen:

| Button | Action |
| --- | --- |
| Left / Right | Select platform |
| X | Open selected platform |
| Triangle | Refresh LiveArea |

Game screen:

| Button | Action |
| --- | --- |
| Left / Right | Browse games |
| Square | Open full game list |
| X | Download/install selected game |
| O | Back to platform screen |

Game list:

| Button | Action |
| --- | --- |
| Up / Down | Move selection |
| X | Select game and return to game screen |
| O | Close list |
| Square | Close list |

## App Data Layout

Default data directory:

```text
ux0:data/vita-romm
```

Common files and folders:

```text
ux0:data/vita-romm/config.txt
ux0:data/vita-romm/install.log
ux0:data/vita-romm/covers/
ux0:data/vita-romm/downloads/psvita/
ux0:data/vita-romm/downloads/psp/
```

## Build From Source

This project is built with VitaSDK.

Expected development setup:

- Windows host.
- WSL2 with VitaSDK installed.
- CMake.
- VitaSDK libraries used by the app: `vita2d`, `libcurl`, `libzip`, PNG/JPEG/zlib-related dependencies.

Build from Windows PowerShell:

```powershell
D:\MyProjects\Software\vita-romm\work\build-romm-vita.ps1
```

Build outputs:

```text
D:\MyProjects\Software\vita-romm\outputs\vita_romm.vpk
D:\MyProjects\Software\vita-romm\outputs\vita_romm_eboot.bin
```

Direct WSL build from the app source folder:

```bash
cd /mnt/d/MyProjects/Software/vita-romm/work/romm-vita-skeleton
./scripts/build.sh
```

## Deploy During Development

The simplest install path is VitaShell FTP:

1. Open VitaShell on the Vita.
2. Press `SELECT` to start FTP.
3. Copy the VPK to `ux0:data/`.
4. Install the VPK from VitaShell.

After the VPK is installed once, development can be faster by replacing only:

```text
ux0:app/ROMM00001/eboot.bin
```

Then relaunch the app bubble.

## Troubleshooting

### App cannot connect to RomM

- Check `server_url` in `ux0:data/vita-romm/config.txt`.
- Make sure the URL is reachable from the Vita browser or another device on the same network.
- Prefer HTTP on a trusted LAN if HTTPS certificate validation causes trouble.
- Check that `api_token` is valid.

### Games list is empty

- Confirm the RomM platforms are mapped to PS Vita and PSP.
- Confirm the platform has games in RomM.
- Confirm the token has access to the RomM API.

### Covers do not appear

- Check if covers appear in the RomM web UI.
- Check network access from Vita to RomM.
- Delete cached covers in `ux0:data/vita-romm/covers` and try again.

### Vita game installed but no bubble appears

- Run Refresh LiveArea from Vita RomM.
- If that fails, run Refresh LiveArea from VitaShell.
- Confirm the game was extracted under `ux0:app/<TITLEID>` and contains `eboot.bin`.

### PSP game does not appear in Adrenaline

- Confirm Adrenaline is installed and configured.
- ISO/CSO files should be under `ux0:pspemu/ISO`.
- EBOOT.PBP games should be under `ux0:pspemu/PSP/GAME/<folder>/EBOOT.PBP`.

## Project Status

Current version: `0.8.0`.

The app currently focuses on PS Vita and PSP installation workflows. The UI and platform list are expected to evolve as more emulated platforms are added.
