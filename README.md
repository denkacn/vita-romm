# Vita RomM

Vita RomM is a PS Vita homebrew client for browsing a self-hosted [RomM](https://github.com/rommapp/romm) library and downloading your own backed-up games directly on the Vita.

It is made for a simple handheld workflow: open the app, pick a platform, browse games with cover art, download or install a title, and keep your Vita/Adrenaline/RetroArch folders organized through one configurable client.

## What It Can Do

- Connect to your own RomM server with an API token.
- Show RomM platforms that have games available.
- Browse games with cached cover art.
- Download and install PS Vita game archives.
- Download PSP games for Adrenaline.
- Download PSX `.bin/.cue` sets or `EBOOT.PBP` games.
- Download common retro ROMs into configured emulator folders.
- Show downloaded and installed status where supported.
- Refresh LiveArea after installing PS Vita titles.

## Supported Platforms

Managed install platforms:

- PS Vita
- PSP

Verified folder-download platforms:

- PSX / PlayStation
- Game Boy Advance
- Game Boy
- Game Boy Color
- NES
- SNES
- Sega Mega Drive / Genesis

Experimental folder-download platforms:

- Nintendo 64
- Custom RomM platforms with a configured download path

For retro platforms, Vita RomM downloads files into the folder you configure. You launch those games with RetroArch, Adrenaline, or another emulator.

## Requirements

- A homebrew-enabled PS Vita or PSTV.
- VitaShell.
- Network access from the Vita to your RomM server.
- A RomM API token.
- Enough free space on `ux0:`.
- Adrenaline for PSP and PSX `EBOOT.PBP` games.
- RetroArch or another emulator for retro ROM folders.

## Quick Start

1. Copy `vita_romm.vpk` to your Vita.
2. Install it with VitaShell.
3. Launch `Vita RomM` once so it creates the data folder.
4. Edit the config file:

   ```text
   ux0:data/vita-romm/config.txt
   ```

5. Set your RomM server URL and API token:

   ```text
   server_url=http://your-romm-server.local/
   api_token=rmm_your_token_here
   ```

6. Launch Vita RomM again and open your library.

On a trusted local network, HTTP is usually easier than HTTPS on PS Vita because certificate handling can be fragile on the console.

## Configuration

Vita RomM stores all user settings in one file:

```text
ux0:data/vita-romm/config.txt
```

The app creates a default config on first launch. You can edit it directly in VitaShell, copy it over FTP/USB, or use the included [config.example.txt](config.example.txt) as a starting point.

Your real config should never be shared publicly because it contains your RomM API token.

### Required Settings

```text
server_url=http://your-romm-server.local/
api_token=rmm_your_token_here
```

`server_url` is the base URL of your RomM server. Use a URL that your Vita can reach on the same network.

Good examples:

```text
server_url=http://192.168.0.10:8080/
server_url=http://romm.local/
```

`api_token` is a RomM client token. You can create it in RomM from your user/account settings. The token usually starts with `rmm_`.

### PS Vita Paths

These paths control where Vita game archives are downloaded and where extracted content is installed:

```text
psvita_download_dir=ux0:data/vita-romm/downloads/psvita
psvita_app_dir=ux0:app
psvita_patch_dir=ux0:patch
psvita_repatch_dir=ux0:rePatch
psvita_addcont_dir=ux0:addcont
psvita_license_dir=ux0:license
```

Most users should keep these defaults. After installing a PS Vita game, run Refresh LiveArea from Vita RomM or VitaShell so the game bubble appears.

### PSP And Adrenaline Paths

These paths are used for PSP games:

```text
psp_install_dir=ux0:pspemu/ISO
psp_game_dir=ux0:pspemu/PSP/GAME
psp_download_dir=ux0:data/vita-romm/downloads/psp
```

`.iso` and `.cso` files go to `psp_install_dir`. `EBOOT.PBP` games go under `psp_game_dir`.

### PSX Paths

PSX has two supported install styles:

```text
platform.psx.path=ux0:data/vita-romm/roms/psx
platform.psx.pbp_path=ux0:pspemu/PSP/GAME
```

Use `platform.psx.path` for `.bin/.cue` games. Vita RomM keeps each PSX game in its own folder because `.cue` files must reference the matching `.bin` files by name.

Use `platform.psx.pbp_path` for PSX `EBOOT.PBP` games that you want to run through Adrenaline.

### Retro Platform Paths

Folder-download platforms need a target path in the config:

```text
platform.gba.path=ux0:data/vita-romm/roms/gba
platform.gb.path=ux0:data/vita-romm/roms/gb
platform.gbc.path=ux0:data/vita-romm/roms/gbc
platform.nes.path=ux0:data/vita-romm/roms/nes
platform.snes.path=ux0:data/vita-romm/roms/snes
platform.genesis.path=ux0:data/vita-romm/roms/genesis
platform.n64.path=ux0:data/vita-romm/roms/n64
```

You can point these paths to your existing RetroArch folders instead. For example:

```text
platform.snes.path=ux0:data/retroarch/roms/snes
```

If Vita RomM sees a RomM platform but no path is configured for it, the app will show that the platform needs a download path.

### Custom Platforms

You can add a custom RomM platform by using its RomM slug:

```text
platform.<slug>.path=ux0:data/retroarch/roms/<slug>
```

Custom platforms are treated as untested folder-download platforms. Vita RomM downloads the file, but you are responsible for using the correct emulator/core.

### Full Example

See [config.example.txt](config.example.txt) for a complete config template.

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
| Select | Refresh current cover |
| X | Download/install selected game |
| O | Back |

## Disclaimer

Vita RomM is provided for educational, research, preservation, and personal backup purposes only.

Vita RomM does not include, host, distribute, decrypt, bypass, or provide any copyrighted games, firmware, keys, licenses, tickets, or other proprietary Sony Interactive Entertainment material. It is only a client for a RomM server that you configure yourself.

You are responsible for complying with all laws in your country and for using this software only with content that you legally own and are allowed to back up. Do not use this project to download, install, share, or distribute copyrighted content without permission.

PlayStation, PS Vita, PSP, LiveArea, and related names are trademarks or registered trademarks of Sony Interactive Entertainment Inc. This project is independent homebrew software and is not affiliated with, endorsed by, sponsored by, or approved by Sony Interactive Entertainment.

## Status

Current version: `0.8.0`
