# Developer Notes

Vita RomM is built with VitaSDK.

## Source Layout

```text
work/romm-vita-skeleton/    Main Vita app source
work/build-romm-vita.ps1    Windows helper build script
outputs/                    Built VPK/eboot/config artifacts
licenses/                   Third-party license notes
Doc/                        Project documentation
```

## Requirements

- Windows host.
- WSL2 with VitaSDK installed.
- CMake.
- VitaSDK libraries used by the app: `vita2d`, `libcurl`, `libzip`, PNG/JPEG/zlib-related dependencies.

## Build From Source

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

## Release Checklist

- Make sure no real `server_url` or `api_token` values are committed.
- Build a fresh `vita_romm.vpk`.
- Install the VPK on a Vita and launch it.
- Test RomM connection, platform loading, cover loading, and at least one download/install path.
- For Vita installs, test Refresh LiveArea.
- Keep generated personal config files out of release archives.
