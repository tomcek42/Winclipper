# M003 Build, Toolchain and Merge Safety

## Vision
Keep Winclipper buildable in a fresh environment and protect against upstream reintroductions of known include-path and WiX assumptions, while leaving a clean release-readiness trail.

## Key goals
- Document the working retargeting path and runtime/toolchain requirements.
- Track and prevent upstream merge drift for absolute include paths.
- Maintain a release-ready build and maintenance checklist.

## Slices
### S01 Review toolchain and build requirements
- Scope: build command, SDK/toolset assumptions, WiX setup caveats
- Risk: medium
- Goal: make build steps reproducible and explicit
- Success: fresh environment can rebuild with the documented path

### S02 Audit upstream merge hazards
- Scope: absolute include path drift, Controls integration, WiX package assumptions
- Risk: high
- Goal: detect reintroduced issues before release or merge
- Success: merge checklist is explicit and reused

### S03 Release readiness and validation closure
- Scope: final evidence and caveats
- Risk: medium
- Goal: close the milestone with clear maintenance and release notes
- Success: build evidence and known risks are recorded

## Requirements addressed
- R006: Build verification must remain reproducible under retargeting
- R007: Upstream merge drift must be detected for absolute include paths and WiX assumptions

## Decisions
- D004: Use retargeting instead of permanent project edits
- D005: Keep the Controls include path under active review during upstream merges
