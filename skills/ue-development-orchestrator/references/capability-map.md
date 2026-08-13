# UE capability map

Load this file only when the route is mixed or unclear.

| Skill/reference | Use for | Important boundary |
|---|---|---|
| `$xg-uecpp-course` | UE C++, reflection/GC, containers, subsystems, async, HTTP/WebSocket/TCP, replication, GAS, Enhanced Input, plugins | UE 5.4-oriented guidance. Historical `D:\UPS\...` course source is absent; verify against the active project and engine. |
| `$unreal-bridge` | Running-editor queries/mutations: assets, Blueprint/UMG, animation, materials, levels, PIE, tags, PoseSearch, Chooser, performance | Require `ready: true`, wrapper/manifest verification, transactions, exact saves, and post-write re-query. Load its task-specific reference. |
| `$ue-xr-android` | OpenXR, VR/MR, Android toolchain, package/deploy/device debugging | Resolve changing requirements from the exact engine version and current primary documentation; require packaged-device evidence. |
| `build-diagnostics.md` | UHT/UBT/compile/link/module/cook/stage/package failures | Identify the failing phase before editing or cleaning. |
| `testing-ci.md` | Automation/Functional/Data Validation/Gauntlet/UAT/BuildGraph and CI | Bridge is interactive-editor tooling, not the default unattended runner. |
| `asset-source-control.md` | Binary assets, checkout/locking, OFPA, rename/migration/redirectors | Bound scope, preview, preserve rollback, and save only intended assets. |

## Combined routes

- C++ plus asset/Blueprint wiring: design/edit/build C++ first; use Bridge only after the reflected class is available; then compile and re-query assets.
- GAS/networking: use the C++ skill for architecture and authority; use Bridge for asset/tag state; verify server/client roles, prediction, and replication.
- Blueprint graph authoring: offer maintainable C++ first, require explicit graph-write confirmation, then plan/build/layout/lint/comment/compile.
- Platform delivery: combine build diagnostics, testing/CI, and `$ue-xr-android`; Editor success never proves packaged-device success.

Re-scan the installed catalog for newer specialist Skills on every relevant task. If none applies, disclose the gap and inspect the project directly.
