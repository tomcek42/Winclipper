# M001 Security Review and Trust Boundary Hardening

## Vision
Harden Winclipper against malformed local state and unsafe parsing by validating all persisted inputs before they are used for arithmetic or allocation, while documenting the trust boundary and verifying the fix with build and parser regression checks.

## Key goals
- Treat `%LOCALAPPDATA%\Winclipper\Winclipper\` as untrusted input.
- Validate all numeric, length, and offset fields before arithmetic or allocation.
- Check every read/readsome result before continuing parser flow.
- Produce evidence that malformed local state fails safely and predictably.

## Slices
### S01 Audit deserialization and parse paths
- Scope: all startup and loader paths that parse persisted local state
- Risk: medium
- Goal: identify every parse entry point and unsafe field use
- Success: all affected files are mapped and root cause is documented

### S02 Validate fields before size and allocation use
- Scope: field validation, subtraction guards, read error handling
- Risk: high
- Goal: reject malformed data before size arithmetic or allocation
- Success: no unchecked arithmetic or unsafe allocation path remains

### S03 Define and enforce trust boundaries for local appdata
- Scope: trust boundary policy and startup assumptions
- Risk: medium
- Goal: explicitly treat local appdata as untrusted, never as a safe source
- Success: design notes and code assumptions reflect the boundary

### S04 Verification and regression checks
- Scope: compile and malformed-input validation
- Risk: medium
- Goal: prove the fix holds under build and malformed-input checks
- Success: build evidence and regression checks are recorded

## Requirements addressed
- R001: Untrusted local state must never be treated as trusted input
- R002: Every parsed numeric or length field must be validated before use
- R003: Unsigned subtraction must be bounded by ordering checks
- R004: Read and readsome return values must always be checked

## Decisions
- D001: Treat Winclipper appdata as untrusted input
- D002: Validate all size and offset arithmetic before use
