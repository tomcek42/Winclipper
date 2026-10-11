# Winclipper — Security-Review & Backlog

Stand: 2026-10-10. Fork `tomcek42/Winclipper`, upstream `Carson-Shook/Winclipper` (keine Merges mehr geplant).
Status: Alle Befunde F1–F14 behoben, alle geplanten Bereiche geprüft. Clips und Bild-Cache sind DPAPI-verschlüsselt.

| Befund | Status | Commit |
|---|---|---|
| F1 Heap-Over-Read Bitmap | behoben | `ac6d8aa` |
| F2 Klartext-Clips | behoben | `b8287b9`, `b1b006b`, `aeff89c` |
| F3 `File::Delete` invertiert | behoben | `3daeee5` |
| F4 `DeleteDir` NUL-Terminierung | behoben | `297178c` |
| F5 Datenrennen | behoben | `29064a4`, `89881b4`, `7139a8a`, `bf88a47` |
| F6 `JoinPath` leerer String | behoben | `25f005a` |
| F7 Absturz bei ungültiger `settings.dat` | behoben | `834a6ca` |
| F8 Fremder Auto-Updater ohne Signaturprüfung | behoben | `69637f4`, `c1bbfb3` |
| F9 Ungeprüfte DIB-Daten aus der Zwischenablage | behoben | `2191642` |
| F10 Unbegrenztes Lesen von `CF_UNICODETEXT` | behoben | `2bc2715` |
| F11 Rechtsklick-Löschen über Menüposition | behoben | `1db2b39` |
| F12 Stack-Overflow beim Lesen von Registry-Werten | behoben | `08187b1` |
| F13 Autostart-Pfad ohne Anführungszeichen | behoben | `ffe49b1` |
| F14 Ausschlussmarkierungen der Zwischenablage ignoriert | behoben | `0370fc7` |

## Build-Setup (verifiziert)

- Toolchain vorhanden: **VS Build Tools 2026** (18.9.12112.369), MSVC 14.51 (`v145`), Windows SDK 10.0.26100.
  Pruefbefehl: `"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -all -format json`
- Projekt zielt auf `v141` + Windows SDK `8.1` (beides nicht installiert). Baut ohne Codeaenderung mit:
  `msbuild Winclipper/Winclipper.sln -p:Configuration=Release -p:Platform=x64 -p:PlatformToolset=v145 -p:WindowsTargetPlatformVersion=10.0.26100.0`
- **Bereits geaendert (uncommitted):** `Winclipper/Winclipper.vcxproj` — hartkodierter Autorenpfad
  `C:\Users\Carson Shook\Projects\Winclipper\Controls` an 4 Stellen ersetzt durch `$(SolutionDir)..\Controls`.
  Echter Upstream-Bug: ausser dem Autor konnte niemand bauen.
- NuGet-Paket WiX 3.11 nicht mehr nötig (`5e8edcb`); Installer bauen per MSBuild mit installiertem WiX 3.14, VC141-Merge-Module entfernt (`799dd20`).
  Setup-Projekte brauchen zusaetzlich die WiX-Toolset-Build-Tools; fuer die reine EXE nicht noetig.

## Angreifermodell

