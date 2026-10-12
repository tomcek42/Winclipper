# Updater

Winclipper can install updates automatically ("Install updates automatically" in Settings, off by default).
The upstream updater was removed because it installed unsigned MSIs from the original author's domain (F8).
This one trusts a signing key, not a server.

## How it works

1. Two minutes after start (ten seconds after the setting is turned on), and then every 24 hours, Winclipper
   downloads `winclipper-update.txt` and `winclipper-update.txt.minisig` from the latest release of
   `tomcek42/Winclipper` over HTTPS.
2. The manifest must carry a valid minisign signature (Ed25519 over BLAKE2b-512, the format the Tauri
   updater uses) from the release signing key, and the trusted comment must carry a valid global signature.
   Legacy (non-prehashed) signatures are refused. The public key is compiled into `Winclipper/Updater.cpp`
   and also stored in `tools/winclipper-update.pub`. Verification uses the vendored Monocypher 4.0.3
   (`Winclipper/monocypher/`), because Windows CNG has no Ed25519.
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

Releases are built and signed by GitHub Actions (`.github/workflows/release.yml`):

1. Bump the version in the three places listed in `CLAUDE.md` and add a `## <version>` section to `CHANGELOG.md`.
2. Commit, then push a tag `v1.6.0` (a trailing `.0` is dropped). The workflow checks that the tag matches the
   sources, builds both EXEs and MSIs, signs the manifest and publishes the release with all four files.
   Its release notes are the matching `CHANGELOG.md` section plus the MSI hashes.
3. Running the workflow manually (`workflow_dispatch`) only builds and uploads the MSIs as artifacts.

The workflow needs two repository secrets: `WINCLIPPER_UPDATE_KEY` (the full content of the minisign `.key`
file) and `WINCLIPPER_UPDATE_KEY_PASSWORD`. A manual fallback still works locally:
`pwsh tools/sign-update.ps1 -Version 1.6.0.0` (needs `minisign`, asks for the password).

## The signing key

The secret key is a password-protected minisign key file (scrypt-encrypted), by default at
`%USERPROFILE%\.winclipper\winclipper-update.key`. It is never committed. Unlike a DPAPI blob it is not bound to
a Windows profile: the file plus its password work on any machine. Keep a backup copy and the password in
separate safe places (for example a password manager); it can also be stored as a GitHub Actions secret if
signing moves to CI later. If it is lost, no further update can be signed, and installed copies need a manual
update to a build with a new public key. If it leaks together with its password, an attacker who can also
serve files from the fork's latest release can push an update; rotate the key in `Updater.cpp` and
`tools/winclipper-update.pub` and ship that build manually.

Create a new key pair (minisign asks for the password):

```
minisign -G -p tools\winclipper-update.pub -s %USERPROFILE%\.winclipper\winclipper-update.key
```

Then put the second line of `tools/winclipper-update.pub` into `SIGNING_PUBLIC_KEY` in `Updater.cpp`.
`tools/sign-update.ps1` refuses to sign if the two differ.
