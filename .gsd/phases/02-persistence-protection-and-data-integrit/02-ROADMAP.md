# M002: Persistence Protection and Data Integrity

**Vision:** Protect persisted clip data and any future storage path from plaintext exposure and corrupted state by enforcing secure storage and fail-safe handling for malformed or manipulated local data.

## Success Criteria

- Clipboard persistence is no longer plain local base64 data without protection.
- Any new persistence path is encrypted at rest or otherwise protected from local-access abuse.
- Corrupt or invalid persisted state fails safely without exposing data or destabilizing the app.

## Slices

- [ ] **S01: Assess plaintext persistence risk** `risk:high` `depends:[]`
  > After this: The current clip persistence pattern is mapped to the local-attack risk and a clear decision is made for secure replacement storage.

- [ ] **S02: Implement protected persistence strategy** `risk:high` `depends:[S01]`
  > After this: Any new persistence path encrypts clip data at rest and includes compatibility handling for existing data or unreadable entries.

- [ ] **S03: Recovery and compatibility checks** `risk:medium` `depends:[S02]`
  > After this: Corrupt or legacy local data can be rejected or migrated without crashing or exposing secrets.

## Boundary Map

- Persisted local clipboard data is the data-at-rest boundary.
- The local user-context filesystem is the adversarial boundary.
- The read/write persistence path is the enforcement boundary for secret protection.
- Safe-failure handling is the closure boundary for corrupted or stale data.
<!-- gsd:state-version=8:0 -->