Angriffsflaeche ist `%LOCALAPPDATA%\Winclipper\Winclipper\` (Clips-Datei + `BitmapCache\`).
Jeder Prozess im Benutzerkontext kann diese Dateien manipulieren; Winclipper parst sie beim Start ohne jede Validierung.

## Befunde

### F1 — Heap-Over-Read beim Bitmap-Laden (`Winclipper/Bitmap.cpp:57-115`) — schwerwiegend
```cpp
const size_t bmSize = header.bfSize - header.bfOffBits;   // beide aus der Datei
std::shared_ptr<BYTE> bits(new BYTE[bmSize], array_deleter<BYTE>());
stream.readsome(reinterpret_cast<char *>(bits.get()), bmSize);
```
- Unsigned-Underflow, falls `bfOffBits > bfSize` → `bmSize` nahe 2^64 (DoS) oder gezielt kleiner Puffer.
- Rueckgabewert von `readsome` nie geprueft → Puffer ggf. uninitialisiert.
- Kern: `GetHbitmap()` gibt den Puffer an `CreateDIBitmap`, das seine Lesemenge aus `biSizeImage`/`biWidth`/`biHeight`
  berechnet (`Bitmap::Size()`, ~Zeile 168) — **unabhaengige** Felder derselben Datei. `bfSize-bfOffBits`=16 und
  `biSizeImage`=16 MB liest 16 MB Heap hinter dem Puffer und rendert ihn als Vorschaubild.
- Zusaetzlich `colorBytes` aus `biClrUsed` (DWORD) unbegrenzt → `push_back`-Schleife (~Zeile 84) bis 4 Mrd. `RGBQUAD` (64 GB).

Fix: `bfOffBits <= bfSize` pruefen; `bmSize` gegen echte Dateigroesse **und** gegen `Size()` abgleichen;
`biClrUsed` auf `1 << biBitCount` deckeln; `read()` + `gcount()` statt `readsome()`.

### F2 — Clip-Inhalte effektiv im Klartext (`Winclipper/Clip.cpp:381`)
Base64, keine Verschluesselung. Kopierte Passwoerter landen dauerhaft und trivial dekodierbar in `%LOCALAPPDATA%`.
Vorschlag: `CryptProtectData` (DPAPI, benutzergebunden, keine Schluesselverwaltung) plus Beachtung der
Zwischenablage-Formate `ExcludeClipboardContentFromMonitorProcessing` und `Clipboard Viewer Ignore`,
die Passwort-Manager setzen — Winclipper ignoriert beide derzeit. Designaenderung, kein Patch.

Behoben: `clips.dat` und `BitmapCache\` werden per `File::WriteProtected` mit `CryptProtectData` (Benutzerschlüssel,
feste Zusatz-Entropie, Kennung `WCDPAPI1`) geschrieben. Klartextdateien älterer Versionen werden einmal gelesen und
sofort verschlüsselt neu geschrieben. Die `-thumb`-BMPs (unverschlüsselte Kopie jedes Bildes) entfallen; Vorschauen
entstehen nur noch im Speicher, Altdateien werden gelöscht. Die Ausschlussformate sind seit F14 berücksichtigt.
Grenzen: DPAPI schützt gegen Offline-Zugriff (gestohlene Platte, Backup, anderer Benutzer), nicht gegen Code, der
als derselbe Benutzer läuft — der kann `CryptUnprotectData` selbst aufrufen. Eine wirksame Alternative ohne
Passwortabfrage gibt es dafür nicht; maximaler Schutz ist „Save clips to disk“ aus. `settings.dat` bleibt
unverschlüsselt (keine Inhalte).

### F3 — `File::Delete` gibt bei Erfolg `false` zurueck (`Winclipper/File.cpp:66-72`)
```cpp
if (DeleteFileW(name) == 0) { return true; }   // 0 = FEHLER bei DeleteFileW
return false;
```
Rueckgabe invertiert. Wird aktuell nirgends ausgewertet, ist aber eine Falle — z. B. in
`SaveClipsAsync` (`ClipsManager.cpp:20`), wo beim Abschalten von "Save to Disk" ungeprueft geloescht wird.

### F4 — `File::DeleteDir`: doppelte NUL-Terminierung fehlt (`Winclipper/File.cpp:74-88`)
`dirName += L"\0"` haengt nichts an (Operator liest bis zum ersten NUL). `SHFILEOPSTRUCT.pFrom` verlangt eine
doppelt nullterminierte Liste; `c_str()` liefert nur eine → `SHFileOperation` liest ueber den Puffer hinaus.
Fix: `dirName.push_back(L'\0')`, `pFrom` auf `dirName.data()` mit explizit zweitem NUL.

### F5 — Datenrennen im Bitmap-Cache (`Winclipper/OnDemandBitmapManager.cpp:52-57`)
```cpp
while (inCriticalSection) { Sleep(10); }
inCriticalSection = true;
```
Nicht-atomares `bool` als Spinlock; Test und Setzen nicht atomar. Gleiches Muster fuer `clipsWriterWaitCount`
(`ClipsManager.cpp:5-38`), manipuliert aus einem detachten Thread ohne Synchronisation.
Fix: `std::mutex` bzw. `std::atomic<int>`.

Behoben: Bitmap-Cache, `ClipsManager`-/`UserSettings`-Writer, `Clip::pBitmap` und Lade-Merker. Nebenbei behoben:
Use-after-free durch `c_str()` eines lokalen Strings an detachte `File::Delete`-Threads.
Ebenfalls behoben:
- `Clip::EnsureBitmapAsync` übergab dem detachten Thread rohes `this` → jetzt `shared_from_this()` (`7139a8a`).
- Writer-Thread las `ClipsCollection`, während der UI-Thread sie veränderte → `std::mutex` in `ClipsCollection`,
  Serialisierung und Datei-I/O auf einer Kopie der `shared_ptr`-Liste außerhalb der Sperre (`bf88a47`).

### F6 — `File::JoinPath` mit leerem String ist UB (`Winclipper/File.cpp:17-31`)
`retVal.back()` / `retVal2.front()` auf leerem `std::wstring`. Erreichbar, falls `SHGetKnownFolderPath` je einen
leeren Pfad liefert.

### F7 — Ungültige `settings.dat` bricht jeden Start ab (`Winclipper/UserSettings.cpp`, `Deserialize`)
`stoi`/`stous` auf Dateiwerte ohne `try`; `1|abc` wirft eine ungefangene Exception beim Start. Jetzt wird der
Eintrag übersprungen und der Standardwert behalten. Gültige Zahlen begrenzen die Setter ohnehin auf ihren Bereich.

### F8 — Auto-Updater des Originalautors (`Winclipper/Winclipper.cpp`, `RunUpdater`) — schwerwiegend
Jeder Start rief `StupidSimpleUpdater.exe` (geschlossenes Binary aus `SSUP_merge_*.msm`) auf. Es fragt täglich
`https://www.carsonshook.com/Winclipper/SSUPUpdateManifest.xml` ab (am 2026-10-10 live). Das Manifest enthält nur
Version und Download-URL, keinen Hash und keine Signatur. Wer die Domain kontrolliert (Übernahme, Ablauf,
Kompromittierung), bestimmt die angebotene MSI; die Installation läuft per-machine. Zudem würde der Kanal die
gehärteten Fork-Builds durch Upstream-MSIs ersetzen. Fix: Aufruf entfernt, Merge-Modul und `SSUPConfig.xml` aus
beiden Installern entfernt. Der Fork hat keinen Update-Kanal.

