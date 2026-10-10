# CLAUDE.md — Winclipper

Fork von `Carson-Shook/Winclipper` (Win32-Clipboard-Manager, C++14, MSVC).
Ziel dieses Forks: Sicherheitsprüfung und Härtung. Befunde und Backlog: `.gsd/SECURITY-FINDINGS.md`.

## Bauen

Fork ohne geplante Upstream-Merges. Beide Projekte (`Winclipper`, `Controls`) sind fest auf
**PlatformToolset v145 + Windows SDK 10.0.26100.0** umgestellt und linken die CRT **statisch**
(`/MT`, Debug `/MTd`) — die EXE braucht keine VC++-Redistributable. `RuntimeLibrary` muss in
beiden Projekten übereinstimmen, sonst scheitert der Linker.

```
msbuild Winclipper/Winclipper.sln -t:Winclipper -p:Configuration=Release -p:Platform=x64
```

`msbuild` liegt nicht im PATH: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`.

`Winclipper.vcxproj` erzwingt das NuGet-Paket **WiX 3.11** (`packages\WiX.3.11.0\build\wix.props`)
und bricht ohne es ab — auch wenn nur die EXE gebaut wird. Das Paket liegt entpackt unter
`Winclipper/packages/` und ist per `.gitignore` ausgeschlossen; nach einem frischen Klon muss es
wiederhergestellt werden. Die Setup-Projekte (`Setup-x64`, `Setup-x86`) brauchen zusätzlich die
installierten WiX-Toolset-Build-Tools und lassen sich sonst gar nicht bauen. Sie binden noch die
VC141-CRT-Merge-Module ein, die die EXE seit `/MT` nicht mehr braucht.

Der Auto-Updater des Originalautors (SSUP) ist bewusst entfernt (F8): Er lud MSIs ohne Signaturprüfung von
dessen Domain. Nicht wieder einbauen; ein künftiger Update-Kanal braucht signierte Pakete.

Installierte Toolsets prüfen:
`"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -all -format json`

## Fallstricke im Bestand

- Upstream hatte in `Winclipper.vcxproj` an vier Stellen einen **hartkodierten absoluten
  Include-Pfad** des Originalautors. Ersetzt durch `$(SolutionDir)..\Controls`.
- Der `Controls`-Unterprojekt-Header wird nur über diesen Include-Pfad gefunden; `Controls.h`
  liegt nicht im Winclipper-Verzeichnis.

## Sicherheitsrelevante Konventionen für Änderungen

Alles unter `%LOCALAPPDATA%\Winclipper\Winclipper\` (Clips-Datei, `BitmapCache\`) ist
**nicht vertrauenswürdig** — jeder Prozess im Benutzerkontext kann es manipulieren, und
Winclipper parst es beim Start. Deserialisierungscode (`Bitmap.cpp`, `Clip.cpp`,
`ClipsCollection.cpp`, `UserSettings.cpp`) muss jedes Feld aus der Datei validieren, bevor es
für Größenberechnungen oder Allokationen benutzt wird. Insbesondere: keine unsignierte
Subtraktion zweier Dateifelder ohne vorherige Ordnungsprüfung, und Rückgabewerte von
`read`/`readsome` immer auswerten.

Clip-Inhalte sind derzeit nur base64-kodiert, also im Klartext. Neue Persistenz nicht ohne
`CryptProtectData` (DPAPI) ergänzen.

## Arbeitsweise in diesem Repo

Nichts pushen ohne ausdrückliche Freigabe. Fixes einzeln committen, damit jeder
Sicherheitsfix für sich prüfbar und per `git revert` rückgängig zu machen ist.
