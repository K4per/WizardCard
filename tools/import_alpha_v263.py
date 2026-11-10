"""Rebuild the approved 90-card release from immutable sources and stable IDs."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import shutil
from pathlib import Path
import import_alpha_v260_xlsx as reader

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/source-archive/alpha-v263"
OUTPUT = ROOT / "assets/release-v260"
METADATA = {"rulesVersion": "2.6.3", "cardSetVersion": "2.6.3",
            "mechanicsRevision": 12, "baseLoadCapacity": 2, "status": "released"}
# Executable clauses whose historical adapters differ from the approved text.
RULE_UPDATES = {
    "v260-word-0013": {"traits": ["return_enemy_ambush"]},
    "v260-talisman-0009": {"traits": ["return_enemy_analytic"]},
    "v260-formation-0001": {"traits": ["end_phase_start"],
        "onEnd": [{"kind": "heal", "amount": 1, "recipient": "owner"}]},
    "v260-formation-0002": {"traits": ["end_phase_start"],
        "onEnd": [{"kind": "lose_life", "amount": 1, "recipient": "owner"},
                  {"kind": "temporary_life", "amount": 3, "recipient": "owner"}]},
    "v260-formation-0003": {"traits": [], "onLeave": [{"kind": "ankh_recover"}]},
    "v260-formation-0007": {"traits": ["replace_formation_destruction"]},
    "v260-analytic-0021": {"traits": ["end_phase_start"]},
    "v260-analytic-0029": {"effects": []},
    "v260-talisman-0002": {"effects": [], "responses": [{
        "id": "capture_physical", "name": "捕获打击", "sources": ["ambush", "casting"],
        "windows": ["cast"], "eventOwner": "opponent", "target": "enemy_spell_link",
        "effects": [{"kind": "negate_capture"}]}]},
}
BOOKS = [("法术卡列表-72张卡.xlsx", "卡表", 72),
         ("符文卡列表-共3张卡.xlsx", "符文卡", 3),
         ("阵法卡列表-共17张卡.xlsx", "阵法卡", 15)]


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


def generate(source=SOURCE, output=OUTPUT):
    baseline = json.loads((source / "baseline.json").read_text(encoding="utf-8"))
    old = {card["name"]: card for card in baseline["cards"]}
    cards, changes, sources = [], [], []
    names = set()
    for filename, sheet, expected in BOOKS:
        path = source / filename
        rows = reader.XlsxBook(path).rows(sheet)
        header_index, headers = reader.header_row(rows, ["卡名", "风味文本", "稀有度"])
        count = 0
        for row_index, values in rows:
            if row_index <= header_index:
                continue
            name = reader.value(values, headers, "卡名")
            if not name:
                continue
            if name in names or name not in old:
                raise ValueError(f"Duplicate or unmapped card identity: {name}")
            names.add(name)
            count += 1
            card = copy.deepcopy(old[name])
            fields = {"阶级":"rank", "设置费用":"cost", "发动费用":"castCost",
                      "解析时间":"analysisTurns", "咒文速度":"speed",
                      "阵法等级":"level", "阵体":"body", "环位":"rings",
                      "荷载上限":"capacity", "魔素收入":"income", "附着费用":"cost"}
            for heading, key in fields.items():
                if heading not in headers:
                    continue
                value = reader.value(values, headers, heading)
                if value is None:
                    raise ValueError(f"Missing numeric field: {name}/{heading}")
                if value == "X" and key == "castCost":
                    card.update(castCost=0, dynamicCost=True, castCostSource="X")
                else:
                    if not isinstance(value, (int, float)) or int(value) != value:
                        raise ValueError(f"Invalid integer: {name}/{heading}/{value}")
                    card[key] = int(value)
            if "类型" in headers:
                card["type"] = reader.TYPE_NAMES[reader.value(values, headers, "类型")]
                card["school"] = reader.SCHOOL_NAMES[reader.value(values, headers, "派系")]
            effect = reader.value(values, headers, "效果")
            if card["type"] == "seal":
                formation = reader.value(values, headers, "附着于阵法")
                spell = reader.value(values, headers, "附着于法术")
                card["text"] = f"附着于阵法：{formation} 附着于法术：{spell}"
                attributes, errors = reader.parse_seal_attributes(
                    {"attachToFormation": formation, "attachToSpell": spell})
                if errors:
                    raise ValueError(errors)
                card.update(attributes)
            else:
                if not effect:
                    raise ValueError(f"Missing effect: {name}")
                card["text"] = effect
            card["flavor"] = reader.value(values, headers, "风味文本") or ""
            rarity = reader.value(values, headers, "稀有度")
            if rarity:
                card["rarity"] = reader.RARITIES[rarity]
            if card["type"] == "formation":
                condition = reader.value(values, headers, "召唤条件")
                recipe, errors = reader.parse_formation_recipe(condition)
                if errors:
                    raise ValueError(f"{name}: {errors}")
                card.update(recipe=recipe, summonCondition=condition)
            card.update(copy.deepcopy(RULE_UPDATES.get(card["id"], {})))
            card["source"] = {"file": filename, "worksheet": sheet, "row": row_index}
            diffs = {key: {"before": old[name].get(key), "after": value}
                     for key, value in card.items() if old[name].get(key) != value and key != "source"}
            if diffs:
                changes.append({"id": card["id"], "name": name, "fields": diffs})
            cards.append(card)
        if count != expected:
            raise ValueError(f"{filename}: expected {expected} valid cards, got {count}")
        sources.append({"file": filename, "sheet": sheet, "count": count,
                        "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    retired = [c for c in baseline["cards"] if c["name"] not in names]
    if {c["name"] for c in retired} != {"守护法阵", "幻象法阵"}:
        raise ValueError("Unapproved card retirement")
    order = {card["id"]: index for index, card in enumerate(baseline["cards"])}
    cards.sort(key=lambda card: order[card["id"]])
    # Finish all validation before changing the runtime catalogue.
    output.mkdir(parents=True, exist_ok=True)
    for card in cards:
        write(output / "cards" / (card["id"] + ".json"), card)
    manifest = {"format": 1, "metadata": METADATA,
                "definitions": [{"id": c["id"], "file": f"cards/{c['id']}.json"} for c in cards]}
    write(output / "catalog.json", manifest)
    write(output / "cards.json", {**METADATA, "cards": cards})
    for name in ("deck.json", "side-deck.json"):
        write(output / name, json.loads((source / name).read_text(encoding="utf-8")))
    presets = json.loads((source / "presets.json").read_text(encoding="utf-8"))
    for preset in presets:
        preset["name"] = "Alpha v2.6.3 入门"
        preset["description"] = "Alpha v2.6.3 正式卡池的 40+10 入门牌组。"
    write(output / "presets.json", presets)
    shutil.copytree(ROOT / "assets/audio", output / "audio", dirs_exist_ok=True)
    for card in retired:
        (output / "cards" / (card["id"] + ".json")).unlink(missing_ok=True)
    report = {"format": 1, **METADATA, "cardCount": len(cards),
              "flavorCount": sum(bool(c["flavor"]) for c in cards),
              "sources": sources, "changes": changes,
              "retired": [{"id": c["id"], "name": c["name"]} for c in retired],
              "missingFields": [], "unmappedIdentities": []}
    write(output / "RELEASE.json", {key: value for key, value in report.items() if key != "changes"})
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--report", type=Path, default=ROOT / "build/v263/content-import.json")
    args = parser.parse_args()
    report = generate(args.source, args.output)
    write(args.report, report)
    print(json.dumps({key: report[key] for key in ("cardCount", "flavorCount", "retired")}, ensure_ascii=False))
