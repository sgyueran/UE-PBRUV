import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "checkpoint.py"


class CheckpointTests(unittest.TestCase):
    def test_init_add_source_and_invalidate(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "checkpoint.json"
            subprocess.run([sys.executable, str(SCRIPT), "init", str(path), "--goal", "Ship XR", "--tier", "complex", "--risk", "R2"], check=True, capture_output=True, text=True)
            subprocess.run([sys.executable, str(SCRIPT), "add-source", str(path), "SKILL.md", "120"], check=True, capture_output=True, text=True)
            subprocess.run([sys.executable, str(SCRIPT), "invalidate-evidence", str(path), "files changed"], check=True, capture_output=True, text=True)
            data = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(data["goal"], "Ship XR")
            self.assertEqual(data["loaded_sources"][0]["estimated_tokens"], 120)
            self.assertFalse(data["evidence_fresh"])
            self.assertEqual(data["loaded_token_estimate"], 120)


if __name__ == "__main__":
    unittest.main()
