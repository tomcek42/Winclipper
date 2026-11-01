# Updater

Winclipper can notify about new versions ("Notify me about updates" in Settings, off by default) and installs
one only after the user confirms it. "Check now" in Settings checks once, whether notifications are on or not.
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
   cannot roll back an installation. If it is, the tray icon blinks and a notification appears (once per
   version and session). Clicking the notification, or "Install update …" in the tray menu, opens a dialog
   with a link to the release notes. On the Settings > Updates page the link and the "Install update" and
   "Later" buttons are shown directly, without the dialog. Nothing is downloaded until the user clicks
   "Install" or "Install update"; the install then uses the manifest data verified in this step.
4. The MSI URL must start with `https://github.com/tomcek42/Winclipper/releases/download/`. The MSI may be
   at most 64 MB, and its SHA-256 must match the manifest.
5. The MSI is written to `%LOCALAPPDATA%\Winclipper\Winclipper\Update\` and kept open without write or delete
   sharing until msiexec has finished, so it cannot be swapped after the hash check.
6. `msiexec /i … /passive /norestart` runs. Windows asks for elevation, because the package installs per
   machine. The Restart Manager closes Winclipper and restarts it after the upgrade
   (`RegisterApplicationRestart`). If the user cancels the elevation prompt, the update stays available in
   the tray menu.

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
   sources and builds both EXEs and MSIs in the `build` job (read-only token, no secrets). The `release` job then
   signs the manifest and publishes the release with all four files; it runs no build tools and is the only job
   that sees the signing key. Its release notes are the matching `CHANGELOG.md` section plus the MSI hashes.
3. The run pauses before the `release` job ("Review deployments"). Check the build log and the `winclipper`
   artifact (the MSIs), then approve. Nothing is signed or published before that; rejecting the deployment or
   letting it time out leaves no release behind.
4. Running the workflow manually (`workflow_dispatch`) only builds and uploads the MSIs as artifacts.

A manual fallback still works locally: `pwsh tools/sign-update.ps1 -Version 1.6.0.0` (needs `minisign`, asks
for the password).

### The `release` environment

The `release` job runs in a GitHub environment named `release`. It must exist before the next tag is pushed;
without it GitHub creates an unprotected one on the first run. Set it up once under the repository's
Settings > Environments:

1. New environment `release`.
2. **Required reviewers:** add the project owner. Leave "Prevent self-review" off, since the owner both pushes the
   tag and approves; with it on, nobody could approve.
3. **Deployment branches and tags:** "Selected branches and tags", add the tag pattern `v*` (as a tag rule, not a
   branch rule). Other refs then cannot use the environment.
4. **Environment secrets:** `WINCLIPPER_UPDATE_KEY` (the full content of the minisign `.key` file) and
   `WINCLIPPER_UPDATE_KEY_PASSWORD`.
5. Delete the repository secrets of the same names under Settings > Secrets and variables > Actions, so that no
   other job or workflow can read them.

What this protects: a compromised build step, action or dependency cannot sign or publish on its own, and the key
reaches a runner only after approval. What it does not protect: an attacker who controls the owner's GitHub
account can approve as well (use a passkey or security key for 2FA), and the approval does not prove the MSIs are
clean; it only gives a moment to notice something unusual before they are signed.

The workflow pins every action to a commit SHA and minisign to a SHA-256 (`MINISIGN_SHA256`). To update minisign,
download the new release, verify its `.minisig` with the minisign author's public key
`RWQf6LRCGA9i53mlYecO4IzT51TGPpvWucNSCh1CBM0QTaLn73Y7GFO3`, and change `MINISIGN_URL` and `MINISIGN_SHA256`
together.

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
