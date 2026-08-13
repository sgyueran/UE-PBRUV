# Testing and continuous integration

Select the smallest test surface that proves the requirement, then add broader packaged or multi-process coverage only where the risk requires it.

## Choose the test layer

| Layer | Use it for | Avoid using it as proof of |
|---|---|---|
| Automation Test / Automation Spec | Deterministic C++ logic, subsystem integration, editor/client behavior, and concise BDD-style cases | Packaged deployment or real multi-process behavior |
| Functional Test | Map-, actor-, streaming-, AI-, input-, or gameplay behavior over multiple frames | Pure logic that can run without a world |
| Data Validation | Asset naming, required metadata, dependency rules, content budgets, and project-specific asset invariants | Runtime gameplay behavior |
| Gauntlet | Packaged sessions, devices, launch/exit health, servers plus clients, crash/log monitoring, and platform smoke tests | Creating the build it is asked to exercise |

Keep tests deterministic: control random seeds and time, wait on observable conditions instead of arbitrary sleeps, isolate maps/assets, clean up spawned state, and state the expected network role. Do not let tests depend on a developer’s Editor preferences, writable content, or prior `Saved` state.

## Run locally before CI

Use the executable and flags supported by the active engine and project. A typical non-rendering Automation invocation is:

```text
UnrealEditor-Cmd <Project>.uproject -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests <Filter>;Quit" -ReportExportPath="<Reports>"
```

Remove `-NullRHI` for rendering, screenshot, Slate, input, or GPU-dependent coverage. Run a narrow filter first, then its owning suite. Treat an empty discovery result as failure, not success.

Run asset rules with the Data Validation commandlet and retain its log/report:

```text
UnrealEditor-Cmd <Project>.uproject -run=DataValidation -unattended -nop4
```

Run Functional Tests through the Automation framework in their required map and world context. Use Gauntlet through AutomationTool when a packaged build, device, or multiple roles must be launched; provide it a known build and unique ports/devices for parallel sessions.

## Compose the pipeline

Use UnrealBuildTool for target compilation. Use AutomationTool commands such as `BuildCookRun` to coordinate build, cook, stage, package, archive, and deployment. Use BuildGraph when the pipeline needs a version-controlled dependency graph, reusable nodes, shared artifacts, or build-farm parallelism. Let the CI service schedule machines and publish results; keep UE build semantics in UAT/BuildGraph rather than duplicating them in vendor-specific shell logic.

Apply risk-based gates:

- Pull request: compile affected targets, run fast Automation suites, and validate changed content.
- Mainline: build the supported target/configuration set, run broader Automation/Functional coverage, and perform the representative cook/package smoke test.
- Scheduled/release: exercise packaged builds with Gauntlet, required server/client topologies, devices/platforms, and longer stability suites.

Do not label an Editor build as packaging coverage or a single-process PIE run as replication coverage.

## Make CI reproducible

- Pin the engine source/binary identity, project/plugin revisions, toolchain, and input changelist. Ensure BuildGraph agents use the same inputs.
- Give concurrent jobs separate workspace, report, staging, archive, port, and device allocations. Share only caches designed for concurrency.
- Key derived-data caches by compatible engine/project inputs and treat cache misses as performance events, not correctness failures.
- Publish the exact command, exit code, Automation report, relevant logs, crash data, build receipts, and packaged artifact identity.
- Fail on test failures, crashes, fatal errors, timeouts, missing reports, zero discovered tests, and required validation errors. Do not rely on process exit code alone when the framework report contains failures.
- Retry only classified infrastructure failures. Preserve the first failure evidence; do not hide deterministic failures behind retries.

## Completion gate

After the final change, rerun every invalidated test layer. Report test counts and failures, target/platform/configuration, packaged-build identity where applicable, and any skipped layer with a reason. Never infer device, cook, stage, or network correctness from a narrower passing test.
