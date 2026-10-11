# Changelog

Changes in this fork compared to `Carson-Shook/Winclipper` 1.4.2. Details on each finding:
`.gsd/SECURITY-FINDINGS.md`.

## 1.5.1 (2026-10-11)

### Bug fixes
- The `Winclipper.exe` version resource still reported 1.4.2.0 in the 1.5.0 release; it now reports 1.5.0.0.

## 1.5.0 — Security hardening (2026-10-11)

### Security
- **F1** Fixed a heap over-read when loading cached bitmaps; every header field is now checked against the file size.
- **F2** `clips.dat` and `BitmapCache\` are encrypted with DPAPI (bound to the current user). Plaintext files from
  older versions are migrated at startup. Thumbnails are kept in memory only; old `-thumb` BMP files are deleted.
- **F3** `File::Delete` reported success inverted.
- **F4** `File::DeleteDir` was missing the double NUL terminator.
- **F5** Fixed data races in the bitmap cache, the writer threads and `ClipsCollection` (mutex/atomics);
  fixed a use-after-free in the asynchronous bitmap loader.
- **F6** Undefined behavior in `File::JoinPath` with an empty string.
- **F7** An invalid `settings.dat` no longer aborts startup.
- **F8** Removed the original author's auto-updater (unsigned MSI downloads). Own `UpgradeCode`s; upstream
  installations are removed during setup.
- **F9** `CF_DIB` clipboard data is fully validated against the block size (over-read/overflow).
- **F10** `CF_UNICODETEXT` is read bounded by the block size.
- **F11** Right-click "Delete" in the menu deletes the correct clip and no longer crashes.
- **F12** Registry values are read with type and size checks (`RegGetValueW`).
- **F13** Autostart path is quoted (settings and installer); registry return values are checked.
- **F14** Content marked as excluded from clipboard history (e.g. passwords from password managers) is ignored.

### Bug fixes
- The tray icon is removed on exit.

### Build
- Toolset v145, Windows SDK 10.0.26100.0, statically linked CRT (`/MT`); no VC++ redistributable required.
- Replaced the original author's hard-coded include path.
- Installers migrated to WiX 7 (SDK projects, `dotnet build`); removed the VC141 merge modules and the WiX 3.11 package.
- Setup projects registered in `Winclipper.sln` with the WiX SDK project type; a full `.sln` build also builds the MSI.
