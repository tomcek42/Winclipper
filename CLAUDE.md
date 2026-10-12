# CLAUDE.md — Winclipper

Fork of `Carson-Shook/Winclipper` (Win32 clipboard manager, C++14, MSVC).
Goal of this fork: security review and hardening. Findings and backlog: `.gsd/SECURITY-FINDINGS.md`.

## Building

Fork without planned upstream merges. Both projects (`Winclipper`, `Controls`) are pinned to
**PlatformToolset v145 + Windows SDK 10.0.26100.0** and link the CRT **statically**
(`/MT`, Debug `/MTd`) — the EXE needs no VC++ redistributable. `RuntimeLibrary` must match in
both projects, otherwise the linker fails.

```
msbuild Winclipper/Winclipper.sln -t:Winclipper -p:Configuration=Release -p:Platform=x64
```

`msbuild` is not on the PATH: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`.

The installers are **WiX 7** (SDK projects, `WixToolset.Sdk` via NuGet, no installation needed). They do **not**
build the EXE: first build the EXE for the platform with msbuild (x86: `-p:Platform=x86`, output in
`Winclipper/Release/`), then run `dotnet build -c Release` in `Setup-<arch>`
(MSI under `Setup-<arch>/bin/<arch>/Release/`). Acceptance of the OSMF EULA is set as `AcceptEula` in the `.wixproj`
(granted by the project owner); without it WiX 7 fails with WIX7015. ICE69 is deliberately suppressed (harmless).
The setup entries in `Winclipper.sln` use the WiX SDK project type; a full `.sln` build (without `-t:`) also builds the
platform's MSI. The VC141 CRT merge modules were removed because the EXE links the CRT statically.

The version number lives in three places: `Winclipper/Winclipper.rc` (`VERSIONINFO`) and the `Version` attribute in
both `Setup-*/Product.wxs`. Bump them together.

The original author's auto-updater (SSUP) was removed on purpose (F8): it downloaded MSIs from the author's domain
without signature checks. Do not add it back. The fork's own updater (`Updater.cpp`, `docs/UPDATER.md`) only installs
MSIs pinned by a manifest signed with the release key; never relax the signature, downgrade or URL-prefix checks.
Every release that should reach auto-updating users needs `tools/sign-update.ps1` and its two assets.
Signatures are minisign/Ed25519 (same scheme as the Tauri updater), verified with the vendored, unmodified
Monocypher in `Winclipper/monocypher/` (C files, precompiled headers off). The secret key is a
password-protected minisign file, deliberately not DPAPI, so it survives a Windows profile change.

Check installed toolsets:
`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -all -format json`

## Pitfalls in the existing code

- Upstream had a **hard-coded absolute include path** of the original author in four places in
  `Winclipper.vcxproj`. Replaced with `$(SolutionDir)..\Controls`.
- The `Controls` subproject header is only found through this include path; `Controls.h`
  is not in the Winclipper directory.
- `Winclipper.rc` is UTF-16 LE. Edit it with an encoding-aware tool, not with sed.

## Security conventions for changes

Everything under `%LOCALAPPDATA%\Winclipper\Winclipper\` (clips file, `BitmapCache\`) is
**untrusted** — any process in the user's context can modify it, and Winclipper parses it at startup.
Deserialization code (`Bitmap.cpp`, `Clip.cpp`, `ClipsCollection.cpp`, `UserSettings.cpp`) must validate every
field from the file before using it for size calculations or allocations. In particular: no unsigned
subtraction of two file fields without checking their order first, and always check the return values of
`read`/`readsome`. The same applies to clipboard data and HKCU registry values.

Clip contents (`clips.dat`, `BitmapCache\`) are encrypted with DPAPI via `File::WriteProtected`/`ReadProtected`.
Persist new clip content only through these, never with `File::Write`. Thumbnails are deliberately never written
to disk (F2).

The installer has its own `UpgradeCode` and removes upstream installations via `<Upgrade>` on the old codes.
Do not bring back the upstream code.

## Working in this repo

Do not push without explicit approval. Commit fixes one by one so that each security fix can be reviewed on its own
and reverted with `git revert`. Everything published on GitHub (docs, changelog, commit messages, release notes)
is written in English.
