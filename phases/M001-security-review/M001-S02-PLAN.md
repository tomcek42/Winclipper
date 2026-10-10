# M001-S02 Validate fields before size and allocation use

## Goal
Harden all field validation and arithmetic guardrails so attacker-controlled or malformed local data cannot cause underflow, overflow, or invalid allocation behavior.

## Tasks
- [ ] T01 Add range and ordering checks for persisted fields
  - Validate file fields before use in arithmetic or allocation
  - Reject impossible values early

- [ ] T02 Guard unsigned subtraction and size arithmetic
  - Add ordering checks before subtraction or offset math
  - Reject malformed ranges before any arithmetic executes

- [ ] T03 Check every read and readsome return value
  - Treat short reads and failed reads as parser corruption
  - Fail safely instead of continuing with partial state

- [ ] T04 Run malformed-input verification checks
  - Build and run focused checks for malformed local data
  - Confirm fail-safe behavior for parser corruption

## Evidence expected
- validated field checks in affected code paths
- fail-safe handling for short reads and bad ranges
- build evidence and regression notes

## Exit criteria
- No unsigned subtraction occurs without ordering validation
- All critical file fields are bounded or rejected before allocation
- Malformed data fails safely and predictably
