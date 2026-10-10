# M001-S01 Audit deserialization and parse paths

## Goal
Identify all parsing and deserialization paths that consume local persisted state and map the risk to specific source files and fields.

## Tasks
- [ ] T01 Map the parsing and deserialization flow
  - Inspect: Bitmap.cpp, Clip.cpp, ClipsCollection.cpp, UserSettings.cpp
  - Outcome: call chain and trust boundary are documented

- [ ] T02 Identify unsafe numeric and length fields
  - Review all size/offset/length fields used during parsing
  - Outcome: all unsafe arithmetic sites are listed

- [ ] T03 Document root cause and trust boundary
  - Tie the issue to local appdata mutability and startup parsing
  - Outcome: root cause analysis is recorded

## Evidence expected
- code-path map
- field inventory for risky values
- risk summary tied to local appdata treats as untrusted

## Exit criteria
- All parsing entry points are mapped
- Unsafe arithmetic assumptions are documented
- The trust boundary is explicit
