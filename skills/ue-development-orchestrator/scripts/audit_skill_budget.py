#!/usr/bin/env python3
"""Audit ue-development-orchestrator structure and estimated context budget."""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path


CORE_REFERENCES = {
    "references/capability-map.md",
    "references/token-budget.md",
    "references/review-protocol.md",
}
HARD_SKILL_TOKENS = 1800
HARD_SKILL_LINES = 250
HARD_REFERENCE_TOKENS = 3000
HARD_TOTAL_TOKENS = 7500


def estimate_tokens(text: str) -> int:
    cjk = len(re.findall(r"[\u3400-\u9fff\uf900-\ufaff]", text))
    other = len(re.sub(r"[\s\u3400-\u9fff\uf900-\ufaff]", "", text))
    return cjk + math.ceil(other / 4)


def frontmatter(text: str) -> tuple[dict[str, str], str]:
    match = re.match(r"\A---\s*\n(.*?)\n---\s*\n(.*)\Z", text, re.S)
    if not match:
        return {}, text
    values: dict[str, str] = {}
    for line in match.group(1).splitlines():
        if ":" in line:
            key, value = line.split(":", 1)
            values[key.strip()] = value.strip()
    return values, match.group(2)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    errors: list[str] = []
    warnings: list[str] = []
    metrics: dict[str, dict[str, int]] = {}

    skill_path = root / "SKILL.md"
    if not skill_path.is_file():
        errors.append("SKILL.md is missing")
    else:
        skill_text = skill_path.read_text(encoding="utf-8")
        meta, body = frontmatter(skill_text)
        if set(meta) != {"name", "description"}:
            errors.append(f"frontmatter keys must be name and description only: {sorted(meta)}")
        if meta.get("name") != root.name:
            errors.append("frontmatter name must match the skill directory")
        if not meta.get("description"):
            errors.append("frontmatter description is missing")
        elif len(meta["description"]) > 500:
            warnings.append("frontmatter description exceeds 500 characters")

        body_tokens = estimate_tokens(body)
        lines = len(skill_text.splitlines())
        metrics["SKILL.md"] = {"estimated_tokens": body_tokens, "lines": lines}
        if body_tokens > HARD_SKILL_TOKENS:
            errors.append(f"SKILL.md body exceeds {HARD_SKILL_TOKENS} estimated tokens: {body_tokens}")
        if lines > HARD_SKILL_LINES:
            errors.append(f"SKILL.md exceeds {HARD_SKILL_LINES} lines: {lines}")
        if re.search(r"\b(?:TODO|TBD)\b|\[TODO", skill_text, re.I):
            errors.append("SKILL.md contains a TODO/TBD placeholder")

        for link in re.findall(r"\]\((?!https?://|file:)([^)#]+)", skill_text):
            if not (root / link).exists():
                errors.append(f"broken relative link: {link}")

    for relative in sorted(CORE_REFERENCES):
        if not (root / relative).is_file():
            errors.append(f"required reference is missing: {relative}")

    markdown_files = sorted(root.rglob("*.md"))
    total_tokens = 0
    for path in markdown_files:
        relative = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8")
        tokens = estimate_tokens(text)
        total_tokens += tokens
        metrics.setdefault(relative, {"estimated_tokens": tokens, "lines": len(text.splitlines())})
        if relative.startswith("references/") and tokens > HARD_REFERENCE_TOKENS:
            errors.append(f"reference exceeds {HARD_REFERENCE_TOKENS} estimated tokens: {relative} ({tokens})")
        if re.search(r"\b(?:TODO|TBD)\b|\[TODO", text, re.I):
            errors.append(f"placeholder found: {relative}")

    if total_tokens > HARD_TOTAL_TOKENS:
        errors.append(f"all Markdown exceeds {HARD_TOTAL_TOKENS} estimated tokens: {total_tokens}")

    result = {
        "root": str(root),
        "status": "pass" if not errors else "fail",
        "estimated_total_tokens": total_tokens,
        "metrics": metrics,
        "warnings": warnings,
        "errors": errors,
    }
    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        print(f"status: {result['status']}")
        print(f"estimated_total_tokens: {total_tokens}")
        for name, values in metrics.items():
            print(f"{name}: {values['estimated_tokens']} tokens, {values['lines']} lines")
        for warning in warnings:
            print(f"WARNING: {warning}")
        for error in errors:
            print(f"ERROR: {error}")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