Folgepunkt behoben (`61687cd`): Beide Installer haben einen eigenen `UpgradeCode`, damit keine Upstream-MSI den Fork
per `MajorUpgrade` ersetzt. Die Upstream-Codes werden per `<Upgrade>` erkannt und Upstream-Installationen jeder
Version entfernt, damit nicht beide nebeneinander liegen. `Manufacturer` bleibt unverändert.

### F9 — Ungeprüfte DIB-Daten aus der Zwischenablage (`Winclipper/ClipsManager.cpp`, `AddToClips`) — schwerwiegend
Jeder Prozess der Sitzung kann beliebige Bytes als `CF_DIB` in die Zwischenablage legen; Winclipper übernimmt sie
automatisch. Der Header wurde nicht gegen `GlobalSize` geprüft: `biSizeImage`/`biClrUsed` steuerten ein `memcpy`
über das Blockende hinaus (Heap-Over-Read), bei Bildgröße 0 lief die Alpha-Schleife bis `0xFFFFFFFF`
(Heap-Overflow beim Schreiben), negative Breiten und große Maße ließen die Größenberechnung überlaufen. Fix:
`CopyClipboardDib` prüft jedes Feld gegen die Blockgröße mit denselben Regeln wie `Bitmap::Deserialize`, speichert
nur einen `BITMAPINFOHEADER` (Masken aus V4/V5-Headern werden übernommen) und fängt `bad_alloc` ab.

### F10 — Unbegrenztes Lesen von `CF_UNICODETEXT` (`Winclipper/ClipsManager.cpp`, `AddToClips`) — mittel
Der Text wurde bis zur ersten NUL gelesen. Ein Prozess kann unterminierten Text in die Zwischenablage legen, dann
las Winclipper über das Ende des Blocks hinaus (Over-Read, Absturz oder Fremddaten im Clip). Fix: Länge per
`wcsnlen` auf `GlobalSize / sizeof(wchar_t)` begrenzt.

### F11 — Rechtsklick-Löschen über Menüposition (`Winclipper/MainWindow.cpp`, `WmMenuRButtonUp`) — mittel
`WM_MENURBUTTONUP` liefert die Position des Eintrags, nicht seine ID. Die Position zählt auch „More...“,
Trennlinien, „Clear Clips“ und „Settings“ mit. „Delete“ auf einem solchen Eintrag löschte einen fremden Clip
oder dereferenzierte das `nullptr` aus `RemoveAt` (Absturz, z. B. bei leerer Liste). Fix: Die Item-ID
(Clip-Index + 1) wird per `GetMenuItemID` gelesen und gegen die Listengröße geprüft; `DeleteClipAt` prüft auf `nullptr`.

Rest von `MainWindow.cpp` geprüft, ohne weiteren Sicherheitsbefund. `WM_COMMAND` (z. B. `IDM_EXIT`) kann jeder
Prozess gleicher Integritätsstufe an das versteckte Fenster senden. Das liegt im Angreifermodell ohnehin vor
(gleicher Benutzer), UIPI blockt niedrigere Stufen. Nebenbefund, funktional: `DeleteNotificationIcon` setzt
kein `hWnd`, und `AddNotificationIcon` belegt `uID` mit dem HRESULT von `LoadIconMetric`, sodass das Entfernen
des Tray-Icons beim Beenden fehlschlägt. Behoben in `55397f8`.

