# Build, cook, and packaging diagnostics

Diagnose the first failing phase before changing code or deleting generated data. Record the exact target, platform, configuration, engine association, command, exit code, and first causal error. Treat later errors as fallout until proven otherwise.

## Locate the failing boundary

| Boundary | Typical evidence | Inspect first |
|---|---|---|
| UHT / reflection | Generated-code or header-tool error before C++ compilation | The first named header, reflection macro placement, generated-header include, reflected types, and module visibility |
| UBT / compile | Compiler error or rules/target failure | The translation unit, include ownership, platform guards, target rules, and public/private module dependencies |
| Link | Unresolved external, duplicate symbol, or missing import library | Declaration/definition match, API export macro, owning module dependency, library architecture, and configuration |
| Module load | Editor or executable starts but rejects a module/plugin | Descriptor type/loading phase, dependency chain, binary freshness, supported target/platform, and `StartupModule` log |
| Cook | Asset load, serialization, shader, or package failure | The first failing package, redirectors, editor-only references, dynamic asset discovery, and target-platform data |
| Stage/package | Cook succeeds but files cannot be staged, signed, archived, or launched | Build receipt, runtime dependencies, staged path, third-party binaries, permissions, signing, and deployment logs |

## Triage by phase

### UHT and compile

- Fix the earliest header-tool diagnostic; generated C++ errors often cascade from it.
- Keep the generated-header include in the position required by the project’s engine version and inspect the declaration immediately around the reported macro.
- Verify that reflected types use supported declarations and that referenced reflected types are visible to the owning module.
- For compiler failures, identify which module owns the missing header or symbol. Add the narrow dependency to the correct public or private list; do not add broad modules merely to silence errors.
- Compare the failing target with the target that works. Editor success does not prove a Game, Server, Client, or platform target compiles.

### Link and module load

- For unresolved externals, match namespace, signature, qualifiers, template instantiation, and build guards between declaration and definition.
- Export cross-module types/functions with the owning module’s API macro and link the consumer to that module.
- For duplicate symbols, find multiple definitions or a non-inline definition placed in a header.
- For third-party libraries, match target architecture, runtime, configuration, delay-load choice, and staged runtime files.
- For module-load failures, inspect the complete dependency error chain and the module’s earliest startup log. Verify `.uproject`, `.uplugin`, `.Build.cs`, and target rules agree about availability and loading.

### Cook, stage, and package

- Search backward from the final AutomationTool failure to the first package, shader, serialization, or file error.
- Load the named asset and inspect its dependencies. Fix missing references and redirectors in a bounded content area, then resave only affected packages.
- Register assets loaded only by string/path or runtime convention through the project’s asset-management or cook rules; do not assume Editor discovery implies cook inclusion.
- Separate cook evidence from stage evidence. A successful cook does not validate receipts, runtime dependencies, signing, archive paths, or device deployment.
- Re-run the same target/configuration after the fix and preserve the relevant log and artifact path.

## Respect the Live Coding boundary

Use Live Coding for small `.cpp` function-body iterations when class layout, reflection metadata, module rules, serialization, and defaults remain unchanged.

Close the Editor and perform a normal build after changing reflected declarations or signatures, headers that affect layout, constructors/default subobjects, module or target rules, descriptors, source-file membership, exported APIs, or serialized defaults. Also use a normal build when adding/removing modules or when reinstancing produces suspicious objects. Existing instances may retain old constructor defaults even when a patch succeeds.

Never treat a Live Coding success as proof of a clean build, another target, cook, or package.

## Escalate safely

- Reproduce with the project’s normal build or AutomationTool command outside Live Coding and retain full logs.
- Compare working and failing targets, machines, or changelists before changing caches.
- Do not delete `Intermediate`, `Binaries`, `Saved`, or Derived Data Cache by default. Use cleanup only after evidence identifies stale generated state, the exact paths are verified, the Editor is closed, and the operation is explicitly authorized.
- After the final change, prove the affected target builds and, when relevant, the intended target/configuration cooks and stages. Report warnings separately from failures and disclose any phase not run.
