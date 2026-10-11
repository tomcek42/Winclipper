# Winclipper — Security Review & Backlog

As of: 2026-10-11. Fork `tomcek42/Winclipper`, upstream `Carson-Shook/Winclipper` (no further merges planned).
Status: all findings F1–F14 fixed, all planned areas reviewed. Clips and the image cache are DPAPI-encrypted.
Released as 1.5.0.

| Finding | Status | Commit |
|---|---|---|
| F1 Heap over-read in bitmap loading | fixed | `ac6d8aa` |
| F2 Plaintext clips | fixed | `b8287b9`, `b1b006b`, `aeff89c` |
| F3 `File::Delete` inverted | fixed | `3daeee5` |
| F4 `DeleteDir` NUL termination | fixed | `297178c` |
| F5 Data races | fixed | `29064a4`, `89881b4`, `7139a8a`, `bf88a47` |
| F6 `JoinPath` with empty string | fixed | `25f005a` |
| F7 Crash on invalid `settings.dat` | fixed | `834a6ca` |
| F8 Third-party auto-updater without signature checks | fixed | `69637f4`, `c1bbfb3`, `61687cd` |
| F9 Unchecked DIB data from the clipboard | fixed | `2191642` |
| F10 Unbounded read of `CF_UNICODETEXT` | fixed | `2bc2715` |
| F11 Right-click delete by menu position | fixed | `1db2b39` |
| F12 Stack overflow when reading registry values | fixed | `08187b1` |
| F13 Unquoted autostart path | fixed | `ffe49b1` |
| F14 Clipboard exclusion markers ignored | fixed | `0370fc7` |

## Build setup

- Toolchain: **VS Build Tools 2026**, MSVC 14.51 (`v145`), Windows SDK 10.0.26100. Both projects are pinned to it
  and link the CRT statically (`7f09f69`). Upstream targeted `v141` + SDK `8.1`.
- Upstream's `Winclipper.vcxproj` contained the author's hard-coded path
  `C:\Users\Carson Shook\Projects\Winclipper\Controls` in four places, so nobody but the author could build it.
  Replaced with `$(SolutionDir)..\Controls` (`8a6ed15`).
- The WiX 3.11 NuGet package is no longer required (`5e8edcb`), the VC141 merge modules are removed (`799dd20`).
  The installers are WiX 7 SDK projects (`5fb77ac`) and need no installed toolset. Build commands: `CLAUDE.md`.

## Threat model

