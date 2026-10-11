# Updater

Winclipper can install updates automatically ("Install updates automatically" in Settings, off by default).
The upstream updater was removed because it installed unsigned MSIs from the original author's domain (F8).
This one trusts a signing key, not a server.

## How it works

1. Two minutes after start (ten seconds after the setting is turned on), and then every 24 hours, Winclipper
   downloads `winclipper-update.txt` and `winclipper-update.sig` from the latest release of
   `tomcek42/Winclipper` over HTTPS.
2. The manifest must carry a valid ECDSA P-256 signature from the release signing key. The public key is
   compiled into `Winclipper/Updater.cpp`.
3. The manifest version must be higher than the running EXE's version resource. Older signed manifests
   cannot roll back an installation.
4. The MSI URL must start with `https://github.com/tomcek42/Winclipper/releases/download/`. The MSI may be
   at most 64 MB, and its SHA-256 must match the manifest.
5. The MSI is written to `%LOCALAPPDATA%\Winclipper\Winclipper\Update\` and kept open without write or delete
   sharing until msiexec has finished, so it cannot be swapped after the hash check.
6. `msiexec /i … /passive /norestart` runs. Windows asks for elevation, because the package installs per
   machine. The Restart Manager closes Winclipper and restarts it after the upgrade
   (`RegisterApplicationRestart`). If the user cancels the elevation prompt, that version is not offered
   again until the next start.

While steps 4 to 6 run, the tray icon blinks and its tooltip reads "Winclipper - installing update".
Diagnostics go to `OutputDebugString` (view with DebugView).

Manifest format (ASCII, LF line endings):

```
winclipper-update 1
version 1.6.0.0
x64 <sha256 hex> https://github.com/tomcek42/Winclipper/releases/download/v1.6.0/WinclipperSetup-x64.msi
x86 <sha256 hex> https://github.com/tomcek42/Winclipper/releases/download/v1.6.0/WinclipperSetup-x86.msi
```

## Releasing an update

1. Bump the version in the three places listed in `CLAUDE.md`, then build both EXEs and both MSIs into `dist/`.
2. `pwsh tools/sign-update.ps1 -Version 1.6.0.0` writes `dist/winclipper-update.txt` and `.sig`.
3. Create the release `v1.6.0` with all four files as assets. The tag must match the URLs in the manifest
   (a trailing `.0` is dropped).

## The signing key

The private key is DPAPI-protected for the project owner at
`%USERPROFILE%\.winclipper\update-signing-key.dpapi`. It is never committed. Back it up, for example
exported to an offline medium. If it is lost, no further update can be signed. Installed copies then need
a manual update to a build with a new public key. If it leaks, an attacker who can also serve files from
the fork's latest release can push an update. Rotate the key in `Updater.cpp` and ship that build manually.
