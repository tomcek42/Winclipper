# Forensik 2026-10-10 — überholte Entscheidungen im Memory, S01 steht seit 13.09.

## Symptom

Kein Absturz. Beobachtet:
- `STATE.md`: `**Phase:** evaluating-gates` / „Evaluate 2 quality gate(s) for S01 before execution." — unverändert seit 2026-09-13.
- Das in die Sitzung injizierte Projekt-Memory enthält gleichzeitig
  „Use retargeting instead of permanent project edits" (D004) **und**
  „Pin project files to current toolset with static CRT" (D006), sowie
  „Keep the Controls include path under active review during upstream merges" (D005) **und**
  „Controls include path is project-relative … no recurring merge check needed" (D007).

## Evidence Trail

1. `.gsd/auto.lock` — nicht vorhanden; `.gsd/activity/`, `.gsd/journal/`, `.gsd/metrics.json` — nicht vorhanden. Auto-Mode ist in diesem Projekt nie gelaufen; es gibt keinen Crash und keinen Stuck-Loop.
2. `.gsd/event-log.jsonl:1-8` / `.gsd/audit/events.jsonl` — nur `plan-milestone`/`plan-slice` (M001, S01–S04, M002, M003, M002/S01), alle Session `e97f225c…`, 2026-09-13 14:31–14:32. Kein Gate-, Task- oder Execute-Event danach.
3. `.gsd/state.json` `next.command = "/gsd auto"`, `updated_at = 2026-09-13T14:32:54Z`; `.gsd/runtime/write-gate-state.json` `pendingGateId: null`, `verifiedApprovalGates: []`.
4. `.gsd/DECISIONS.md:12-15` — D006 „(amends D004)", D007 „(amends D005)": die Ablösung steht nur im Titeltext. D004/D005 sind nicht als superseded markiert und bleiben aktiv.
5. `git log` — `7f09f69` (v145/statische CRT fest eingetragen) und `8a6ed15` (Include-Pfad) setzen D006/D007 um; D004 („without permanently changing project files") widerspricht dem Ist-Stand.
6. `.gsd/phases/0{1,2,3}-*/` (gerendert, in `.compat.json` geführt) vs. `phases/M00{1,2,3}-*/` im Repo-Root (getrackt, `abe88f0`) — zwei parallele Planungsbäume mit unterschiedlicher Benennung.
7. `.gsd/quarantine/projections/` — 13 Einträge. 12 davon 2026-09-13 14:31–14:32 während des Planungs-Bursts (REQUIREMENTS/STATE/ROADMAP/QUEUE, Diff nur `Phase: planning → evaluating-gates`), einer 2026-10-10T08:09 (`KNOWLEDGE.md`, leere Patterns/Lessons-Tabellen). Inhalt unkritisch.

## Root Cause

**Hauptbefund (fachlich relevant):** D006/D007 wurden per `gsd_decision_save` ohne `supersedes`-Feld angelegt; „amends D00x" steht nur im Freitext. Die Memory-Injektion filtert superseded Entscheidungen, sieht hier aber vier aktive und liefert widersprüchliche Build-Vorgaben. Ein Agent, der D004 folgt, könnte die Toolset-/CRT-Festlegung aus `7f09f69` wieder entfernen oder `-p:PlatformToolset=…`-Retargeting erneut einführen.

**Nebenbefund:** S01 wartet auf zwei Quality-Gates, die nie ausgewertet wurden. Kein Fehler — der Workflow wurde nach dem Planning nicht fortgesetzt; die Sicherheitsfixes (z. B. F3, `3daeee5`) laufen außerhalb von GSD und werden in Tasks/Slices nicht erfasst.

**Nebenbefund:** Root-`phases/` ist ein manuell/früher geschriebener Planungsbaum, `.gsd/phases/` die DB-Projektion. `CODEBASE.md` listet nur den Root-Baum; welcher maßgeblich ist, ist nicht dokumentiert.

## Proposed Fix

1. D004 durch D006 und D005 durch D007 formal ablösen: `gsd_decision_save` mit `supersedes: "D004"` bzw. `"D005"` (ggf. D006/D007 neu anlegen, da `supersedes` nachträglich nicht setzbar ist). Danach prüfen, dass `gsd_decision_list` ohne `includeSuperseded` nur noch D006/D007 liefert.
2. Entscheiden: S01 über GSD fortführen (Gates auswerten, F3 als Task nachtragen) oder M001 parken (`gsd_milestone_park`), damit `STATE.md` nicht dauerhaft „evaluating-gates" meldet.
3. Root-`phases/` entweder löschen (DB ist Quelle) oder in `CLAUDE.md` als historisch kennzeichnen.

## Test

`gsd_decision_list` (Default) darf keine zwei aktiven Entscheidungen mit gleichem Scope `build` und widersprechender Choice liefern. Werkzeugseitig wäre eine Warnung sinnvoll, wenn ein Decision-Titel „amends/replaces Dxxx" enthält, aber `supersedes` leer ist.

## Confidence

Hoch für Befund 1 (direkt in `DECISIONS.md` und im injizierten Memory sichtbar). Mittel für Befund 2/3 — ob ein Weiterführen über GSD überhaupt gewollt ist, ist eine Nutzerentscheidung, kein Defekt.

## Korrektur (2026-10-10, später)

Der Hauptbefund war falsch. `gsd_decision_save` mit `supersedes: D004` bzw. `D005` lehnt ab:
"Cannot supersede D004: it is already superseded by D006" (analog D005/D007). Die Ablösung ist
in der Datenbank also korrekt erfasst. `DECISIONS.md` ist ein Append-only-Register und listet
abgelöste Zeilen absichtlich weiter. Offen bleibt nur, dass die `gsd-memory`-Injektion D004/D005
weiterhin als "Architecture"-Memory einspielt — das ist ein Verhalten von GSD, nicht ein Fehler
im Projekt.
