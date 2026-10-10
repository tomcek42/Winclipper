# M001-S03 Define and enforce trust boundaries for local appdata

## Goal
Ensure `%LOCALAPPDATA%\Winclipper\Winclipper\` is treated as an untrusted boundary and never as safe source data.

## Tasks
- [ ] T01 Document the trust boundary in design notes
  - Note why local user-context files are attacker-controlled
  - Record the boundary assumption clearly

- [ ] T02 Audit startup and parser trust assumptions
  - Verify no code path silently treats local cache as trusted
  - Confirm parser logic rejects malformed values and bad state

- [ ] T03 Capture residual risk and operational expectations
  - Record remaining threat model and safe-failure expectations
  - Note what behavior is required for malformed or manipulated local data

## Evidence expected
- written trust-boundary documentation
- code-path review confirming the boundary is enforced
- explicit residual-risk note

## Exit criteria
- Trust boundary is documented and used consistently
- Startup assumptions no longer rely on local appdata trust
- Remaining operational risk is explicit
