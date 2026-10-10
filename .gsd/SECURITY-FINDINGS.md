# Winclipper — Security-Review & Backlog

Stand: 2026-09-04. Fork `tomcek42/Winclipper` @ `b8b0e7d` (Release 1.4.2), upstream `Carson-Shook/Winclipper`.
Status: Review der Persistenzschicht abgeschlossen, Rest offen. Noch **nichts committet**.

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
- Repo hat **keine `.gitignore`** — `packages/`, `x64/`, `Debug/`, `Release/` gehoeren hinein.

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

### F6 — `File::JoinPath` mit leerem String ist UB (`Winclipper/File.cpp:17-31`)
`retVal.back()` / `retVal2.front()` auf leerem `std::wstring`. Erreichbar, falls `SHGetKnownFolderPath` je einen
leeren Pfad liefert.

## Noch nicht geprueft

- `MainWindow.cpp` / Fensternachrichten-Handling
- Zwischenablage-Erfassung selbst (Formate, Ownership, Groessenlimits)
- `RegistryUtilities.cpp` (Autostart)
- `UserSettings.cpp`-Parsing (518 Zeilen, dateibasiert, ebenfalls unvalidiert)
- Update-Mechanismus: `SSUPConfig.xml` in beiden Setup-Projekten deutet auf einen hin — Update-Kanaele sind
  klassisch fuer unsignierte Downloads. Hohe Prioritaet.

## Vorgeschlagenes Vorgehen (offen, wartet auf Entscheidung)

1. F3, F4, F6 sofort fixen — klein, risikoarm.
2. F1 als eigener Commit: Haertung des Bitmap-Parsers.
3. F2 und F5 als separate Vorhaben — Designaenderungen.

Offene Fragen an den Nutzer:
- Eigener Branch fuer den `vcxproj`-Pfadfix? (Nicht pushen ohne Freigabe.)
- `.gitignore` anlegen?
- Erst Review fertigmachen, oder parallel schon fixen?
