# Asset source control and save safety

Treat `.uasset` and `.umap` packages as binary, dependency-bearing records. Coordinate ownership before editing and save only packages intentionally changed.

## Establish repository rules

- Track project descriptors, `Config/`, `Source/`, project plugins, content, and required source art. Exclude generated caches and per-user state according to the project’s established policy.
- Configure repository rules before bulk-adding content. Do not rewrite history or migrate existing binaries without a reviewed migration and rollback plan.
- Verify the workspace is at the intended revision and the Editor is not holding stale packages before external sync operations.

For Perforce, configure the project typemap so Unreal packages are binary and use the team’s exclusive-lock policy. Work inside the correct client/stream, sync first, check out packages before mutation, and keep related work in one reviewed changelist. Prefer Editor-integrated checkout/status for assets; close the Editor before a broad external sync.

For Git, configure Git LFS before adding Unreal packages. A minimal policy commonly includes:

```gitattributes
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap   filter=lfs diff=lfs merge=lfs -text
```

Add project-specific large source formats deliberately. Verify staged files are LFS pointers and that CI can fetch the corresponding objects. LFS tracking is not retroactive; use file locking when the hosting workflow supports it, because LFS storage does not make binary packages mergeable.

## Perform an exact save

1. Resolve the exact package paths, source-control state, and dependent packages before mutation.
2. Check out or lock only the intended packages. Include packages that a rename, redirector fixup, Blueprint compile, or OFPA edit will legitimately modify.
3. Apply editor mutations in a transaction when supported.
4. Compile or validate the changed asset, then save the explicit package set. Do not use Save All to conceal an unknown dirty-state set.
5. Re-query package state and inspect the source-control changelist/staging area. Confirm every changed file maps to the intended asset or dependency.
6. Revert an unintended file only by exact verified path and only after confirming it contains no user work. Never use a broad revert as cleanup.

Autosaves, recovery files, Derived Data Cache, `Intermediate`, and most `Saved` content are not substitutes for submitted source assets.

## Handle OFPA changelists

One File Per Actor stores actor instances in external packages so collaborators can edit different actors with less map contention. Do not manually rename or interpret encoded external-actor filenames.

- Inspect OFPA changes through the Editor’s changelist/source-control view so filenames resolve to actor and level identities.
- Submit the coherent set: changed actors, level/world-setting changes, new referenced assets, external objects, redirectors, and deletions.
- Remember that changing level settings or non-externalized data can still modify the main map.
- Load the level at the submitted revision and validate references before handing off. A partial external-actor submission can leave dangling references even when each binary file exists.

## Move, rename, and redirect safely

Move or rename assets through the Content Browser so Unreal can update references and create asset redirectors. Fix redirectors only in the bounded folders involved, with all referenced packages available and writable. Review every package resaved and every redirector deleted before submit; do not run a project-wide fixup as routine cleanup.

Distinguish asset redirectors from Core Redirects. Use Core Redirects for serialized C++ type/member renames, then load, validate, and deliberately resave affected assets. Remove a redirect only after all supported content and branches no longer require it; premature removal can lose serialized references.

## Submit gate

- Review checked-out, added, modified, deleted, and untracked files; exclude credentials, local settings, logs, and generated output.
- Verify new assets include required dependencies and source files, and confirm locks are not held accidentally after submit.
- Run the narrow asset validation/load test and, for cook-sensitive changes, the representative cook.
- Record the changelist/commit and the exact assets tested. Do not claim a binary merge or redirector cleanup succeeded without loading the resulting assets.
