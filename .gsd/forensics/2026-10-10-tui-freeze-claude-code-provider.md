## Symptom

GSD (pi TUI) repeatedly freezes right after a prompt is submitted: no assistant output appears, the
input box no longer accepts keystrokes (Esc / typing have no effect), and the session has to be
killed hard (closing the terminal). Happens across projects, several times per week.

## Environment

- `@opengsd/gsd-pi` 1.21.1 (global npm install), Node v26.7.0, Windows 11 Enterprise 10.0.26200
- Provider `claude-code` (Claude Code CLI subscription path), model `claude-opus-5-5`, thinking `minimal`
- `~/.gsd/agent/settings.json`: `compaction.enabled: true`, `compactionThresholdOverride: 0.7`,
  `terminal.adaptiveMode: "workflow"`, `steeringMode`/`followUpMode: "one-at-a-time"`
- Note: `settings.json` has `lastChangelogVersion: "2.51.0"` while the installed package reports 1.21.1

## Evidence Trail

Every frozen session ends with the same two records — the user prompt and the hidden `gsd-memory`
injection, written in the same millisecond — and **nothing after it** (no assistant message, no
`stopReason`, no error). A new session is started 8–20 min later (= hard kill + restart).

| Session file (`~/.gsd/sessions/<project>/…`) | Last records | Next session |
|---|---|---|
| `Winclipper/2026-10-10T08-09-23-574Z_…jsonl` (15 lines) | `user` 08:14:18.023 "Ja, gehe so vor." → `custom_message/gsd-memory` 08:14:18.023 | 08:22:11 |
| `Winclipper/2026-10-10T08-22-11-002Z_…jsonl` (38 lines) | `user` 08:38:33.648 "Ja, führe die Schritte 1 und 2 aus." → `gsd-memory` 08:38:33.648 | 08:55:47 |
| `VirtualCopyPaste/2026-10-05T19-31-28-397Z_…jsonl` (43 lines) | `user` 20:10:18.860 (commit + release request) → `gsd-memory` 20:10:18.861 | — |
| `VirtualCopyPaste/2026-10-06T04-40-10-388Z_…jsonl` (55 lines) | assistant 18:48:32 `stopReason: aborted` "Claude Code stream aborted by caller" → `user` 18:48:53 → `gsd-memory` 18:48:53 | 19:08:34 |

Common traits:
- All four are short follow-up prompts that ask for **tool-using work** (commit, run commands, apply steps),
  never pure questions. The preceding turn always completed normally (`stopReason: stop`) except in the last case.
- All four use provider `claude-code`.
- No `.gsd/auto.lock`, `activity/`, `journal/` exist — this is interactive (non-auto) mode.
- Sessions that did complete show the `gsd-memory` record followed by an assistant message within 5–20 s,
  so the injection itself completes; the hang is after it, before the first streamed assistant event is persisted.

## Root Cause (most likely — known bug #2636 / #2743)

The installed 1.21.1 still contains the infinite loop fixed on main by #2649 (merged after the 1.21.1 cut):

`node_modules/@opengsd/gsd-pi/packages/gsd-agent-modes/dist/modes/interactive/components/tool-execution.js:63`
`while (lastCr === line.length - 1) { … }` in `collapseCarriageReturnFrames` spins forever on a line made
only of `\r`. This happens with any tool output containing blank CRLF lines, which on Windows is common
(git, msbuild, PowerShell output). The loop runs on the main thread, so it blocks keyboard input too, and
only a hard kill recovers. That matches the symptom exactly.

It also fits the logs. With the `claude-code` provider, the assistant message and tool results are only
persisted at turn end. In completed sessions all `toolResult` records share one timestamp. A turn that
wedges while rendering its first live tool output therefore leaves *nothing* after the `gsd-memory`
record. Every hung prompt asked for tool work. The `\r`-only output itself is not in the logs (it was never
persisted), so this remains a strong match, not a proof.

## Alternative hypotheses (only if the freeze persists after the fix ships)

The keyboard is dead, not just the output. A turn merely waiting on a slow/stuck network stream would
still let the TUI accept Esc. A dead input loop points to the **Node event loop being blocked** (synchronous
work) or the TUI input handler being detached while the turn starts.

Candidates found in the installed code (`~/.gsd/agent/extensions/`):
- `claude-code-cli/stream-adapter.js:255` — `execSync(getClaudeLookupCommand(), { timeout: 5_000 })`
  on the turn path (bounded to 5 s, so unlikely alone to cause a multi-minute hang).
- `gsd/db/engine.js:133,246,263,…` — `PRAGMA busy_timeout = 5000` on a synchronous SQLite handle; the
  `gsd-workflow` MCP server is a second process writing the same `.gsd/gsd.db` (WAL). Repeated lock
  waits would block the event loop in 5 s chunks.
- `claude-code-cli/stream-adapter.js:1374-1424` — AskUserQuestion elicitation forms: if the SDK opens an
  interview/permission form that never renders, the turn waits on it while the TUI may have lost focus.

I could not determine which one applies from the persisted artifacts alone.

## What would confirm it

- A Node CPU profile or `--inspect` stack dump of the frozen process (`node --cpu-prof` or `kill -USR1`-equivalent
  on Windows via `process._debugProcess(pid)`), showing whether the main thread is in `execSync`, SQLite, or idle.
- Debug log of a frozen session (`GSD_DEBUG`/debug-logger output) showing the last event before the hang.
- Whether the child `claude` CLI process is still alive and consuming CPU during the freeze.

## Proposed Fix

1. Persist an early "turn started / provider request sent" record so frozen turns are diagnosable from the session log.
2. Make the TUI input loop independent from turn startup: Esc must always abort, even before the first stream event.
3. Replace `execSync` CLI lookup on the turn path with a cached/async lookup.
4. Add a watchdog: if no stream event arrives within N seconds, surface a notice and allow abort.

## Test

A test that starts a turn with a provider that never emits its first event and asserts that an Esc keypress
still aborts the turn and returns control to the editor.

## Confidence

Symptom and pattern: **high** (4 session logs). Root cause = #2636/#2743: **medium-high**. The buggy loop
is verified in the installed dist and the logs are consistent with it, but the triggering output was not persisted.
Confirmation: the freezes stop after upgrading to the first release after 1.21.1.