The attack surface is `%LOCALAPPDATA%\Winclipper\Winclipper\` (clips file + `BitmapCache\`), the clipboard and
HKCU registry values. Any process in the user's context can modify the files; upstream Winclipper parsed them at
startup without any validation. Any process in the session can put arbitrary data on the clipboard.

## Findings

### F1 — Heap over-read in bitmap loading (`Winclipper/Bitmap.cpp:57-115`) — severe
```cpp
const size_t bmSize = header.bfSize - header.bfOffBits;   // both from the file
std::shared_ptr<BYTE> bits(new BYTE[bmSize], array_deleter<BYTE>());
stream.readsome(reinterpret_cast<char *>(bits.get()), bmSize);
```
- Unsigned underflow if `bfOffBits > bfSize` → `bmSize` close to 2^64 (DoS) or a deliberately small buffer.
- The return value of `readsome` was never checked → the buffer may be uninitialized.
- Core issue: `GetHbitmap()` passes the buffer to `CreateDIBitmap`, which derives its read size from
  `biSizeImage`/`biWidth`/`biHeight` (`Bitmap::Size()`, ~line 168) — **independent** fields of the same file.
  `bfSize-bfOffBits` = 16 and `biSizeImage` = 16 MB reads 16 MB of heap behind the buffer and renders it as a
  thumbnail.
- In addition, `colorBytes` from `biClrUsed` (DWORD) was unbounded → `push_back` loop (~line 84) up to 4 billion
  `RGBQUAD`s (64 GB).

Fix: check `bfOffBits <= bfSize`; validate `bmSize` against the real file size **and** against `Size()`;
cap `biClrUsed` at `1 << biBitCount`; `read()` + `gcount()` instead of `readsome()`.

### F2 — Clip contents effectively in plaintext (`Winclipper/Clip.cpp:381`)
Base64, no encryption. Copied passwords ended up permanently and trivially decodable in `%LOCALAPPDATA%`.

Fixed: `clips.dat` and `BitmapCache\` are written via `File::WriteProtected` with `CryptProtectData` (user key,
fixed additional entropy, marker `WCDPAPI1`). Plaintext files from older versions are read once and immediately
rewritten encrypted; at startup `OnDemandBitmapManager::ProtectDiskCache` also encrypts orphaned cache files.
The `-thumb` BMPs (an unencrypted copy of every image) are gone; thumbnails exist only in memory and old files are
deleted. The exclusion formats are honored since F14.
Limits: DPAPI protects against offline access (stolen disk, backup, another user), not against code running as the
same user — that code can call `CryptUnprotectData` itself. There is no effective alternative without a password
prompt; maximum protection is turning "Save clips to disk" off. `settings.dat` stays unencrypted (no content).
Rewriting files in place does not securely erase old plaintext blocks on disk.

### F3 — `File::Delete` returns `false` on success (`Winclipper/File.cpp:66-72`)
```cpp
if (DeleteFileW(name) == 0) { return true; }   // 0 = FAILURE for DeleteFileW
return false;
```
Return value inverted. Not evaluated anywhere at the time, but a trap — e.g. in `SaveClipsAsync`
(`ClipsManager.cpp:20`), which deletes unchecked when "Save to Disk" is turned off.

### F4 — `File::DeleteDir`: missing double NUL termination (`Winclipper/File.cpp:74-88`)
`dirName += L"\0"` appends nothing (the operator reads up to the first NUL). `SHFILEOPSTRUCT.pFrom` requires a
double-NUL-terminated list; `c_str()` provides only one → `SHFileOperation` reads past the buffer.
Fix: `dirName.push_back(L'\0')`.

### F5 — Data races in the bitmap cache (`Winclipper/OnDemandBitmapManager.cpp:52-57`)
```cpp
while (inCriticalSection) { Sleep(10); }
inCriticalSection = true;
```
A non-atomic `bool` used as a spinlock; test and set are not atomic. Same pattern for `clipsWriterWaitCount`
(`ClipsManager.cpp:5-38`), modified from a detached thread without synchronization.

Fixed: bitmap cache, `ClipsManager`/`UserSettings` writers, `Clip::pBitmap` and loading flags (`std::mutex`,
`std::atomic`). Also fixed: a use-after-free from passing `c_str()` of a local string to detached `File::Delete`
threads.
- `Clip::EnsureBitmapAsync` passed a raw `this` to the detached thread → now `shared_from_this()` (`7139a8a`).
- The writer thread read `ClipsCollection` while the UI thread modified it → `std::mutex` in `ClipsCollection`,
  serialization and file I/O work on a copy of the `shared_ptr` list outside the lock (`bf88a47`).

### F6 — `File::JoinPath` with an empty string is UB (`Winclipper/File.cpp:17-31`)
`retVal.back()` / `retVal2.front()` on an empty `std::wstring`. Reachable if `SHGetKnownFolderPath` ever returns
an empty path.

### F7 — Invalid `settings.dat` aborts every startup (`Winclipper/UserSettings.cpp`, `Deserialize`)
`stoi`/`stous` on file values without `try`; `1|abc` threw an uncaught exception at startup. Now the entry is
skipped and the default is kept. The setters clamp valid numbers to their range anyway.

### F8 — The original author's auto-updater (`Winclipper/Winclipper.cpp`, `RunUpdater`) — severe
Every start launched `StupidSimpleUpdater.exe` (closed-source binary from `SSUP_merge_*.msm`). It polls
`https://www.carsonshook.com/Winclipper/SSUPUpdateManifest.xml` daily (live on 2026-10-10). The manifest contains
only a version and a download URL, no hash and no signature. Whoever controls the domain (takeover, expiry,
compromise) decides which MSI is offered; it installs per-machine. The channel would also replace the hardened fork
builds with upstream MSIs. Fix: call removed, merge module and `SSUPConfig.xml` removed from both installers.
The fork has no update channel.

Follow-up fixed (`61687cd`): both installers have their own `UpgradeCode`, so no upstream MSI replaces the fork via
`MajorUpgrade`. The upstream codes are detected via `<Upgrade>`, and upstream installations of any version are
removed so the two do not coexist. `Manufacturer` is unchanged.

### F9 — Unchecked DIB data from the clipboard (`Winclipper/ClipsManager.cpp`, `AddToClips`) — severe
Any process in the session can put arbitrary bytes on the clipboard as `CF_DIB`; Winclipper picks them up
automatically. The header was not checked against `GlobalSize`: `biSizeImage`/`biClrUsed` drove a `memcpy` past
the end of the block (heap over-read), with an image size of 0 the alpha loop ran up to `0xFFFFFFFF` (heap
overflow on write), and negative widths and large dimensions overflowed the size calculation. Fix:
`CopyClipboardDib` checks every field against the block size using the same rules as `Bitmap::Deserialize`, stores
only a `BITMAPINFOHEADER` (masks from V4/V5 headers are carried over) and catches `bad_alloc`.

