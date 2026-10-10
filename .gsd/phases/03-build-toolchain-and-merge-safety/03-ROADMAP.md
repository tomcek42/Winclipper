# M003: Build, Toolchain and Merge Safety

**Vision:** Keep Winclipper buildable in a fresh environment and protect against upstream reintroductions of known include-path and WiX assumptions, while leaving a clean release-readiness trail.

## Success Criteria

- The project build path is documented and reproducible from a fresh environment.
- Known upstream merge hazards are listed and checked during maintenance.
- Release readiness is backed by environment and source-level evidence.

## Slices

- [ ] **S01: Review toolchain and build requirements** `risk:medium` `depends:[]`
  > After this: The project build path and toolchain constraints are captured in a reproducible form that any maintainer can execute without guessing.

- [ ] **S02: Audit upstream merge hazards** `risk:high` `depends:[S01]`
  > After this: Known root-cause issues such as the absolute Controls include path are listed as merge hazards and checked before each upstream sync.

- [ ] **S03: Release readiness and validation closure** `risk:medium` `depends:[S01,S02]`
  > After this: A final audit confirms all critical build and merge risks are either resolved or explicitly tracked, and the project is ready for the next release step.

## Boundary Map

- Toolchain and SDK assumptions are the build boundary.
- Controls include path and WiX assumptions are the merge boundary.
- Release readiness and final validation are the closure boundary.
<!-- gsd:state-version=7:0 -->
