# GSD Graph: Winclipper security and hardening map

## Core relations
- `%LOCALAPPDATA%\Winclipper\Winclipper\` -> affects -> parser startup and loading
- parser startup -> reads -> persisted clip data
- persisted clip data -> can trigger -> unsafe arithmetic or allocation
- unsafe arithmetic -> relates to -> unchecked subtraction and bad field ordering
- bad field ordering -> relates to -> malformed files, underflow, invalid size calculations
- malformed local data -> affects -> build and runtime robustness
- plaintext persistence -> relates to -> DPAPI protection requirement
- absolute include path drift -> relates to -> upstream merge risk
- WiX assumptions -> relates to -> build reproducibility and release readiness

## Security anchors
- local appdata is untrusted
- deserialization must validate all fields before arithmetic
- read/readsome results must be checked
- new persistence must be encrypted at rest

## Milestones that capture the graph
- M001: Security Review and Trust Boundary Hardening
- M002: Persistence Protection and Data Integrity
- M003: Build, Toolchain and Merge Safety
