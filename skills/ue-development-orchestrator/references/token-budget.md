# Token and context budget

Use these limits to control context growth. They are planning estimates, not exact provider telemetry. They become operational only when the checkpoint's loaded-source ledger is maintained; otherwise report them as guidance, not enforced caps.

## Budget tiers

| Tier | Typical work | Per-phase context target | Loading rule |
|---|---|---:|---|
| Small | Explanation, one config edit, narrow diagnosis | 2,500 estimated tokens | Core Skill, project evidence, at most one focused reference section. |
| Standard | One feature or bug spanning a module and related assets | 6,000 estimated tokens | Relevant specialist Skill plus only the references required by the route. |
| Complex | Cross-module, networked, async, packaging, GAS, or mixed C++/Blueprint work | 10,000 estimated tokens | Split into checkpoints; never load all specialist references at once. |

If the user supplies an explicit token limit, it overrides these targets. Preserve safety, required validation, and user requirements; reduce scope or request a continuation rather than skipping critical checks.

## Stage allocation

Use the selected tier as a soft envelope:

- Discovery and scope: 15%.
- Design and decisions: 15%.
- Implementation and diagnostics: 45%.
- Review, verification, and handoff: reserve 25%.

Do not consume the review reserve to explore optional enhancements.

## Reading discipline

1. Inventory before reading: list files, Skills, modules, assets, and logs.
2. Search before opening: locate symbols, errors, configuration keys, or relevant headings.
3. Read narrow windows around matches; expand only when dependencies require it.
4. Load reference documents one at a time and only for an active decision.
5. Reuse the working ledger instead of rereading unchanged sources.
6. Summarize tool output immediately: retain exact errors, paths, counts, and verification results; discard noise.
7. Do not load `.full`, backup, generated, cache, Intermediate, DerivedDataCache, or unrelated Saved logs unless the task specifically requires them.
8. Record each loaded Skill, file, section, or log excerpt in the checkpoint with an estimated token count; use the sum as the budget proxy.

## Checkpoints

At roughly 70% of the selected envelope, persist or refresh the canonical checkpoint with `scripts/checkpoint.py`. Do not maintain a second competing ledger.

At roughly 85%, stop expanding scope. Finish the smallest safe implementation, run required review gates, and hand off remaining optional work.

At the cap, do not start a new subsystem or speculative investigation. Preserve the ledger and request a continuation if essential work remains.

## Output discipline

- Lead with outcome and evidence.
- Do not repeat the plan, tool transcript, and final summary in full.
- Quote only the error lines or API signatures needed to support a decision.
- Use tables only when they compress repeated mappings.
- Keep optional rationale behind a short “why” section or provide it on request.

## Skill file budgets

- `SKILL.md`: target 1,200 estimated tokens; hard cap 1,800; hard cap 250 lines.
- Each reference: target 2,000 estimated tokens; hard cap 3,000.
- All Markdown in this Skill: hard cap 7,500 estimated tokens.
- Frontmatter description: target at most 500 characters.

Run `scripts/audit_skill_budget.py` after modifying the Skill.

## Borrowed patterns

- Anthropic Skills: three-level progressive disclosure—metadata, core Skill, then on-demand resources.
- obra/superpowers: keep frequently loaded Skills short; use evidence before completion claims and staged reviews.
- spec-kit-token-budget: define phase scopes, preserve information-bearing requirements/contracts, compress reversibly, and support concise output with explicit user override.
