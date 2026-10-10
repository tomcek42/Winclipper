# M001: Security Review and Trust Boundary Hardening

**Vision:** Harden Winclipper against malformed local state and unsafe parsing by validating all persisted inputs before they are used for arithmetic or allocation, while documenting the trust boundary and verifying the fix with build and parser regression checks.

## Success Criteria

- Untrusted local state is treated as attacker-controlled input.
- All critical parsed fields are validated before use in arithmetic or allocation.
- Malformed or incomplete persisted data fails safely without underflow or invalid reads.
- The fix is backed by verifiable build and regression evidence.

## Slices

- [ ] **S01: Audit deserialization and parse paths** `risk:medium` `depends:[]`
  > After this: A clear risk map of the parsing and deserialization call chain, with untrusted inputs and unsafe assumptions called out.

- [ ] **S02: Validate fields before size and allocation use** `risk:high` `depends:[S01]`
  > After this: Malformed persisted data is rejected before any size arithmetic or allocation occurs, and read failures are treated as data corruption instead of silent success.

- [ ] **S03: Define and enforce trust boundaries for local appdata** `risk:medium` `depends:[S01]`
  > After this: The local Winclipper cache is explicitly treated as untrusted data at startup, and the code and docs state that assumption consistently.

- [ ] **S04: Verification and regression checks** `risk:medium` `depends:[S02,S03]`
  > After this: The patched parser code compiles cleanly and malformed input scenarios are shown to fail without unsafe arithmetic or invalid allocations.

## Boundary Map

- Local appdata cache is the untrusted boundary.
- Startup and parser logic are the trust-boundary enforcement points.
- Allocation and arithmetic logic is the final validation boundary before state is accepted.
- Build and regression validation are the closure boundary for the fix.
<!-- gsd:state-version=5:0 -->
