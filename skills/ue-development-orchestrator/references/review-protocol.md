# Complete review protocol

## Contents

1. Risk selection
2. Required gates
3. Two-pass review
4. UE verification matrix
5. Evidence ledger
6. Finding policy
7. Completion rule

## 1. Select risk

| Tier | Examples | Minimum review depth |
|---|---|---|
| R0 | Documentation, comments, Skill text, read-only analysis | Requirement trace, diff review, structural validation. |
| R1 | Contained C++/config/asset edit with local impact | R0 plus affected build/test or state re-query. |
| R2 | Cross-module change, Blueprint graph, GAS, replication, async/threading, plugin integration | Full two-pass review plus affected-target build and domain checks. |
| R3 | Deletion, migration, packaging/release, security-sensitive config, broad asset mutation | R2 plus explicit authorization, rollback plan, broader regression check, and destructive-target verification. |

Choose the highest tier triggered by any part of the change. Do not lower the tier to save tokens; narrow the scope instead.

## 2. Required gates

### G0 — Scope and requirements

- Convert the request into atomic requirements.
- Mark each as implemented, intentionally unchanged, blocked, or out of scope.
- Record user approvals and time/token constraints.
- Reject unrelated refactors and speculative enhancements.

### G1 — Baseline and source integrity

- Identify the project, engine version, modules, plugins, assets, and active specialist Skills.
- Confirm referenced files/scripts actually exist.
- Capture the pre-change failure or state when practical.
- Distinguish current project evidence from course examples and assumptions.

### G2 — Change review

- Inspect the actual diff and any editor-side mutations.
- Check accidental files, generated output, secrets, absolute machine paths, stale TODOs, and unrelated formatting.
- Confirm ownership, lifecycle, thread, authority, module, and asset dependency assumptions.
- For Blueprint graphs, inspect naming, layout, lint, compile state, and unintended node changes.

### G3 — Verification

- Identify which command or re-query proves each completion claim.
- Run the full relevant check after the final change.
- Read the exit code and complete relevant output; count failures and warnings.
- Re-query editor state after mutations rather than trusting the write response.
- Never reuse stale evidence from before the final change.

### G4 — Regression and safety

- Run the narrowest meaningful regression set, then broaden for R2/R3 work.
- Verify rollback or transaction behavior for stateful changes.
- Check network roles, GameThread handoff, UObject lifetime, packaging target, and platform configuration when applicable.
- Confirm unrelated user work remains untouched.

### G5 — Delivery

- Match every requirement to evidence or a disclosed gap.
- Report unresolved findings by severity.
- State exactly what was not tested and why.
- Make no “complete”, “fixed”, “passing”, or equivalent claim without fresh evidence.

## 3. Two-pass review

Run both passes for R2/R3 and any user request for a complete review.

Use an independent fresh-context reviewer when the runtime supports it. Provide raw requirements, diff or editor mutation inventory, and fresh logs only. Do not provide the implementer's summary during the first compliance pass. If independence is unavailable, disclose this limitation and rerun the passes from the raw artifacts in a fresh context.

### Pass A — Compliance

Review only against the user request, project instructions, selected UE Skill rules, engine/version constraints, and safety approvals. Find missing, extra, or incorrectly interpreted work.

### Pass B — Quality

Review maintainability, API correctness, ownership/GC, thread safety, network authority, performance, asset integrity, error handling, observability, test quality, and token/context economy.

Do not mix passes: compliance must not be hidden by code-quality commentary.

## 4. UE verification matrix

| Changed surface | Required evidence |
|---|---|
| C++/module | Affected target/module compiles after final edit; relevant automated or focused runtime test. |
| Blueprint | Auto-layout after graph mutation, lint findings resolved, clean compile, intended assets saved. |
| Asset/property/tag | Re-query exact changed value and affected references/dependencies. |
| Gameplay/GAS | Correct ASC/attribute/ability/effect state; authority, prediction, and replication checked when relevant. |
| Networking | Appropriate server/client PIE topology; role-specific behavior and RPC/property replication evidence. |
| Async/threading | No GameThread blocking; thread handoff, cancellation, shutdown, and UObject lifetime checked. |
| Editor automation | Bridge `ready: true`, verified API/signature, transaction for mutation, post-write state query. |
| Packaging/platform | Intended target/configuration packaging result and relevant platform/plugin permissions/config. |
| Skill/documentation | Official Skill validator, token/link audit, placeholder scan, trigger and routing scenarios. |

## 5. Evidence ledger

Use the single durable checkpoint created by `scripts/checkpoint.py`; render this compact evidence view when reporting:

| Claim/requirement | Change or non-change | Proof | Fresh? | Status |
|---|---|---|---|---|
| Example: module compiles | `Source/Foo/...` | Build command, exit 0 | Yes | Pass |

Evidence must be reproducible: include the command or bridge query, target, result, and relevant count/error. Avoid pasting full noisy logs.

## 6. Finding policy

- Critical: destructive risk, data loss, security issue, invalid completion evidence—stop and resolve or obtain explicit direction.
- High: requirement failure, build/compile failure, unsafe UE lifecycle/thread/network behavior—must resolve before completion.
- Medium: maintainability, incomplete regression coverage, plausible edge case—resolve when within scope or disclose clearly.
- Low: optional polish—record only if useful; do not expand scope automatically.

After a fix, rerun the invalidated gate. Do not assume the earlier evidence still applies.

## 7. Completion rule

Completion requires all of the following:

- Every requirement has a status.
- Critical/high findings are zero.
- Required checks ran after the final relevant edit.
- Evidence supports each success claim.
- Unverified areas and remaining medium/low findings are disclosed.
- Token budget did not remove a safety or correctness gate.
