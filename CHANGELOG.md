# Changelog

Changes in this fork compared to `Carson-Shook/Winclipper` 1.4.2. Details on each finding:
`.gsd/SECURITY-FINDINGS.md`.

## 1.6.3 (2026-10-29)

### Fixes
- The list of programs whose private content is recorded anyway never matched programs that put content on the
  clipboard without an owner window, such as 1Password. Such content is now attributed to the program in the
  foreground.

### New
- "Never record clips from these programs": a second program list in Settings > Clips. Clips from these programs
  are never recorded, whether marked as private or not.
- Option "Record clips from RDP sessions (only hidden from Windows history)", off by default. RDP marks every clip
  as hidden from the Windows clipboard history; with the option on, this marker alone no longer makes a clip
  private. Content marked with `ExcludeClipboardContentFromMonitorProcessing` (password managers) is still skipped.
- Diagnostics: every clipboard update and the reason a clip was not recorded are written to the debug output
  (view with DebugView, prefix `Winclipper clipboard:`). Clip contents are never logged.

### Changes
- "Check now" shows its result below the button instead of a message box. If an update is available, an
  "Install update..." button opens the confirmation dialog directly.
- Clearer labels for the program lists ("Always record" / "Never record"), more spacing on the Clips page and an
  info tooltip for "Save clips to disk".

## 1.6.2 (2026-10-13)

### Fixes
- Installing an update failed (msiexec error 1619) because Winclipper still held the downloaded MSI open for
  writing. The file is now reopened read-only while staying locked against changes. 1.6.0 and 1.6.1 cannot update
  themselves; install 1.6.2 manually.
- The clips menu could stall or stop reacting to clicks: no shared input queue while the menu is open, no endless
  window walk, no second menu on top of an open one, and Winclipper takes the foreground before showing the menu.
- Menus are freed after use (handle leak).

### Changes
- Refreshed Settings window: owner-drawn sidebar, page headings, rounded corners and dark title bar.
- "About" is no longer in the tray menu; it is a tab in Settings.

## 1.6.1 (2026-10-12)

### Changes
- Settings are organized in vertical tabs (General, Clips, Images, Updates, About). The File/Help menu bar is gone;
  "Clear clips", "Recreate thumbnails" and "Check now" are buttons. The tray menu has "About" below "Settings".
- Updates are no longer installed automatically. "Notify me about updates" shows a notification and a blinking
  tray icon; the update is installed only after confirming it (notification click or "Install update …" in the
  tray menu). The checks (signature, hash, no downgrade) are unchanged.
- Content that programs mark as private is kept if the program is on the allow list (1Password by default).
- The About box shows the real version and the fork's copyright next to the original one.

## 1.6.0 (2026-10-12)

### Features
- Optional automatic updates ("Install updates automatically" in Settings, off by default). Updates come from
  this fork's GitHub releases and are only installed if the manifest carries a valid minisign (Ed25519) signature from
  the release signing key and the MSI hash matches; downgrades are refused. The tray icon blinks while an update installs.
  Details: `docs/UPDATER.md`.

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
