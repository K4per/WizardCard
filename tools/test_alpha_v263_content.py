"""Verify the approved workbook fields against the shipped catalogue."""
import json
from pathlib import Path
import unittest
import tempfile
import import_alpha_v263
import import_alpha_v260_xlsx as reader

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/source-archive/alpha-v263"
RELEASE = ROOT / "assets/release-v260"


class Content263Tests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads((RELEASE / "catalog.json").read_text(encoding="utf-8"))
        self.cards = {c["name"]: c for c in (
            json.loads((RELEASE / d["file"]).read_text(encoding="utf-8"))
            for d in self.manifest["definitions"])}

    def test_retired_formations_are_absent_without_renumbering_other_cards(self):
        baseline = json.loads((SOURCE / "baseline.json").read_text(encoding="utf-8"))["cards"]
        expected = {c["name"]: c["id"] for c in baseline if c["name"] not in {"守护法阵", "幻象法阵"}}
        self.assertEqual(len(self.cards), 90)
        self.assertEqual({name: c["id"] for name, c in self.cards.items()}, expected)
        for c in baseline:
            if c["name"] in self.cards:
                self.assertEqual(self.cards[c["name"]]["illustrationId"], c["illustrationId"])
                self.assertEqual(self.cards[c["name"]]["rarity"], c["rarity"])

    def test_spell_fields_and_all_flavor_texts_match_workbook(self):
        book = reader.XlsxBook(SOURCE / "法术卡列表-72张卡.xlsx")
        rows = book.rows("卡表")
        _, headers = reader.header_row(rows, ["卡名", "风味文本"])
        count = 0
        for index, values in rows[1:]:
            name = reader.value(values, headers, "卡名")
            if not name:
                continue
            count += 1
            card = self.cards[name]
            for field, key in {"阶级":"rank", "设置费用":"cost", "解析时间":"analysisTurns", "咒文速度":"speed"}.items():
                self.assertEqual(card[key], reader.value(values, headers, field), (name, field))
            self.assertEqual(card.get("flavor"), reader.value(values, headers, "风味文本"), name)
            fee = reader.value(values, headers, "发动费用")
            if fee == "X":
                self.assertTrue(card["dynamicCost"])
                self.assertEqual(card["castCostSource"], "X")
            else:
                self.assertEqual(card["castCost"], fee, name)
        self.assertEqual(count, 72)

    def test_five_new_recipes_are_executable_data(self):
        self.assertEqual(self.cards["强化法阵"]["recipe"], [
            {"kind":"sacrifice_formation", "amount":1, "definition":"v260-formation-0010"},
            {"kind":"sacrifice_spell", "amount":1, "faceDownOnly":True}])
        self.assertEqual(self.cards["复合法阵"]["recipe"], [
            {"kind":"sacrifice_formation", "amount":1, "definition":"v260-formation-0011"},
            {"kind":"sacrifice_formation", "amount":1, "definition":"v260-formation-0009"}])
        for name, previous, spells, mana in [
            ("增幅法阵","v260-formation-0012",1,2),
            ("高阶法阵","v260-formation-0013",1,4),
            ("完美法阵","v260-formation-0016",2,6)]:
            self.assertEqual(self.cards[name]["recipe"], [
                {"kind":"sacrifice_formation","amount":1,"definition":previous},
                {"kind":"sacrifice_spell","amount":spells,"readyOnly":True},
                {"kind":"mana","amount":mana}], name)

    def test_facedown_recipe_parser_preserves_material_constraint(self):
        recipe, errors = reader.parse_formation_recipe("【标准法阵】+盖伏卡×1")
        self.assertEqual(errors, [])
        self.assertTrue(recipe[1]["faceDownOnly"])

    def test_aggregate_is_generated_from_manifest_definitions(self):
        expected = dict(self.manifest["metadata"])
        expected["cards"] = [json.loads((RELEASE / row["file"]).read_text(encoding="utf-8"))
                             for row in self.manifest["definitions"]]
        self.assertEqual(json.loads((RELEASE / "cards.json").read_text(encoding="utf-8")), expected)

    def test_formation_and_rune_fields_match_all_valid_source_rows(self):
        for filename, sheet, expected in import_alpha_v263.BOOKS[1:]:
            rows = reader.XlsxBook(SOURCE / filename).rows(sheet)
            heading, headers = reader.header_row(rows, ["卡名", "风味文本"])
            count = 0
            for index, values in rows:
                name = reader.value(values, headers, "卡名")
                if index <= heading or not name:
                    continue
                count += 1
                card = self.cards[name]
                for field, key in {"阵法等级":"level", "阵体":"body", "环位":"rings",
                                   "荷载上限":"capacity", "魔素收入":"income", "附着费用":"cost"}.items():
                    if field in headers:
                        self.assertEqual(card[key], reader.value(values, headers, field), (name, field))
                self.assertEqual(card["flavor"], "")
                if sheet == "符文卡":
                    for field in ("附着于阵法", "附着于法术"):
                        self.assertIn(reader.value(values, headers, field), card["text"])
                else:
                    self.assertEqual(card["text"], reader.value(values, headers, "效果"))
            self.assertEqual(count, expected)

    def test_regeneration_reproduces_shipped_content_and_legal_starter_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            import_alpha_v263.generate(output=output)
            for path in output.rglob("*.json"):
                self.assertEqual(path.read_bytes(), (RELEASE / path.relative_to(output)).read_bytes(), path.name)
        ids = {card["id"] for card in self.cards.values()}
        for name, count in [("deck.json", 40), ("side-deck.json", 10)]:
            rows = json.loads((RELEASE / name).read_text(encoding="utf-8"))
            if isinstance(rows, dict):
                rows = rows["cards"]
            self.assertEqual(sum(row["count"] for row in rows), count)
            self.assertTrue(all(row["id"] in ids for row in rows))


if __name__ == "__main__":
    unittest.main()
