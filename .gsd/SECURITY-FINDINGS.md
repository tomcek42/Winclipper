# Winclipper — Security-Review & Backlog

Stand: 2026-10-10. Fork `tomcek42/Winclipper`, upstream `Carson-Shook/Winclipper` (keine Merges mehr geplant).
Status: Persistenzschicht gehärtet, Auto-Updater entfernt (F1, F3–F9 behoben). Offen: F2, ungeprüfte Bereiche unten.

| Befund | Status | Commit |
|---|---|---|
| F1 Heap-Over-Read Bitmap | behoben | `ac6d8aa` |
| F2 Klartext-Clips | offen (Designentscheidung, M002) | — |
| F3 `File::Delete` invertiert | behoben | `3daeee5` |
| F4 `DeleteDir` NUL-Terminierung | behoben | `297178c` |
| F5 Datenrennen | behoben | `29064a4`, `89881b4`, `7139a8a`, `bf88a47` |
| F6 `JoinPath` leerer String | behoben | `25f005a` |
| F7 Absturz bei ungültiger `settings.dat` | behoben | `834a6ca` |
| F8 Fremder Auto-Updater ohne Signaturprüfung | behoben | `69637f4`, `c1bbfb3` |
| F9 Ungeprüfte DIB-Daten aus der Zwischenablage | behoben | `2191642` |

## Build-Setup (verifiziert)

- Toolchain vorhanden: **VS Build Tools 2026** (18.9.12112.369), MSVC 14.51 (`v145`), Windows SDK 10.0.26100.
  Pruefbefehl: `"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -all -format json`
- Projekt zielt auf `v141` + Windows SDK `8.1` (beides nicht installiert). Baut ohne Codeaenderung mit:
  `msbuild Winclipper/Winclipper.sln -p:Configuration=Release -p:Platform=x64 -p:PlatformToolset=v145 -p:WindowsTargetPlatformVersion=10.0.26100.0`
- **Bereits geaendert (uncommitted):** `Winclipper/Winclipper.vcxproj` — hartkodierter Autorenpfad
  `C:\Users\Carson Shook\Projects\Winclipper\Controls` an 4 Stellen ersetzt durch `$(SolutionDir)..\Controls`.
  Echter Upstream-Bug: ausser dem Autor konnte niemand bauen.
- NuGet-Paket **WiX 3.11** wird von `Winclipper.vcxproj` erzwungen; nach `Winclipper/packages/WiX.3.11.0/` entpackt.
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

Offen (Folgepunkt): Installer verwendet weiter `UpgradeCode` und `Manufacturer` von Upstream. Eine Upstream-MSI
gleicher oder höherer Version ersetzt den Fork per `MajorUpgrade`. Eigener `UpgradeCode` wäre sauberer, ist
aber eine Produktentscheidung.

### F9 — Ungeprüfte DIB-Daten aus der Zwischenablage (`Winclipper/ClipsManager.cpp`, `AddToClips`) — schwerwiegend
Jeder Prozess der Sitzung kann beliebige Bytes als `CF_DIB` in die Zwischenablage legen; Winclipper übernimmt sie
automatisch. Der Header wurde nicht gegen `GlobalSize` geprüft: `biSizeImage`/`biClrUsed` steuerten ein `memcpy`
über das Blockende hinaus (Heap-Over-Read), bei Bildgröße 0 lief die Alpha-Schleife bis `0xFFFFFFFF`
(Heap-Overflow beim Schreiben), negative Breiten und große Maße ließen die Größenberechnung überlaufen. Fix:
`CopyClipboardDib` prüft jedes Feld gegen die Blockgröße mit denselben Regeln wie `Bitmap::Deserialize`, speichert
nur einen `BITMAPINFOHEADER` (Masken aus V4/V5-Headern werden übernommen) und fängt `bad_alloc` ab.

Offen (Folgepunkt): Der `CF_UNICODETEXT`-Pfad liest den Text per `GlobalLock` ohne Längengrenze; fehlt die
NUL-Terminierung, liest er über den Block hinaus.

## Noch nicht geprueft

- `MainWindow.cpp` / Fensternachrichten-Handling
- Zwischenablage-Erfassung: Textpfad (siehe F9), Ownership
- `RegistryUtilities.cpp` (Autostart)

## Nächste Schritte

1. F9-Folgepunkt: `CF_UNICODETEXT` auf `GlobalSize` begrenzen.
2. F2 (DPAPI) nach Designentscheidung.
