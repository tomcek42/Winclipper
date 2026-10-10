# M001-S04 Verification and regression checks

## Goal
Verify that the hardening change is real, buildable, and stable under malformed input conditions.

## Tasks
- [ ] T01 Build the project with the documented retargeting path
  - Use the documented MSBuild command with v145 and Windows SDK setup
  - Capture toolchain constraints and environment notes

- [ ] T02 Run focused malformed-input regression checks
  - Exercise parser paths with malformed or incomplete persisted data
  - Confirm invalid state fails safely without arithmetic underflow or bad allocation

- [ ] T03 Close milestone evidence and residual risk notes
  - Consolidate build evidence and regression checks into final milestone notes
  - Record any residual risk or follow-up work

## Evidence expected
- build proof for the current state
- malformed-input regression findings
- final closeout summary with residual risk

## Exit criteria
- Build passes with the documented retargeting path
- Focused regression checks pass
- Evidence and residual risk are recorded for the milestone
