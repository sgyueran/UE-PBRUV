#!/usr/bin/env python3
"""Maintain one durable, compact UE task checkpoint."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def save(path: Path, data: dict) -> None:
    data["updated_at"] = datetime.now(timezone.utc).isoformat()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    init = commands.add_parser("init")
    init.add_argument("path", type=Path)
    init.add_argument("--goal", required=True)
    init.add_argument("--tier", choices=("small", "standard", "complex"), required=True)
    init.add_argument("--risk", choices=("R0", "R1", "R2", "R3"), required=True)
    show = commands.add_parser("show")
    show.add_argument("path", type=Path)
    source = commands.add_parser("add-source")
    source.add_argument("path", type=Path)
    source.add_argument("source")
    source.add_argument("estimated_tokens", type=int)
    invalidate = commands.add_parser("invalidate-evidence")
    invalidate.add_argument("path", type=Path)
    invalidate.add_argument("reason")
    args = parser.parse_args()

    if args.command == "init":
        data = {"goal": args.goal, "tier": args.tier, "risk": args.risk,
                "project": {}, "baseline_revision": "", "requirements": [],
                "approvals": [], "loaded_sources": [], "loaded_token_estimate": 0,
                "edits": [], "evidence": [], "evidence_fresh": True,
                "open_risks": [], "next_action": ""}
        save(args.path, data)
    elif args.command == "show":
        print(json.dumps(load(args.path), ensure_ascii=False, indent=2))
        return 0
    elif args.command == "add-source":
        data = load(args.path)
        data["loaded_sources"].append({"source": args.source, "estimated_tokens": args.estimated_tokens})
        data["loaded_token_estimate"] = sum(item["estimated_tokens"] for item in data["loaded_sources"])
        save(args.path, data)
    else:
        data = load(args.path)
        data["evidence_fresh"] = False
        data.setdefault("evidence_invalidations", []).append(args.reason)
        save(args.path, data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
