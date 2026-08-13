#!/usr/bin/env python3
"""Emit a compact, read-only fingerprint of an Unreal Engine project."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1
TARGET_TYPE = re.compile(r"\bTargetType\s*\.\s*([A-Za-z_][A-Za-z0-9_]*)")
CONFIG_PATTERNS = {
    "android": re.compile(r"AndroidRuntimeSettings|\bAndroid\b", re.I),
    "enhanced_input": re.compile(r"EnhancedInput", re.I),
    "gameplay_tags": re.compile(r"GameplayTags?|DefaultGameplayTags", re.I),
    "maps": re.compile(r"GameMapsSettings|GameDefaultMap|EditorStartupMap", re.I),
    "online_subsystem": re.compile(r"OnlineSubsystem", re.I),
    "openxr": re.compile(r"OpenXR", re.I),
    "packaging": re.compile(r"ProjectPackagingSettings|BuildConfiguration", re.I),
}


class InspectionError(ValueError):
    """Raised for an invalid or ambiguous project selection."""


def relative(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def load_descriptor(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise InspectionError(f"cannot read descriptor {path.name}: {exc}") from exc
    if not isinstance(value, dict):
        raise InspectionError(f"descriptor must contain a JSON object: {path.name}")
    return value


def select_uproject(root: Path, requested: Path | None) -> Path:
    if requested is not None:
        selected = requested if requested.is_absolute() else root / requested
        selected = selected.resolve()
        try:
            selected.relative_to(root)
        except ValueError as exc:
            raise InspectionError("--uproject must be inside the project root") from exc
        if not selected.is_file() or selected.suffix.casefold() != ".uproject":
            raise InspectionError(f".uproject not found: {requested}")
        return selected

    candidates = sorted(root.glob("*.uproject"), key=lambda path: path.name.casefold())
    if not candidates:
        raise InspectionError("no .uproject file found")
    if len(candidates) > 1:
        raise InspectionError("multiple .uproject files found; pass --uproject")
    return candidates[0]


def build_rule_files(root: Path) -> list[Path]:
    paths: list[Path] = []
    for directory in (root / "Source", root / "Plugins"):
        if directory.is_dir():
            paths.extend(path for path in directory.rglob("*.Build.cs") if path.is_file())
    return sorted(paths, key=lambda path: relative(path, root).casefold())


def collect_modules(root: Path, descriptor: dict[str, Any]) -> list[dict[str, Any]]:
    rules = build_rule_files(root)
    rule_by_name = {path.name[: -len(".Build.cs")].casefold(): path for path in rules}
    modules: dict[str, dict[str, Any]] = {}

    raw_modules = descriptor.get("Modules", [])
    if isinstance(raw_modules, list):
        for item in raw_modules:
            if not isinstance(item, dict) or not isinstance(item.get("Name"), str):
                continue
            name = item["Name"]
            module = {
                "name": name,
                "type": item.get("Type"),
                "loading_phase": item.get("LoadingPhase"),
                "build_cs": None,
            }
            build_file = rule_by_name.get(name.casefold())
            if build_file is not None:
                module["build_cs"] = relative(build_file, root)
            modules[name.casefold()] = module

    for path in rules:
        name = path.name[: -len(".Build.cs")]
        modules.setdefault(
            name.casefold(),
            {
                "name": name,
                "type": None,
                "loading_phase": None,
                "build_cs": relative(path, root),
            },
        )
    return sorted(modules.values(), key=lambda item: item["name"].casefold())


def collect_targets(root: Path) -> list[dict[str, Any]]:
    source = root / "Source"
    if not source.is_dir():
        return []
    targets: list[dict[str, Any]] = []
    for path in source.rglob("*.Target.cs"):
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8-sig", errors="replace")
        except OSError:
            text = ""
        match = TARGET_TYPE.search(text)
        targets.append(
            {
                "name": path.name[: -len(".Target.cs")],
                "type": match.group(1) if match else None,
                "path": relative(path, root),
            }
        )
    return sorted(targets, key=lambda item: (item["name"].casefold(), item["path"]))


def collect_plugins(root: Path, descriptor: dict[str, Any]) -> list[dict[str, Any]]:
    plugins: dict[str, dict[str, Any]] = {}
    raw_plugins = descriptor.get("Plugins", [])
    if isinstance(raw_plugins, list):
        for item in raw_plugins:
            if not isinstance(item, dict) or not isinstance(item.get("Name"), str):
                continue
            name = item["Name"]
            enabled = item.get("Enabled")
            plugins[name.casefold()] = {
                "name": name,
                "enabled": enabled if isinstance(enabled, bool) else None,
                "source": "project",
            }

    plugin_root = root / "Plugins"
    if plugin_root.is_dir():
        descriptors = sorted(
            (path for path in plugin_root.rglob("*.uplugin") if path.is_file()),
            key=lambda path: relative(path, root).casefold(),
        )
        for path in descriptors:
            name = path.stem
            key = name.casefold()
            if key in plugins:
                plugins[key]["descriptor"] = relative(path, root)
            else:
                plugins[key] = {
                    "name": name,
                    "enabled": None,
                    "source": "local",
                    "descriptor": relative(path, root),
                }
    return sorted(plugins.values(), key=lambda item: item["name"].casefold())


def collect_config(root: Path) -> dict[str, list[str]]:
    config_root = root / "Config"
    paths = (
        sorted(config_root.rglob("*.ini"), key=lambda path: relative(path, root).casefold())
        if config_root.is_dir()
        else []
    )
    indicators: set[str] = set()
    for path in paths:
        try:
            text = f"{path.name}\n{path.read_text(encoding='utf-8-sig', errors='replace')}"
        except OSError:
            text = path.name
        for name, pattern in CONFIG_PATTERNS.items():
            if pattern.search(text):
                indicators.add(name)
    return {
        "files": [relative(path, root) for path in paths],
        "indicators": sorted(indicators),
    }


def collect_source_control_hints(root: Path) -> list[str]:
    checks = {
        "git": [root / ".git"],
        "gitignore": [root / ".gitignore", root / ".gitattributes"],
        "mercurial": [root / ".hg"],
        "perforce": [root / ".p4config", root / ".p4ignore"],
        "plastic": [root / ".plastic", root / "plastic.workspace"],
        "svn": [root / ".svn"],
    }
    return sorted(name for name, candidates in checks.items() if any(path.exists() for path in candidates))


def inspect_project(project_root: Path, uproject: Path | None = None) -> dict[str, Any]:
    root = project_root.resolve()
    if not root.is_dir():
        raise InspectionError(f"project root is not a directory: {project_root}")
    descriptor_path = select_uproject(root, uproject)
    descriptor = load_descriptor(descriptor_path)
    association = descriptor.get("EngineAssociation")
    return {
        "schema_version": SCHEMA_VERSION,
        "project_root": str(root),
        "uproject": {
            "path": relative(descriptor_path, root),
            "name": descriptor_path.stem,
            "engine_association": association if isinstance(association, str) else None,
        },
        "modules": collect_modules(root, descriptor),
        "targets": collect_targets(root),
        "plugins": collect_plugins(root, descriptor),
        "config": collect_config(root),
        "source_control_hints": collect_source_control_hints(root),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project_root", nargs="?", type=Path, default=Path.cwd())
    parser.add_argument("--uproject", type=Path)
    args = parser.parse_args()
    try:
        result = inspect_project(args.project_root, args.uproject)
    except InspectionError as exc:
        print(json.dumps({"error": str(exc)}, ensure_ascii=False, separators=(",", ":")), file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
