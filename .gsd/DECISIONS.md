# Decisions Register

<!-- Append-only. Never edit or remove existing rows.
     To reverse a decision, add a new row that supersedes it.
     Read this file at the start of any planning or research phase. -->

| # | When | Scope | Decision | Choice | Rationale | Revisable? | Made By |
| --- | --- | --- | --- | --- | --- | --- | --- |
| D001 | M001 | security | Treat Winclipper appdata as untrusted input | Do not trust %LOCALAPPDATA%\Winclipper\Winclipper\ as safe persisted state. | Any user-context process can modify it, and the startup code parses it. The security model must treat all local cache content as attacker-controlled. | Yes | agent |
| D002 | M001 | security | Validate all size and offset arithmetic before use | Reject malformed or out-of-order fields before any unsigned subtraction or allocation arithmetic. | This prevents wraparound, invalid bounds, and unsafe size calculations in the deserialization path. | Yes | agent |
| D003 | M002 | security | Use DPAPI for any new persistence format | New clipboard persistence must be encrypted at rest with DPAPI or an equivalent secure mechanism. | The current base64 persistence is plaintext and is not acceptable for newly added storage. | Yes | agent |
| D004 | M003 | build | Use retargeting instead of permanent project edits | Retarget the solution with v145 and WindowsTargetPlatformVersion 10.0.26100.0 during build, without permanently changing project files. | This preserves upstream compatibility and avoids merge churn while keeping the project buildable in this environment. | Yes | agent |
| D005 | M003 | integration | Keep the Controls include path under active review during upstream merges | Use the project-relative Controls path and treat the original absolute include path as a recurring merge hazard. | The original absolute author path can reappear after upstream syncs and must be caught during validation. | Yes | agent |
| D006 | M003, after deciding against upstream merges (2026-10-10) | build | Pin project files to current toolset with static CRT (amends D004) | Winclipper.vcxproj and Controls.vcxproj permanently use PlatformToolset v145, WindowsTargetPlatformVersion 10.0.26100.0 and static CRT (RuntimeLibrary MultiThreaded / MultiThreadedDebug). Plain msbuild without retargeting parameters. | No upstream merges are planned, so merge churn no longer matters. Static CRT removes the dependency on a VC++ redistributable that the installer's VC141 merge modules cannot satisfy. Verified: Release x64/x86 and Debug x64 build; dumpbin shows no VCRUNTIME/MSVCP imports. | Yes | collaborative |
| D007 | M003, commit 8a6ed15 | integration | Controls include path is project-relative (amends D005) | Winclipper.vcxproj finds Controls.h via $(SolutionDir)..\Controls; no recurring merge check needed. | Fork no longer merges from upstream, so the original author's absolute path cannot return. | Yes | collaborative |
