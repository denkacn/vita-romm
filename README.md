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
   debug_logging=0
   ```

6. Launch Vita RomM again and open your library.

On a trusted local network, HTTP is usually easier than HTTPS on PS Vita because certificate handling can be fragile on the console.

## Logs

Vita RomM writes its log here:

```text
ux0:data/vita-romm/install.log
```

For normal use, keep this option disabled:

```text
debug_logging=0
```

If the app gets stuck while loading platforms, opening a large platform, downloading covers, or downloading games, set it to:

```text
debug_logging=1
```

Then restart Vita RomM, reproduce the problem, and check `ux0:data/vita-romm/install.log`. Debug logging records network timing, RomM API page loading, worker startup and finish events, cover cache activity, and download details.

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

## Documentation

More detailed documentation is in the `Doc` folder:

- [User Guide](Doc/UserGuide.md)
- [Developer Notes](Doc/Developer.md)
- [RomM OpenAPI Snapshot](Doc/RomM-OpenAPI.md)

## Disclaimer

Vita RomM is provided for educational, research, preservation, and personal backup purposes only.

Vita RomM does not include, host, distribute, decrypt, bypass, or provide any copyrighted games, firmware, keys, licenses, tickets, or other proprietary Sony Interactive Entertainment material. It is only a client for a RomM server that you configure yourself.

You are responsible for complying with all laws in your country and for using this software only with content that you legally own and are allowed to back up. Do not use this project to download, install, share, or distribute copyrighted content without permission.

PlayStation, PS Vita, PSP, LiveArea, and related names are trademarks or registered trademarks of Sony Interactive Entertainment Inc. This project is independent homebrew software and is not affiliated with, endorsed by, sponsored by, or approved by Sony Interactive Entertainment.

## Status

Current version: `0.8.1`
