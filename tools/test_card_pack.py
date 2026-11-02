"""Card pack regression tests; use the current built native content validator."""
import base64
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

import card_pack

VALIDATOR = Path(os.environ.get("WIZARDCARD_CLI", card_pack.ROOT / "build/windows/Release/wizard_cli.exe"))
PNG = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=")


class CardPackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.card = json.loads((card_pack.ROOT / "assets/cards/analytic/fireball.json").read_text(encoding="utf-8-sig"))
        self.base = {"id": "test-base", "name": "Test base", "rarity": "common", "type": "formation",
                     "number": "010999", "level": 1, "body": 1, "rings": 1, "capacity": 4, "income": 1,
                     "text": "Base formation for a self-contained test pool"}
        self.metadata = {"rulesVersion": "2.5.0", "cardSetVersion": "2.5.0", "mechanicsRevision": 3, "baseLoadCapacity": 2}
        self.write_source(self.source)
        self.package = self.root / "test.cardpack.json"
        self.pack = card_pack.export_pack(self.source, self.package, None, None)
        self.target = self.root / "target"

    def write_source(self, target):
        (target / "cards/analytic").mkdir(parents=True, exist_ok=True)
        (target / "cards/analytic/fireball.json").write_bytes(card_pack.encoded(self.card))
        (target / "cards/formation").mkdir()
        (target / "cards/formation/test-base.json").write_bytes(card_pack.encoded(self.base))
        (target / "catalog.json").write_bytes(card_pack.encoded({"format": 1, "metadata": self.metadata,
            "definitions": [{"id": "fireball", "file": "cards/analytic/fireball.json"},
                            {"id": "test-base", "file": "cards/formation/test-base.json"}]}))
        (target / "card-pack-resources").mkdir()
        (target / "card-pack-resources/fireball.png").write_bytes(PNG)
        (target / "card-pack-resources/test-base.png").write_bytes(PNG)
        (target / "card-pack-resources.json").write_bytes(card_pack.encoded({"format": 1, "resources": [{
            "id": "illustration." + card_id, "file": "card-pack-resources/" + card_id + ".png", "sha256": hashlib.sha256(PNG).hexdigest()}
            for card_id in ("fireball", "test-base")]}))

    def save(self, pack):
        self.package.write_bytes(card_pack.encoded(pack))

    def snapshot(self):
        return {p.relative_to(self.target).as_posix(): p.read_bytes() for p in self.target.rglob("*") if p.is_file()} if self.target.exists() else {}

    def invalid(self, pack, message):
        self.save(pack)
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, message):
            card_pack.import_pack(self.package, self.target, VALIDATOR)
        self.assertEqual(before, self.snapshot())

    def test_deterministic_export(self):
        other = self.root / "again.json"
        card_pack.export_pack(self.source, other, ["test-base", "fireball"], None)
        self.assertEqual(self.package.read_bytes(), other.read_bytes())

    def test_real_native_roundtrip_and_explicit_overwrite(self):
        self.assertEqual(card_pack.import_pack(self.package, self.target, VALIDATOR), 2)
        self.assertEqual(card_pack.load_catalog(self.source), card_pack.load_catalog(self.target))
        output = self.root / "roundtrip.json"
        card_pack.export_pack(self.target, output, None, None)
        self.assertEqual(self.package.read_bytes(), output.read_bytes())
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, "overwrite"):
            card_pack.import_pack(self.package, self.target, VALIDATOR)
        self.assertEqual(before, self.snapshot())
        card_pack.import_pack(self.package, self.target, VALIDATOR, True)
        self.assertEqual(before, self.snapshot())

    def test_reject_version_and_duplicate_json_keys(self):
        pack = copy.deepcopy(self.pack); pack["version"] = 99
        self.invalid(pack, "version")
        self.package.write_text('{"version":1,"version":2}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "duplicate JSON key"):
            card_pack.import_pack(self.package, self.target, VALIDATOR)
        self.assertFalse(self.target.exists())

    def test_reject_paths_and_resource_hashes(self):
        for path in ("../outside.png", "/absolute.png", "C:/absolute.png", "card-pack-resources/../outside.png", "card-pack-resources\\escape.png"):
            pack = copy.deepcopy(self.pack); pack["resources"][0]["file"] = path
            self.invalid(pack, "path")
        pack = copy.deepcopy(self.pack); pack["resources"][0]["sha256"] = "0" * 64
        self.invalid(pack, "SHA-256")
        pack = copy.deepcopy(self.pack); pack["resources"] = []
        self.invalid(pack, "missing")
        pack = copy.deepcopy(self.pack); pack["resources"].append(copy.deepcopy(pack["resources"][0]))
        self.invalid(pack, "duplicate")

    def test_reject_duplicate_ids_numbers_and_fields(self):
        pack = copy.deepcopy(self.pack); pack["cards"].append(copy.deepcopy(pack["cards"][0]))
        self.invalid(pack, "duplicate ID")
        pack["cards"][-1]["id"] = "another-fireball"
        self.invalid(pack, "duplicate number")
        pack = copy.deepcopy(self.pack); pack["cards"][0]["unknownImplementation"] = True
        self.invalid(pack, "unsupported fields")
        pack = copy.deepcopy(self.pack); pack["cards"][0]["effects"][0]["unimplemented"] = True
        self.invalid(pack, "unsupported fields")

    def test_native_rejects_unknown_effect_and_invalid_number_before_writes(self):
        self.write_source(self.target)
        pack = copy.deepcopy(self.pack)
        pack["cards"] = [pack["cards"][0]]
        pack["resources"] = [entry for entry in pack["resources"] if entry["id"] == "illustration.fireball"]
        pack["cards"][0]["id"] = "new-card"
        pack["cards"][0]["number"] = "020999"
        pack["cards"][0]["effects"][0]["kind"] = "unsupported_effect"
        self.invalid(pack, "unknown value")
        pack["cards"][0]["effects"][0]["kind"] = "damage"
        pack["cards"][0]["cost"] = -1
        self.invalid(pack, "integer")

    def test_incompatible_metadata_preserves_target(self):
        self.write_source(self.target)
        pack = copy.deepcopy(self.pack); pack["pool"]["rulesVersion"] = "999"
        self.invalid(pack, "incompatible")

    def test_commit_failure_rolls_back_every_file(self):
        self.write_source(self.target)
        before = self.snapshot()
        actual = card_pack.os.replace
        count = 0
        def fail_once(source, destination):
            nonlocal count
            count += 1
            if count == 3:
                raise OSError("simulated disk failure")
            return actual(source, destination)
        with patch.object(card_pack.os, "replace", side_effect=fail_once):
            with self.assertRaisesRegex(OSError, "disk failure"):
                card_pack.import_pack(self.package, self.target, VALIDATOR, True)
        self.assertEqual(before, self.snapshot())


if __name__ == "__main__":
    unittest.main()
