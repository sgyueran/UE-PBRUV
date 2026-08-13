from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "inspect_ue_project.py"


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def snapshot(root: Path) -> dict[str, bytes | None]:
    return {
        path.relative_to(root).as_posix(): path.read_bytes() if path.is_file() else None
        for path in sorted(root.rglob("*"))
    }


def make_project(root: Path) -> None:
    write(
        root / "Ban.uproject",
        json.dumps(
            {
                "FileVersion": 3,
                "EngineAssociation": "5.4",
                "Modules": [
                    {"Name": "Ban", "Type": "Runtime", "LoadingPhase": "Default"}
                ],
                "Plugins": [
                    {"Name": "EnhancedInput", "Enabled": True},
                    {"Name": "OpenXR", "Enabled": False},
                ],
            }
        ),
    )
    write(root / "Source" / "Ban" / "Ban.Build.cs", "// module rules\n")
    write(
        root / "Source" / "Ban.Target.cs",
        "public class BanTarget { Type = TargetType.Game; }\n",
    )
    write(
        root / "Source" / "BanEditor.Target.cs",
        "public class BanEditorTarget { Type = TargetType.Editor; }\n",
    )
    write(
        root / "Plugins" / "LocalTools" / "LocalTools.uplugin",
        json.dumps(
            {
                "FileVersion": 3,
                "FriendlyName": "Local Tools",
                "Modules": [{"Name": "LocalTools", "Type": "Editor"}],
            }
        ),
    )
    write(
        root / "Config" / "DefaultEngine.ini",
        "[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]\n"
        "[/Script/EngineSettings.GameMapsSettings]\n",
    )
    write(
        root / "Config" / "DefaultGameplayTags.ini",
        "[/Script/GameplayTags.GameplayTagsSettings]\n",
    )
    (root / ".git").mkdir()
    write(root / ".gitignore", "Binaries/\n")
    write(root / ".p4config", "P4PORT=perforce:1666\n")


class InspectUeProjectTests(unittest.TestCase):
    def run_script(self, root: Path, *args: str) -> subprocess.CompletedProcess[str]:
        self.assertTrue(SCRIPT.is_file(), f"missing script: {SCRIPT}")
        return subprocess.run(
            [sys.executable, str(SCRIPT), str(root), *args],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_emits_compact_read_only_project_fingerprint(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            make_project(root)
            before = snapshot(root)

            result = self.run_script(root)

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, "")
            self.assertEqual(result.stdout.count("\n"), 1)
            payload = json.loads(result.stdout)
            self.assertEqual(payload["schema_version"], 1)
            self.assertEqual(payload["project_root"], str(root.resolve()))
            self.assertEqual(
                payload["uproject"],
                {
                    "path": "Ban.uproject",
                    "name": "Ban",
                    "engine_association": "5.4",
                },
            )
            self.assertEqual(
                payload["modules"],
                [
                    {
                        "name": "Ban",
                        "type": "Runtime",
                        "loading_phase": "Default",
                        "build_cs": "Source/Ban/Ban.Build.cs",
                    }
                ],
            )
            self.assertEqual(
                payload["targets"],
                [
                    {"name": "Ban", "type": "Game", "path": "Source/Ban.Target.cs"},
                    {
                        "name": "BanEditor",
                        "type": "Editor",
                        "path": "Source/BanEditor.Target.cs",
                    },
                ],
            )
            self.assertEqual(
                payload["plugins"],
                [
                    {"name": "EnhancedInput", "enabled": True, "source": "project"},
                    {
                        "name": "LocalTools",
                        "enabled": None,
                        "source": "local",
                        "descriptor": "Plugins/LocalTools/LocalTools.uplugin",
                    },
                    {"name": "OpenXR", "enabled": False, "source": "project"},
                ],
            )
            self.assertEqual(
                payload["config"],
                {
                    "files": [
                        "Config/DefaultEngine.ini",
                        "Config/DefaultGameplayTags.ini",
                    ],
                    "indicators": ["android", "gameplay_tags", "maps"],
                },
            )
            self.assertEqual(
                payload["source_control_hints"], ["git", "gitignore", "perforce"]
            )
            self.assertEqual(snapshot(root), before)

    def test_requires_an_unambiguous_uproject(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root / "One.uproject", "{}")
            write(root / "Two.uproject", "{}")

            result = self.run_script(root)

            self.assertEqual(result.returncode, 2)
            error = json.loads(result.stderr)
            self.assertEqual(
                error["error"], "multiple .uproject files found; pass --uproject"
            )

    def test_accepts_explicit_uproject_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root / "One.uproject", json.dumps({"EngineAssociation": "5.3"}))
            write(root / "Two.uproject", json.dumps({"EngineAssociation": "5.4"}))

            result = self.run_script(root, "--uproject", "Two.uproject")

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                json.loads(result.stdout)["uproject"]["engine_association"], "5.4"
            )


if __name__ == "__main__":
    unittest.main()
