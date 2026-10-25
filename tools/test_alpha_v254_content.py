"""Check source coverage, stable migration and supplied-art integrity for 2.5.5."""
from __future__ import annotations

from collections import Counter
import json
import unittest

from catalog_source import load_catalog
import import_alpha_v254_content as migration


class Alpha254ContentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = load_catalog()
        cls.cards = {card["id"]: card for card in cls.catalog["cards"]}
        cls.previous = {card["id"]: card for card in migration.read_json(migration.SNAPSHOT / "previous-catalog.json")["cards"]}
        cls.rows = migration.parse_cards()

    def test_complete_source_coverage_and_stable_legacy_identities(self):
        self.assertEqual(Counter(row["source"] for row in self.rows), Counter(migration.EXPECTED_COUNTS))
        resolved = [migration.resolve_id(row, self.cards) for row in self.rows]
        self.assertEqual(len(resolved), len(set(resolved)))
        self.assertEqual(set(resolved) - migration.PENDING, set(self.cards))
        self.assertEqual(len(self.cards), 135)
        self.assertEqual(set(self.cards) - self.previous.keys(), set(migration.NEW_IDS.values()))
        self.assertTrue(self.previous.keys() <= self.cards.keys())
        for card_id, old in self.previous.items():
            with self.subTest(card_id=card_id):
                self.assertEqual(self.cards[card_id]["number"], old["number"])
                # The 2.5.5 brief explicitly replaces the earthquake artwork;
                # the card identity remains stable while its illustration ID changes.
                if card_id == "earthquake":
                    self.assertEqual(self.cards[card_id]["illustrationId"], "earthquake")
                else:
                    self.assertEqual(self.cards[card_id]["illustrationId"], old["illustrationId"])

    def test_explicit_renames_and_source_corrections(self):
        self.assertEqual(self.cards["instantly-sleep"]["name"], "沉默术")
        self.assertEqual(self.cards["engeas-four-point-star"]["name"], "恩格亚斯之星")
        self.assertEqual(self.cards["ring"]["name"], "奥术圆环")
        self.assertEqual(self.cards["sique-divine-stew"]["speed"], 2)
        for card_id in ("sique-roast", "sique-blowfire"):
            self.assertEqual(self.cards[card_id]["school"], "evocation")
        for card_id in ("blood-blade", "blood-debt", "blood-repayment", "blood-rain"):
            self.assertIn("暗蚀伤害", self.cards[card_id]["text"])
        self.assertIn("1至2张", self.cards["cloud-mist"]["text"])
        self.assertIn("1至5张", self.cards["sique-refire"]["text"])
        self.assertIn("1至5张", self.cards["sique-backfire"]["text"])
        self.assertIn("【七星星移】", self.cards["qixing-tianxuan"]["text"])
        self.assertNotIn("早成", self.cards["qixing-ruin"]["text"])
        for card in self.cards.values():
            self.assertNotIn("恒风", card["name"] + card["text"])
            self.assertNotIn("昏昏入睡", card["name"] + card["text"])
        guidance = self.cards["storm-guidance"]["text"]
        self.assertIn("【云雾术】/【召雷术】", guidance)
        self.assertNotIn("【沉默术】", guidance)
        self.assertFalse(migration.PENDING & self.cards.keys())

    def test_every_new_card_has_structured_capabilities_and_unique_numbers(self):
        self.assertEqual(len({card["number"] for card in self.cards.values()}), len(self.cards))
        for card_id in migration.NEW_IDS.values():
            card = self.cards[card_id]
            with self.subTest(card_id=card_id):
                # Tianshu's common aspect provider is recognized by series and
                # formation type; other new mechanics have explicit capabilities.
                self.assertTrue(card_id == "qixing-tianshu" or card.get("effects") or card.get("traits") or card.get("recipe"))
                self.assertEqual(card["tags"], ["七星" if card_id.startswith("qixing-") else "血契"])
                self.assertTrue(card["number"].startswith(migration.PREFIXES[card["type"]]))
        self.assertEqual(self.catalog["rulesVersion"], "2.5.5")
        self.assertEqual(self.catalog["cardSetVersion"], "2.5.5")
        self.assertEqual(self.catalog["mechanicsRevision"], 9)

    def test_all_38_supplied_illustrations_retain_original_bytes(self):
        sources = migration.read_json(migration.ART / "manifest.json")["assets"]
        runtime = {entry["id"]: entry for entry in migration.read_json(migration.ROOT / "godot/art/assets.json")["assets"]}
        self.assertEqual(len(sources), 38)
        self.assertEqual({entry["cardId"] for entry in sources}, set(migration.NEW_IDS.values()) | {"engeas-four-point-star", "earthquake"})
        for entry in sources:
            with self.subTest(card_id=entry["cardId"]):
                original = migration.ART / entry["file"]
                installed = migration.ROOT / "godot" / runtime[entry["id"]]["file"].removeprefix("res://")
                self.assertEqual(migration.sha256(original), entry["sha256"])
                self.assertEqual(migration.sha256(installed), entry["sha256"])
                self.assertEqual(migration.image_dimensions(installed), entry["size"])

    def test_aggregate_and_snapshot_provenance_are_current(self):
        self.assertEqual(migration.read_json(migration.ROOT / "assets/cards.json"), self.catalog)
        for source in migration.read_json(migration.SNAPSHOT / "source-manifest.json")["sources"]:
            with self.subTest(source=source["key"]):
                content = (migration.SNAPSHOT / source["snapshot"]).read_bytes().replace(b"\r\n", b"\n")
                self.assertEqual(migration.hashlib.sha256(content).hexdigest(), source["sha256"])


if __name__ == "__main__":
    unittest.main()
