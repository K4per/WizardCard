import runpy
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path


SCRIPT = Path(__file__).with_name("text_lint")


class TextLintTests(unittest.TestCase):
    def test_source_literals_only(self):
        find = runpy.run_path(str(SCRIPT))["chinese_literals"]
        self.assertEqual(list(find('const char *s = "名字"; // "注释"\n/* "注释" */', ".cpp")), ["名字"])
        self.assertEqual(list(find('push_warning("日志")\nbutton.text = "按钮" # "注释"', ".gd")), ["按钮"])
        self.assertEqual(list(find('[node name="A"]\ntext = "文字"\n; "注释"', ".tscn")), ["文字"])

    def test_index_worktree_and_new_files(self):
        with tempfile.TemporaryDirectory() as temp, patch.dict(os.environ, {"GIT_CONFIG_COUNT": "0"}):
            work = Path(temp)
            for args in (["init", "-q"], ["config", "user.email", "test@example.invalid"],
                         ["config", "user.name", "Tester"]):
                subprocess.run(["git", *args], cwd=work, check=True)
            file = work / "old.gd"
            file.write_text('var one = "旧"\n', encoding="utf-8")
            subprocess.run(["git", "add", "."], cwd=work, check=True)
            subprocess.run(["git", "commit", "-qm", "base"], cwd=work, check=True)
            file.write_text('var renamed = "旧"\n', encoding="utf-8")
            new = work / "new.gd"
            new.write_text('var two = "新增"\n', encoding="utf-8")
            def run():
                return subprocess.run([sys.executable, str(SCRIPT)], cwd=work, capture_output=True)
            self.assertEqual(run().returncode, 1)
            subprocess.run(["git", "add", "."], cwd=work, check=True)
            self.assertEqual(run().returncode, 1)
            new.write_text('var two = "label"\n', encoding="utf-8")
            self.assertEqual(run().returncode, 0)


if __name__ == "__main__":
    unittest.main()

