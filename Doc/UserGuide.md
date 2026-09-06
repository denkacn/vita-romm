# Vita RomM User Guide

This guide explains how to install Vita RomM, configure it, and prepare your RomM library for PS Vita, PSP, PSX, and retro platforms.

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

Installing a newer VPK should update the same bubble.

## Configuration File

Vita RomM reads its config from:

```text
ux0:data/vita-romm/config.txt
```

If the file does not exist, the app creates a default one on first launch. After the first launch, edit it in VitaShell or copy your prepared config to that path.

## Minimal Config

```text
server_url=http://your-romm-server.local/
api_token=rmm_your_token_here
```

Do not share your real `api_token` publicly.

## Full Config Example

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
| `psp_game_dir` | Target directory for PSP `EBOOT.PBP` games. Usually `ux0:pspemu/PSP/GAME`. |
| `psp_download_dir` | Where PSP ZIP or temporary downloads are stored. |
| `platform.psx.path` | Target folder for PSX `.bin/.cue` file sets. |
| `platform.psx.pbp_path` | Target folder for PSX `EBOOT.PBP` games used by Adrenaline. Defaults to `ux0:pspemu/PSP/GAME`. |
| `platform.<slug>.path` | Target folder for folder-download platforms. Example: `platform.gba.path=ux0:data/retroarch/roms/gba`. |
| `auto_refresh_livearea` | `0` disables automatic LiveArea refresh. `1` enables experimental automatic refresh after Vita installs. |

## RomM Platform Setup

Vita RomM reads platforms from RomM and shows platforms that have at least one game.

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

Experimental:

```text
n64
```

Custom platforms can be enabled with:

```text
platform.<slug>.path=ux0:data/retroarch/roms/<slug>
```

If a platform has games but no configured path, Vita RomM shows it as requiring a path.

## PS Vita Install Formats

Vita RomM supports Vita ZIP archives that contain Vita game folders such as:

```text
app/PCSB00079/eboot.bin
patch/PCSB00079/...
rePatch/PCSB00079/...
addcont/PCSB00079/...
license/...
```

Archives with one extra root folder are also supported:

```text
Super Meat Boy/app/PCSE00769/eboot.bin
Super Meat Boy/patch/PCSE00769/...
Super Meat Boy/repatch/PCSE00769/...
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

Adrenaline is responsible for showing and running PSP content.

## PSX Formats

PSX has two supported flows:

```text
EBOOT.PBP -> platform.psx.pbp_path/<game-folder>/EBOOT.PBP
.bin/.cue -> platform.psx.path/<game-folder>/
```

For `.bin/.cue` games, Vita RomM downloads the whole file set into a dedicated game folder and preserves file names because `.cue` files reference exact `.bin` names.

If a PSX game is stored in a ZIP archive:

- ZIP with `EBOOT.PBP` is extracted to `platform.psx.pbp_path/<game-folder>/EBOOT.PBP`.
- ZIP with `.cue` and `.bin` files is extracted to `platform.psx.path/<game-folder>/`.

## Retro Folder Platforms

Folder-download platforms save ROM files into the configured folder:

```text
gba: .gba
gb: .gb
gbc: .gbc
nes: .nes
snes: .sfc .smc
genesis: .md .gen .bin
n64: .z64 .n64 .v64
```

For simple retro platforms such as SNES, NES, GBA, GB, GBC, and Genesis, ZIP files are kept as ZIP files when that is what RomM provides. RetroArch cores commonly support zipped ROMs for these systems, but behavior can depend on the core.

## Cover Art

The app downloads cover/poster images exposed by RomM and caches them on the Vita under:

```text
ux0:data/vita-romm/covers
```

If you update a cover in RomM, open the game in Vita RomM and press `Select` to refresh the cached cover.

## LiveArea Refresh

Recommended workflow:

1. Install/download a Vita game.
2. Return to the platform screen.
3. Press `Triangle` to run Refresh LiveArea.
4. Wait for the progress to finish.
5. Return to LiveArea and check for the new bubble.

If the built-in refresh fails, open VitaShell and use its Refresh LiveArea option.

## Troubleshooting

### App cannot connect to RomM

- Check `server_url`.
- Make sure the Vita and RomM server are on the same network.
- Prefer HTTP on a trusted LAN if HTTPS certificate validation fails.
- Check that `api_token` is valid.

### Games list is empty

- Confirm the platform has games in RomM.
- Confirm the token has access to the RomM API.
- Confirm custom platforms have `platform.<slug>.path` configured.

### Covers do not appear

- Check if covers appear in the RomM web UI.
- Press `Select` on the game screen to refresh the current cover.
- Check network access from Vita to RomM.
- Try clearing `ux0:data/vita-romm/covers`.

### Vita game installed but no bubble appears

- Run Refresh LiveArea from Vita RomM.
- If that fails, run Refresh LiveArea from VitaShell.
- Confirm the game was extracted under `ux0:app/<TITLEID>` and contains `eboot.bin`.

### PSP game does not appear in Adrenaline

- Confirm Adrenaline is installed and configured.
- ISO/CSO files should be under `ux0:pspemu/ISO`.
- EBOOT.PBP games should be under `ux0:pspemu/PSP/GAME/<folder>/EBOOT.PBP`.
