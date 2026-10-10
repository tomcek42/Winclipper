# Requirements

This file is the explicit capability and coverage contract for the project.

## Active

### R001 — Untrusted local state must never be treated as trusted input
- Class: compliance/security
- Status: active
- Description: Untrusted local state must never be treated as trusted input
- Why it matters: %LOCALAPPDATA%\Winclipper\Winclipper\ is writable by user-context processes, so persisted data can be attacker-controlled.
- Source: claude.md
- Primary owning slice: security-hardening
- Supporting slices: M001-S01,M001-S03
- Validation: Startup and parser code must reject malformed or unexpected local cache data before any allocation or size use.

### R002 — Every parsed numeric or length field must be validated before use in allocation or size logic
- Class: quality-attribute
- Status: active
- Description: Every parsed numeric or length field must be validated before use in allocation or size logic
- Why it matters: Unsafe deserialization creates integer overflows, underflows, and out-of-bounds reads.
- Source: claude.md
- Primary owning slice: security-hardening
- Supporting slices: M001-S02
- Validation: All file fields must be checked for ordering, bounds, minimums, and maximums before use in arithmetic or allocations.

### R003 — Unsigned subtraction used for size or offset calculations must be bounded by ordering checks
- Class: quality-attribute
- Status: active
- Description: Unsigned subtraction used for size or offset calculations must be bounded by ordering checks
- Why it matters: This prevents wraparound and malformed-payload errors in deserialization paths.
- Source: claude.md
- Primary owning slice: security-hardening
- Supporting slices: M001-S02
- Validation: All relevant arithmetic sites must verify ordering before subtraction and reject malformed values.

### R004 — Read and readsome return values must always be checked
- Class: quality-attribute
- Status: active
- Description: Read and readsome return values must always be checked
- Why it matters: Silent partial reads can hide corruption and leave parser state inconsistent.
- Source: claude.md
- Primary owning slice: security-hardening
- Supporting slices: M001-S02
- Validation: All read/readsome calls must inspect return values and trigger fail-safe behavior on short or failed reads.

### R005 — New persistence must not store clipboard data in cleartext
- Class: compliance/security
- Status: active
- Description: New persistence must not store clipboard data in cleartext
- Why it matters: Clipboard content is sensitive and current base64 storage exposes it in plaintext on disk.
- Source: claude.md
- Primary owning slice: security-hardening
- Supporting slices: M002-S01,M002-S02
- Validation: Any new persistence path must encrypt data at rest or otherwise protect it from local user-context access.

### R006 — Build verification must remain reproducible under retargeting
- Class: operability
- Status: active
- Description: Build verification must remain reproducible under retargeting
- Why it matters: The project depends on a specific toolchain and SDK; build steps must remain clear and repetitive.
- Source: claude.md
- Primary owning slice: build-ops
- Supporting slices: M003-S01
- Validation: The repository must be buildable with the documented retargeting command and maintain clear dependency notes.

## Validated

## Deferred

## Out of Scope

## Traceability

| ID | Class | Status | Primary owner | Supporting | Proof |
| --- | --- | --- | --- | --- | --- |
| R001 | compliance/security | active | security-hardening | M001-S01,M001-S03 | Startup and parser code must reject malformed or unexpected local cache data before any allocation or size use. |
| R002 | quality-attribute | active | security-hardening | M001-S02 | All file fields must be checked for ordering, bounds, minimums, and maximums before use in arithmetic or allocations. |
| R003 | quality-attribute | active | security-hardening | M001-S02 | All relevant arithmetic sites must verify ordering before subtraction and reject malformed values. |
| R004 | quality-attribute | active | security-hardening | M001-S02 | All read/readsome calls must inspect return values and trigger fail-safe behavior on short or failed reads. |
| R005 | compliance/security | active | security-hardening | M002-S01,M002-S02 | Any new persistence path must encrypt data at rest or otherwise protect it from local user-context access. |
| R006 | operability | active | build-ops | M003-S01 | The repository must be buildable with the documented retargeting command and maintain clear dependency notes. |

## Coverage Summary

- Active requirements: 6
- Mapped to slices: 0
- Validated: 0
- Unmapped active requirements: 6
