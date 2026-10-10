# M002 Persistence Protection and Data Integrity

## Vision
Protect persisted clip data and any future storage path from plaintext exposure and corrupted state by enforcing secure storage and fail-safe handling for malformed or manipulated local data.

## Key goals
- Treat persistent local clip contents as sensitive data at rest.
- Prohibit new plaintext persistence.
- Use DPAPI or equivalent protected storage for new persisted state.
- Fail safely when old, corrupt, or unreadable data is encountered.

## Slices
### S01 Assess plaintext persistence risk
- Scope: current local persistence format and exposure path
- Risk: high
- Goal: document why plaintext clip state is unsafe
- Success: threat model and secure replacement requirement are explicit

### S02 Implement protected persistence strategy
- Scope: secure writes, encrypted reads, compatibility planning
- Risk: high
- Goal: secure and protect newly persisted clip data
- Success: new storage path is protected and fail-safe

### S03 Recovery and compatibility checks
- Scope: legacy/corrupt state handling
- Risk: medium
- Goal: verify safe rejection of unreadable or malformed persisted data
- Success: invalid storage fails safely without crash or exposure

## Requirements addressed
- R005: New persistence must not store clipboard data in cleartext
- R006: Build verification must remain reproducible under retargeting
- R008: Protected persistence must use DPAPI or equivalent secure storage

## Decisions
- D003: Use DPAPI for any new persistence format
