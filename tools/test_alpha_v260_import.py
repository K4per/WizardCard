"""Regression checks for the Alpha v2.6.0 workbook staging importer."""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import import_alpha_v260_xlsx as importer


ROOT = importer.ROOT
DESIGN = importer.DEFAULT_DESIGN
OUTPUT = importer.DEFAULT_OUT
STAGING = OUTPUT / "staging"


class Alpha260ImportTests(unittest.TestCase):
    def test_exact_effect_forms_map_to_existing_runtime_primitives(self):
        self.assertEqual(importer.parse_effect("选择1个目标才能发动。对其造成7点火焰伤害。", "analytic"), (
            [{"kind": "damage", "amount": 7, "damageType": "fire", "recipient": "target"}], [], "player"))
        self.assertEqual(importer.parse_effect("选择1个目标才能发动。对其造成5点暗蚀伤害，己方恢复2点生命值。", "analytic"), (
            [
                {"kind": "damage", "amount": 5, "damageType": "necrotic", "recipient": "target"},
                {"kind": "heal", "amount": 2, "recipient": "owner"},
            ], [], "player"))
        self.assertEqual(importer.parse_effect("获得3点魔素和3点独立荷载。", "word"), (
            [
                {"kind": "mana", "amount": 3, "recipient": "owner"},
                {"kind": "independent", "amount": 3, "recipient": "owner"},
            ], [], "none"))
        self.assertEqual(importer.parse_effect("获得3点临时荷载，使对方获得6点临时荷载。", "word"), (
            [
                {"kind": "temporary", "amount": 3, "recipient": "owner"},
                {"kind": "temporary", "amount": 6, "recipient": "opponent"},
            ], [], "none"))
        self.assertEqual(importer.parse_effect("抽2张卡，获得2点独立荷载。", "word"), (
            [
                {"kind": "draw", "amount": 2, "recipient": "owner"},
                {"kind": "independent", "amount": 2, "recipient": "owner"},
            ], [], "none"))
        self.assertEqual(importer.parse_effect(
            "选择1个目标才能发动。对其造成5点寒冷伤害。那之后，可以使对方解析区1张解析法术卡的解析时间+1。", "analytic"), (
            [
                {"kind": "damage", "amount": 5, "damageType": "cold", "recipient": "target"},
                {"kind": "delay_enemy_analysis", "amount": 1},
            ], [], "player"))
        self.assertEqual(importer.parse_effect(
            "对对方造成15点火焰伤害。那之后，可以销毁对方1张阵法卡上的全部法术卡。己方灰烬区每存在1张塑能派系法术卡，此法术卡的设置费用-1。", "analytic"), (
            [
                {"kind": "damage", "amount": 15, "damageType": "fire", "recipient": "opponent"},
                {"kind": "destroy_formation_spells"},
            ], [], "opponent"))
        self.assertEqual(importer.parse_card_attributes({
            "type": "analytic",
            "effect": "计时4。己方结束阶段开始时，恢复2点生命值。",
        }), ({"duration": 4, "onEnd": [{"kind": "heal", "amount": 2, "recipient": "owner"}]}, [], []))
        seal = {
            "attachToFormation": "阵法等级+1，环位+1，魔素收入-1。",
            "attachToSpell": "发动费用+1，额外造成5点力场伤害。",
        }
        self.assertEqual(importer.parse_seal_attributes(seal), (
            {"rankBonus": 1, "ringBonus": 1, "incomeBonus": -1,
             "castCostBonus": 1, "damageBonus": 5}, []))

    def test_uncertain_or_unmodeled_effects_remain_blocked(self):
        effects, blockers, target = importer.parse_effect(
            "选择1个目标才能发动。对其造成7点自选伤害，伤害类型从强酸/寒冷/火焰中选择。", "analytic")
        self.assertEqual(effects, [{"kind": "choose_damage", "amount": 7, "recipient": "target"}])
        self.assertEqual(target, "player")
        self.assertFalse(blockers)
        effects, blockers, _ = importer.parse_effect("对方发动法术卡时才能发动。将其无效并销毁。", "seal")
        self.assertEqual(effects, [])
        self.assertTrue(blockers)

    def test_workbook_detail_and_blocker_categories_are_separate(self):
        staged, issues, report = importer.parse_workbooks(DESIGN, OUTPUT)
        self.assertEqual(len(staged), 92)
        self.assertEqual(report["counts"], {
            "spells": 72, "analytic": 39, "word": 21, "talisman": 12,
            "seals": 3, "formations": 17, "total": 92,
        })
        self.assertEqual(Counter(card["effectStatus"] for card in staged), {
            "structured-candidate": 40, "blocked": 52,
        })
        self.assertEqual(Counter(card["art"]["status"] for card in staged), {
            "matched": 49, "missing": 42, "conflict": 1,
        })
        by_name = {card["name"]: card for card in staged}
        self.assertEqual(by_name["魔力扰动"]["effectStatus"], "structured-candidate")
        self.assertEqual(by_name["魔力扰动"]["art"]["status"], "missing")
        self.assertTrue(by_name["魔力扰动"]["effectBlockers"] == [])
        self.assertTrue(by_name["火球术"]["implementationBlockers"])
        self.assertTrue(issues)

    def test_workbook_static_statistics_are_explained_as_stale(self):
        book = importer.XlsxBook(DESIGN / "法术卡列表.xlsx")
        detail = book.rows("卡表")
        counts = Counter()
        for _, row in detail[1:]:
            serial = row.get(1)
            if not isinstance(serial, int):
                continue
            counts[row[2]] += 1
        self.assertEqual(counts, {"解析法术": 39, "言灵法术": 21, "咒符法术": 12})
        note = "".join(str(value) for _, row in book.rows("说明") for value in row.values())
        self.assertIn("统计表为生成时的静态计算结果", note)
        self.assertIn("改动卡表后需手动同步", note)

    def test_staging_catalog_is_schema_loadable_and_keeps_2_6_release_gate(self):
        catalog = json.loads((STAGING / "catalog.json").read_text(encoding="utf-8"))
        self.assertEqual(catalog["format"], 1)
        self.assertEqual(catalog["metadata"]["rulesVersion"], "2.6.0")
        self.assertEqual(catalog["metadata"]["mechanicsRevision"], 10)
        self.assertEqual(len(catalog["definitions"]), 92)
        self.assertEqual(len(json.loads((STAGING / "deck.json").read_text(encoding="utf-8"))), 14)
        side = json.loads((STAGING / "side-deck.json").read_text(encoding="utf-8"))
        self.assertEqual(sum(row["count"] for row in side["cards"]), 10)

    def test_cpp_loader_schema_probe_when_release_binary_is_available(self):
        binary = ROOT / "build/windows/Release/wizard_cli.exe"
        if not binary.is_file():
            self.skipTest("local Release wizard_cli.exe is not built")

        # The temporary version override probes data/schema validation only.
        # Production's explicit 2.6.0 version rejection remains in force.
        with tempfile.TemporaryDirectory(prefix="wizardcard-v260-schema-") as temporary:
            content = Path(temporary) / "content"
            shutil.copytree(STAGING, content)
            catalog_path = content / "catalog.json"
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
            catalog["metadata"].update(
                rulesVersion="2.5.5", cardSetVersion="2.5.5", mechanicsRevision=9)
            catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            result = subprocess.run(
                [str(binary), "validate", str(content)], text=True, capture_output=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("92 definitions; 40 cards", result.stdout)


if __name__ == "__main__":
    unittest.main()
