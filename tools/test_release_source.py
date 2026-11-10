"""Verify release inputs remain portable when exported from a Windows checkout."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import release_source


class ReleaseSourceTests(unittest.TestCase):
    def test_export_preserves_linux_script_and_binary_bytes_with_crlf_git_settings(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(["git", *args], cwd=root, stderr=subprocess.STDOUT)

            git("init", "-q")
            git("config", "core.autocrlf", "true")
            git("config", "core.eol", "crlf")
            (root / ".gitattributes").write_bytes(b"* text=auto\n*.bin -text\n")
            (root / "deploy").mkdir()
            script = b"#!/bin/sh\nset -eu\nprintf 'ready\\n'\n"
            binary = b"\x00\x01\r\n\x02"
            (root / "deploy/start.sh").write_bytes(script)
            (root / "deploy/fixture.bin").write_bytes(binary)
            git("add", ".")
            git("-c", "user.name=Release Test", "-c", "user.email=release-test@example.invalid",
                "commit", "-qm", "fixture")
            output = root / "inputs.tar.gz"
            with patch.object(release_source, "ROOT", root):
                release_source.export(output)
            with tarfile.open(output) as archive:
                actual = archive.extractfile("deploy/start.sh").read()
                self.assertEqual(actual, script, "Linux shell input must retain committed LF bytes")
                self.assertEqual(archive.extractfile("deploy/fixture.bin").read(), binary)
                manifest = json.load(archive.extractfile("SOURCE.json"))
                self.assertEqual(manifest["files"]["deploy/start.sh"], hashlib.sha256(script).hexdigest())


if __name__ == "__main__":
    unittest.main()