### F12 — Stack-Overflow beim Lesen von Registry-Werten (`Winclipper/RegistryUtilities.cpp`) — schwerwiegend
`AppsUseLightTheme`, `ClipboardHistoryEnabled` und `ClipboardStartupWarningDisabled` übergaben `RegQueryValueExW`
`cbData = 4096` für ein 4-Byte-`INT32`, ohne den Typ zu prüfen. Ein HKCU-Wert anderen Typs (z. B. `REG_BINARY`
mit 4 KB) überschrieb den Stack; `ClipboardHistoryEnabled` läuft bei jedem Start. Schreibbar für jeden Prozess
des Benutzers, also wie die Appdata-Dateien zu behandeln. `ReleaseId` (HKLM) wurde ohne garantierte
NUL-Terminierung an `stoi` gegeben. Fix: `RegGetValueW` mit `RRF_RT_REG_DWORD`/`RRF_RT_REG_SZ` (prüft Typ, Größe
und Terminierung). Nebenbei: kein `RegCloseKey` mehr auf ungeöffnete Handles, Schreibschlüssel nur mit
`KEY_SET_VALUE`.

### F13 — Autostart-Pfad ohne Anführungszeichen (`Winclipper/SettingsWindow.cpp`, `WriteRegistryRun`; `Setup-*/Product.wxs`) — mittel
Der Run-Wert enthielt den EXE-Pfad ohne Anführungszeichen. Windows zerlegt ihn wie eine Kommandozeile;
bei `C:\Program Files\Winclipper\Winclipper.exe` wird zuerst `C:\Program.exe` versucht. Ausnutzbar nur, wenn
dort jemand schreiben kann (normal nur Admins), daher mittel. Der Installer schrieb den Wert genauso. Außerdem
wurde ein fehlgeschlagenes `RegOpenKeyExW` ignoriert und `sizeof(pPath)` (520 Byte) statt der echten Länge
geschrieben. Fix: Pfad in Anführungszeichen (Code und beide `.wxs`), Rückgabewerte geprüft, minimale
Zugriffsrechte. Bestehende Run-Werte werden erst beim nächsten Umschalten in den Einstellungen bzw. bei einer
Neuinstallation korrigiert.

### F14 — Ausschlussmarkierungen der Zwischenablage ignoriert (`Winclipper/ClipsManager.cpp`, `AddToClips`) — schwerwiegend
Passwortmanager markieren kopierte Geheimnisse mit `ExcludeClipboardContentFromMonitorProcessing`,
`Clipboard Viewer Ignore` oder `CanIncludeInClipboardHistory = 0`. Winclipper prüfte das nicht und speicherte
Passwörter dauerhaft in `clips.dat`, im Klartext (F2). Fix: `ClipboardContentExcluded` prüft die drei Formate
bei geöffneter Zwischenablage und überspringt markierte Inhalte. Ein unlesbarer `CanIncludeInClipboardHistory`-
Wert gilt als Ausschluss.

Ownership sonst ohne Sicherheitsbefund: `ClipLock` (nicht-atomares `bool`, nur UI-Thread) unterdrückt das
`WM_CLIPBOARDUPDATE` nach eigenem Setzen. Scheitern alle Versuche in `SetClipboardToClipAtIndex`, bleibt es
gesetzt und die nächste echte Kopie wird verschluckt (funktional, gering).

## Nächste Schritte

1. Upgrade von einer Upstream-Installation auf das Fork-MSI testen (braucht Adminrechte).

## Laufzeittests (2026-10-10)

- Migration (x64 Release): Klartext-`clips.dat` mit Text- und Bildclips plus verwaisten Upstream-Bildern und
  `-thumb`-Dateien. Erster Test fand eine Lücke: Bilddateien wurden erst beim Laden verschlüsselt, verwaiste nie.
  Behoben mit `aeff89c` (`OnDemandBitmapManager::ProtectDiskCache` beim Start). Danach beginnen alle Dateien mit
  `WCDPAPI1`, keine `-thumb`-Dateien, alte Clips bleiben erhalten, neue Text-/Bildkopien landen verschlüsselt.
  Grenze: Überschreiben an Ort und Stelle löscht alte Klartextblöcke auf der Platte nicht sicher.
- Installer: Beide MSIs mit `candle`/`light` aus dem WiX-3.11-NuGet-Paket gebaut. Upgrade-Tabelle geprüft:
  eigener `UpgradeCode`, Upstream-Code (x64 `34e3392e…`, x86 `a332e06b…`) wird entfernt, Run-Wert in Anführungszeichen.
