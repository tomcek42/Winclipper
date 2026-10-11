# Changelog

Änderungen dieses Forks gegenüber `Carson-Shook/Winclipper` 1.4.2. Details zu jedem Befund:
`.gsd/SECURITY-FINDINGS.md`.

## 1.5.0 — Sicherheitshärtung (2026-10-11)

### Sicherheit
- **F1** Heap-Over-Read beim Laden gecachter Bitmaps behoben; alle Header-Felder werden gegen die Dateigröße geprüft.
- **F2** `clips.dat` und `BitmapCache\` werden mit DPAPI (benutzergebunden) verschlüsselt. Klartextdateien älterer
  Versionen werden beim Start migriert. Vorschaubilder liegen nur noch im Speicher, alte `-thumb`-BMPs werden gelöscht.
- **F3** `File::Delete` meldete Erfolg invertiert.
- **F4** `File::DeleteDir` ohne doppelte NUL-Terminierung.
- **F5** Datenrennen im Bitmap-Cache, in den Writer-Threads und in `ClipsCollection` behoben (Mutex/atomics);
  Use-after-free im asynchronen Bitmap-Lader behoben.
- **F6** Undefiniertes Verhalten in `File::JoinPath` bei leerem String.
- **F7** Ungültige `settings.dat` bricht den Start nicht mehr ab.
- **F8** Auto-Updater des Originalautors (unsignierte MSI-Downloads) entfernt. Eigene `UpgradeCode`s; Upstream-
  Installationen werden beim Installieren entfernt.
- **F9** `CF_DIB`-Daten aus der Zwischenablage werden vollständig gegen die Blockgröße validiert (Over-Read/Overflow).
- **F10** `CF_UNICODETEXT` wird auf die Blockgröße begrenzt gelesen.
- **F11** Rechtsklick „Delete“ im Menü löscht den richtigen Clip und stürzt nicht mehr ab.
- **F12** Registry-Werte werden typ- und größengeprüft gelesen (`RegGetValueW`).
- **F13** Autostart-Pfad in Anführungszeichen (Einstellungen und Installer); Registry-Rückgabewerte geprüft.
- **F14** Als „nicht in den Verlauf“ markierte Inhalte (z. B. Passwörter aus Passwortmanagern) werden ignoriert.

### Fehlerbehebungen
- Tray-Icon wird beim Beenden entfernt.

### Build
- Toolset v145, Windows SDK 10.0.26100.0, statisch gelinkte CRT (`/MT`); keine VC++-Redistributable nötig.
- Hartkodierter Include-Pfad des Originalautors ersetzt.
- Installer auf WiX 7 (SDK-Projekte, `dotnet build`) umgestellt; VC141-Merge-Module und WiX-3.11-Paket entfernt.
- Setup-Projekte in `Winclipper.sln` mit WiX-SDK-Projekttyp; voller `.sln`-Build baut die MSI mit.
