# Forensics 2026-10-10 — stale decisions in memory, S01 stalled since 2026-09-13

## Symptom

No crash. Observed:
- `STATE.md`: `**Phase:** evaluating-gates` / "Evaluate 2 quality gate(s) for S01 before execution." — unchanged since 2026-09-13.
- The project memory injected into the session contains both
  "Use retargeting instead of permanent project edits" (D004) **and**
  "Pin project files to current toolset with static CRT" (D006), as well as
  "Keep the Controls include path under active review during upstream merges" (D005) **and**
  "Controls include path is project-relative … no recurring merge check needed" (D007).

## Evidence Trail

1. `.gsd/auto.lock` — absent; `.gsd/activity/`, `.gsd/journal/`, `.gsd/metrics.json` — absent. Auto mode never ran in this project; there is no crash and no stuck loop.
2. `.gsd/event-log.jsonl:1-8` / `.gsd/audit/events.jsonl` — only `plan-milestone`/`plan-slice` (M001, S01–S04, M002, M003, M002/S01), all session `e97f225c…`, 2026-09-13 14:31–14:32. No gate, task or execute event afterwards.
3. `.gsd/state.json` `next.command = "/gsd auto"`, `updated_at = 2026-09-13T14:32:54Z`; `.gsd/runtime/write-gate-state.json` `pendingGateId: null`, `verifiedApprovalGates: []`.
4. `.gsd/DECISIONS.md:12-15` — D006 "(amends D004)", D007 "(amends D005)": the supersession appears only in the title text. D004/D005 are not marked as superseded and remain active.
5. `git log` — `7f09f69` (v145/static CRT pinned) and `8a6ed15` (include path) implement D006/D007; D004 ("without permanently changing project files") contradicts the current state.
6. `.gsd/phases/0{1,2,3}-*/` (rendered, tracked in `.compat.json`) vs. `phases/M00{1,2,3}-*/` in the repo root (tracked, `abe88f0`) — two parallel planning trees with different naming.
7. `.gsd/quarantine/projections/` — 13 entries. 12 of them 2026-09-13 14:31–14:32 during the planning burst (REQUIREMENTS/STATE/ROADMAP/QUEUE, diff only `Phase: planning → evaluating-gates`), one at 2026-10-10T08:09 (`KNOWLEDGE.md`, empty Patterns/Lessons tables). Content uncritical.

## Root Cause

**Main finding (functionally relevant):** D006/D007 were created with `gsd_decision_save` without the `supersedes` field; "amends D00x" is only free text. The memory injection filters superseded decisions, but sees four active ones here and delivers contradictory build guidance. An agent following D004 could remove the toolset/CRT pinning from `7f09f69` or reintroduce `-p:PlatformToolset=…` retargeting.

**Side finding:** S01 is waiting on two quality gates that were never evaluated. Not a defect — the workflow was not continued after planning; the security fixes (e.g. F3, `3daeee5`) happen outside GSD and are not tracked in tasks/slices.

**Side finding:** the root `phases/` is a manually/earlier written planning tree, `.gsd/phases/` is the DB projection. `CODEBASE.md` lists only the root tree; which one is authoritative is not documented.

## Proposed Fix

1. Formally supersede D004 with D006 and D005 with D007: `gsd_decision_save` with `supersedes: "D004"` or `"D005"` (possibly recreate D006/D007, since `supersedes` cannot be set afterwards). Then check that `gsd_decision_list` without `includeSuperseded` returns only D006/D007.
2. Decide: continue S01 through GSD (evaluate the gates, add F3 as a task) or park M001 (`gsd_milestone_park`) so that `STATE.md` does not report "evaluating-gates" forever.
3. Either delete the root `phases/` (the DB is the source) or mark it as historical in `CLAUDE.md`.

## Test

`gsd_decision_list` (default) must not return two active decisions with the same scope `build` and contradicting choices. On the tool side, a warning would help when a decision title contains "amends/replaces Dxxx" but `supersedes` is empty.

## Confidence

High for finding 1 (directly visible in `DECISIONS.md` and in the injected memory). Medium for findings 2/3 — whether continuing through GSD is wanted at all is a user decision, not a defect.

## Correction (2026-10-10, later)

The main finding was wrong. `gsd_decision_save` with `supersedes: D004` or `D005` refuses:
"Cannot supersede D004: it is already superseded by D006" (likewise D005/D007). The supersession is
therefore recorded correctly in the database. `DECISIONS.md` is an append-only register and lists
superseded rows on purpose. The only remaining issue is that the `gsd-memory` injection still feeds
D004/D005 in as "Architecture" memory — that is GSD behavior, not a defect in the project.
