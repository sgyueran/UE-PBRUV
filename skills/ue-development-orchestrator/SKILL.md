---
name: ue-development-orchestrator
description: Use for Unreal Engine project work involving C++, gameplay, GAS, networking, Enhanced Input, assets, Blueprints, editor automation, animation, materials, levels, performance, builds, packaging, or mixed code-and-editor changes that need coordinated UE skills, bounded context use, and evidence-based review.
---

# UE Development Orchestrator

Coordinate Unreal Engine work through the installed specialist skills, current project evidence, and a single verification loop. Prefer Chinese when the user writes Chinese.

## Start with discovery

1. Inspect the current task context for every relevant Unreal/UE skill. At minimum, check:
   - `$xg-uecpp-course` for UE C++, engine architecture, gameplay systems, GAS, networking, async work, Enhanced Input, plugins, and diagnostics.
   - `$unreal-bridge` for querying or changing a running UE 5.3+ editor, assets, Blueprints, levels, animation, materials, UMG, DataTables, GameplayTags, PoseSearch, Chooser, PIE, or performance state.
2. Inspect the project before proposing changes:
   - Locate the `.uproject`, `Source/`, `Plugins/`, `Config/`, and target/module `.Build.cs` files.
   - Read `EngineAssociation` and project/plugin descriptors. Do not assume UE 5.4 APIs merely because the C++ reference skill targets UE 5.4.
   - Check project-local instructions and tooling.
3. If a relevant UE skill is unavailable, continue with direct project inspection and state that the specialist skill was unavailable. Never imply it was used.

Read [references/capability-map.md](references/capability-map.md) only when the route is mixed, unclear, or requires specialist discovery.

## Set a context budget

Classify the task as small, standard, or complex before loading specialist material. The default planning envelopes are 2,500, 6,000, and 10,000 estimated loaded-context tokens per phase. Read [references/token-budget.md](references/token-budget.md) only for standard/complex work, explicit token constraints, or checkpoint recovery.

- Start from metadata, file lists, targeted search, and narrow source windows.
- Load only the specialist skill and reference sections required by the selected route.
- Never reread unchanged large files or paste full logs when a focused excerpt proves the point.
- For standard/complex work, use `scripts/checkpoint.py` to persist one working ledger and record every loaded source with its estimated size.
- Treat budget values as planning estimates. Never claim they are exact platform token telemetry.

## Route the task

Use all routes that materially apply:

| Task | Route |
|---|---|
| Explain, design, or implement UE C++ | Load `$xg-uecpp-course`, then verify against the current project and engine version. |
| Query or mutate the open editor | Load `$unreal-bridge`; follow its preconditions, wrapper-first policy, and safety rules. |
| Implement C++ and connect assets/BPs | Use both: design and edit C++ first, build/reload, then inspect or wire editor state through the bridge. |
| Diagnose build/package/runtime issues | Inspect logs and project configuration first; use the C++ skill for likely causes and the bridge only when live editor evidence helps. |
| Diagnose UHT/UBT/link/cook/stage failures | Read [references/build-diagnostics.md](references/build-diagnostics.md); identify the failing phase before changing files. |
| Add or run UE tests/CI | Read [references/testing-ci.md](references/testing-ci.md); keep interactive Bridge work out of unattended CI. |
| Mutate binary assets or team content | Read [references/asset-source-control.md](references/asset-source-control.md) before checkout, rename, migration, redirector, or save operations. |
| Work involving XR/OpenXR or Android | Load `$ue-xr-android` when available. Otherwise inspect project plugins/configuration and use engine-version-appropriate primary documentation. |

## Plan one coherent change

Before editing, state the intended outcome, affected modules/assets, runtime/editor boundary, thread and network authority assumptions, and verification method.

For implementation decisions:

- Prefer C++ for reusable gameplay logic, performance-sensitive code, complex state, networking, and maintainable systems.
- Use Blueprints for thin composition, data wiring, presentation, and user-requested graph work.
- Before Blueprint node/graph mutation, ask a separate yes/no confirmation. After approval, state `$unreal-bridge` must `ping` `ready: true`; then inspect, transact, layout/lint/compile/save/re-query.
- For mixed C++/Blueprint work, complete R2 independent review after fresh verification.
- Respect UObject ownership, reflection, GC, module dependencies, subsystem lifetimes, GameThread restrictions, and server authority.
- Treat engine-version differences as evidence to check, not details to guess.

## Execute live-editor work safely

When `$unreal-bridge` applies:

1. Ping first and require `ready: true`.
2. Prefer the generated `unreal_bridge` wrapper with keyword arguments.
3. Verify function names, signatures, result fields, and object properties before calling. Use the manifest or the relevant bridge reference.
4. Batch multi-step one-off work in a single stdin execution. Do not block the GameThread with sleeps or polling loops.
5. Search assets across all content when the user supplies only a name and plugin content is possible.
6. Do not silently fall back to broad raw `unreal.*` registry scans. Ask before a genuine raw fallback.
7. Describe state-changing operations first, wrap them in editor transactions, and never delete assets or actors without explicit confirmation.
8. After Blueprint graph changes, auto-layout, lint, resolve findings, compile, and save only the intended assets.

## Run the review gates

Select a risk tier and complete the five gates below. Read [references/review-protocol.md](references/review-protocol.md) only for R2/R3 work or an explicit complete review:

1. Trace each user requirement to a change or an explicit non-change.
2. Review the actual diff or mutated editor state for scope and unintended effects.
3. Run task-appropriate verification and capture fresh evidence.
4. Perform a second pass for maintainability, UE correctness, safety, and regressions.
5. Resolve critical/high findings before completion; disclose lower findings and unverified areas.

Treat mixed C++/Blueprint changes as R2. For R2/R3, use an independent fresh-context reviewer when available. Give it raw requirements, the actual diff/editor mutations, and verification output—not the implementer's conclusions. If unavailable, disclose the fallback and perform a fresh-context self-review.

On resume, load the persisted checkpoint, re-inspect the current diff/editor state, and invalidate evidence affected by later changes before continuing.

For changes to this Skill itself, run `scripts/audit_skill_budget.py` and the official Skill validator.
Use `evals/routing-safety-cases.json` for fresh-agent behavioral evaluation; keyword presence checks do not count as behavior tests.

## Verify the result

Choose checks that match the change:

- C++: compile the affected target/module and inspect the complete relevant error output.
- Blueprint: lint and compile cleanly; inspect graph layout when nodes changed.
- Assets/editor: re-query the changed property, dependency, tag, actor, material, animation, or asset state.
- Gameplay/networking: test the correct PIE topology and authority role; do not treat single-player success as replication proof.
- Async work: verify thread handoff and object lifetime behavior.
- Packaging: validate the intended target platform and configuration rather than relying on Editor success.

Only claim success when fresh evidence from the current run supports it. Report what changed, the exact checks and results, what remains unverified, review findings, and any engine-version or missing-source limitation.

## Preserve source boundaries

Keep specialist details in their owning skills:

- Do not duplicate the full C++ course or bridge API catalog here.
- Load only the relevant specialist references for the current task.
- Prefer concise evidence summaries over narrative repetition; preserve exact requirement IDs, API signatures, error lines, and verification results.
- If a source skill changes, treat its current instructions as authoritative unless they conflict with user or higher-priority instructions.
- Preserve unrelated user changes in the project and avoid destructive cleanup.