### F10 — Unbounded read of `CF_UNICODETEXT` (`Winclipper/ClipsManager.cpp`, `AddToClips`) — medium
The text was read up to the first NUL. A process can put unterminated text on the clipboard, and Winclipper then
read past the end of the block (over-read, crash or foreign data in the clip). Fix: length bounded with `wcsnlen`
to `GlobalSize / sizeof(wchar_t)`.

### F11 — Right-click delete by menu position (`Winclipper/MainWindow.cpp`, `WmMenuRButtonUp`) — medium
`WM_MENURBUTTONUP` delivers the item's position, not its ID. The position also counts "More...", separators,
"Clear Clips" and "Settings". "Delete" on such an item deleted an unrelated clip or dereferenced the `nullptr`
returned by `RemoveAt` (crash, e.g. with an empty list). Fix: the item ID (clip index + 1) is read with
`GetMenuItemID` and checked against the list size; `DeleteClipAt` checks for `nullptr`.

The rest of `MainWindow.cpp` was reviewed without further security findings. Any process at the same integrity level
can send `WM_COMMAND` (e.g. `IDM_EXIT`) to the hidden window. That is already covered by the threat model (same
user); UIPI blocks lower levels. Functional side finding: `DeleteNotificationIcon` set no `hWnd`, and
`AddNotificationIcon` assigned the HRESULT of `LoadIconMetric` to `uID`, so removing the tray icon on exit failed.
Fixed in `55397f8`.

### F12 — Stack overflow when reading registry values (`Winclipper/RegistryUtilities.cpp`) — severe
`AppsUseLightTheme`, `ClipboardHistoryEnabled` and `ClipboardStartupWarningDisabled` passed `cbData = 4096` to
`RegQueryValueExW` for a 4-byte `INT32` without checking the type. An HKCU value of another type (e.g. a 4 KB
`REG_BINARY`) overwrote the stack; `ClipboardHistoryEnabled` runs on every start. Writable by any process of the
user, so to be treated like the appdata files. `ReleaseId` (HKLM) was passed to `stoi` without guaranteed NUL
termination. Fix: `RegGetValueW` with `RRF_RT_REG_DWORD`/`RRF_RT_REG_SZ` (checks type, size and termination).
Also: no more `RegCloseKey` on unopened handles, write keys opened with `KEY_SET_VALUE` only.

### F13 — Unquoted autostart path (`Winclipper/SettingsWindow.cpp`, `WriteRegistryRun`; `Setup-*/Product.wxs`) — medium
The Run value contained the EXE path without quotes. Windows parses it like a command line; for
`C:\Program Files\Winclipper\Winclipper.exe` it tries `C:\Program.exe` first. Exploitable only if someone can write
there (normally only admins), hence medium. The installer wrote the value the same way. In addition, a failed
`RegOpenKeyExW` was ignored and `sizeof(pPath)` (520 bytes) was written instead of the real length. Fix: quoted
path (code and both `.wxs`), return values checked, minimal access rights. Existing Run values are only corrected
the next time the setting is toggled or on reinstall.

### F14 — Clipboard exclusion markers ignored (`Winclipper/ClipsManager.cpp`, `AddToClips`) — severe
Password managers mark copied secrets with `ExcludeClipboardContentFromMonitorProcessing`,
`Clipboard Viewer Ignore` or `CanIncludeInClipboardHistory = 0`. Winclipper did not check this and stored passwords
permanently in `clips.dat`, in plaintext (F2). Fix: `ClipboardContentExcluded` checks the three formats while the
clipboard is open and skips marked content. An unreadable `CanIncludeInClipboardHistory` value counts as excluded.

Clipboard ownership otherwise without security findings: `ClipLock` (non-atomic `bool`, UI thread only) suppresses
the `WM_CLIPBOARDUPDATE` after Winclipper sets the clipboard itself. If all attempts in `SetClipboardToClipAtIndex`
fail, it stays set and the next real copy is swallowed (functional, minor).

## Runtime tests (2026-10-10/11)

- Migration (x64 Release): plaintext `clips.dat` with text and image clips plus orphaned upstream images and
  `-thumb` files. The first test found a gap: image files were only encrypted when loaded, orphaned ones never.
  Fixed with `aeff89c` (`OnDemandBitmapManager::ProtectDiskCache` at startup). Afterwards all files start with
  `WCDPAPI1`, no `-thumb` files remain, old clips are kept, and new text/image copies are stored encrypted.
- Installer: upgrade from an upstream installation tested by the project owner — only one Winclipper installation
  remains. The WiX 7 MSIs were installed over the existing version; "Launch Winclipper" works. MSI tables checked:
  own `UpgradeCode`, upstream code (x64 `34e3392e…`, x86 `a332e06b…`) is removed, Run value quoted.
