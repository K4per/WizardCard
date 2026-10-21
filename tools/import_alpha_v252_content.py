"""Import the reviewed Alpha 2.5.2 design snapshot and register supplied art.

Run once with --design-dir to refresh the checked-in snapshot. Subsequent runs
use only repository files. Stable IDs and numbers survive display-name changes.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT = ROOT / "assets/design/alpha-v252"
ART = ROOT / "assets/art/alpha-v252-20261021"
SOURCES = {"spells": "法术列表.md", "seals": "符文列表.md", "formations": "阵法列表.md"}
SCHOOLS = dict(zip("塑能 变化 咒法 惑控 幻术 防护 死灵 预言 阿卡那".split(),
                   "evocation transmutation conjuration enchantment illusion abjuration necromancy divination arcana".split()))
TYPES = {"解析法术": "analytic", "言灵法术": "word", "咒符法术": "talisman"}
PREFIX = {"formation": "01", "analytic": "02", "word": "03", "talisman": "04", "seal": "05"}
ALIASES = {
    "次级复原术": "lesser-restoration", "奥数圆环": "ring", "四芒星": "small-hexagram",
    "六芒星": "hexagram", "五芒星": "pentagram-of-life", "八芒星": "octagram", "太阳": "sun",
    "恒风·风暴纹章": "storm-crest", "恒风·风暴之心": "storm-heart",
    "恒风·风暴烙印": "storm-brand", "恒风·湮灭之纹章": "storm-annihilation-crest",
    "恒风·冰风暴": "ice-storm", "恒风·切割之风": "cutting-wind", "恒风·大风暴术": "great-storm",
    "恒风·风语术": "wind-whisper", "恒风·极速之风": "swift-wind", "恒风·雪雨暴": "snow-rain-storm",
    "恒风·造风术": "windmaking", "恒风·风暴导向": "storm-guidance", "恒风·风暴气旋": "storm-vortex",
    "恒风·风击术": "wind-strike", "恒风·大旋风术": "great-cyclone",
    "地震术": "earthquake", "味契·炙烤": "sique-roast", "味契·吹火术": "sique-blowfire",
    "味契·回火": "sique-backfire", "三芒星": "three-star", "二芒星": "two-star", "月亮": "moon",
}
ARCANA = dict(zip("愚者 魔术师 女祭司 皇后 皇帝 教皇 恋人 战车 力量 隐士 正义 倒吊人 死神 节制 恶魔 高塔 世界".split(),
                  "fool magician priestess empress emperor hierophant lovers chariot strength hermit justice hanged-man death temperance devil tower world".split()))
ALIASES.update({name: "arcana-" + slug for name, slug in ARCANA.items()})
FORMATION_ART = {"四芒星": "small-hexagram", "三芒星": "three-star", "二芒星": "two-star",
                 "六芒星": "hexagram", "五芒星": "pentagram-of-life", "八芒星": "octagram", "太阳": "sun", "月亮": "moon"}


def load(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def save(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def digest(path: Path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sections(path: Path):
    for chunk in re.split(r"(?m)^### ", path.read_text(encoding="utf-8"))[1:]:
        name, _, body = chunk.partition("\n")
        fields = dict(re.findall(r"(?m)^\* ([^：\n]+)：([^\n]*)", body))
        effect = re.sub(r"(?m)^# .*$", "", body.partition("* 效果：")[2])
        text = re.sub(r"\s+", " ", re.sub(r"\*+", "", effect)).strip()
        yield name.strip(), fields, text


def eff(kind, amount=0, **fields):
    return {"kind": kind, "amount": amount, **fields}


def recipe(kind, amount=1, **fields):
    return {"kind": kind, "amount": amount, **fields}


# Executable changes are reviewed explicitly; prose never selects an effect kind.
OVERRIDES = {
    "instant-circuit-overload": {"effects": [eff("accelerate_analysis", 2)]},
    "scorching-ray": {"effects": [eff("ray_volley", 5, damageType="fire")]},
    "ice-storm": {"effects": [eff("damage", 8, damageType="cold"), eff("delay_all_enemy_analysis", 1)]},
    "recall": {"effects": [eff("draw", 2), eff("independent", 2)]},
    "magic-missile": {"effects": [eff("damage", 4, damageType="force"), eff("destroy_ambush")]},
    "lightning-bolt": {"effects": [eff("damage", 8, damageType="lightning"), eff("destroy_enemy_analysis")]},
    "thunder-summon": {"effects": [eff("damage", 9, damageType="thunder"), eff("temporary", 5, recipient="opponent")]},
    "messy-wave": {"effects": [], "onCast": [], "traits": ["opponent_spell_load_4"]},
    "lesser-restoration": {"effects": [eff("heal", 2), eff("clear_temporary", 6, recipient="owner")]},
    "aid": {"effects": [eff("temporary_life", 5), eff("temporary", 2)], "target": "self"},
    "snow-rain-storm": {"effects": [], "concentration": True, "concentrationUpkeep": 2},
    "sique-mushroom-soup": {"traits": ["sique_detach_1"]},
    "sique-vinegar": {"effects": [eff("temporary_life", 2, recipient="owner")]},
    "earthquake": {"target": "none", "duration": 2, "effects": [], "onMain": [eff("clear_all_ambush")]},
    "sique-roast": {"target": "player", "tags": ["味契"], "traits": ["sique_campfire_only"], "effects": [eff("damage", 6, damageType="fire"), eff("sique_roast_recover")]},
    "sique-blowfire": {"target": "opponent", "tags": ["味契"], "traits": ["sique_requires_campfire"], "effects": [eff("damage", 3, damageType="fire"), eff("sique_blowfire")]},
    "sique-backfire": {"target": "none", "tags": ["味契"], "traits": ["sique_requires_cauldron"], "effects": [eff("sique_backfire", 5)]},
    "wind-strike": {"responseOnly": True},
    "arcana-fool": {"traits": ["arcana_fool_condition"], "effects": [eff("arcana_fool", 5)]},
    "arcana-magician": {"effects": [eff("arcana_magician", 3)]},
    "arcana-priestess": {"traits": ["arcana_priestess_cost"], "effects": [eff("arcana_priestess")]},
    "arcana-empress": {"duration": 4, "traits": ["end_phase_start"], "effects": [], "onEnd": [eff("heal", 2, recipient="owner"), eff("clear_any_load", 2, recipient="owner")]},
    "arcana-emperor": {"traits": ["arcana_emperor_cost"], "effects": [eff("arcana_emperor")]},
    "arcana-hierophant": {"effects": [eff("prevent_next_damage")]},
    "arcana-lovers": {"effects": [eff("arcana_lovers")]},
    "arcana-chariot": {"target": "player", "traits": ["arcana_chariot"], "effects": [eff("damage", 20, damageType="force")]},
    "arcana-strength": {"perpetual": True, "traits": ["unique", "arcana_strength"], "effects": []},
    "arcana-hermit": {"effects": [eff("arcana_hermit")]},
    "arcana-justice": {"effects": [eff("arcana_justice")]},
    "arcana-hanged-man": {"traits": ["arcana_hanged_cost"], "effects": [eff("arcana_hanged_man")]},
    "arcana-death": {"target": "player", "traits": ["arcana_death"], "effects": [eff("arcana_death", 40)]},
    "arcana-temperance": {"concentration": True, "concentrationUpkeep": 2, "traits": ["arcana_temperance"], "effects": []},
    "arcana-devil": {"target": "pending_link", "responseOnly": True, "traits": ["arcana_devil"], "effects": [eff("arcana_devil")],
                     "responses": [{"id": "retarget_spell", "name": "改变法术目标", "sources": ["casting"], "windows": ["cast"], "eventOwner": "opponent", "target": "pending_link", "effects": [eff("arcana_devil")]}]},
    "arcana-tower": {"target": "own_card", "effects": [eff("arcana_tower")]},
    "arcana-world": {"effects": [eff("mana", 1, recipient="owner"), eff("arcana_world")]},
    "small-hexagram": {"illustrationId": "alpha-v252-small-hexagram"},
    "hexagram": {"recipe": [recipe("sacrifice_formation", definition="small-hexagram"), recipe("mana", 2)], "illustrationId": "alpha-v252-hexagram"},
    "pentagram-of-life": {"recipe": [recipe("sacrifice_formation", definition="hexagram"), recipe("sacrifice_spell", readyOnly=True)], "illustrationId": "alpha-v252-pentagram-of-life"},
    "octagram": {"recipe": [recipe("sacrifice_formation", definition="small-hexagram"), recipe("sacrifice_formation", definition="hexagram")], "illustrationId": "alpha-v252-octagram"},
    "sun": {"traits": ["effect_immune", "destroy_enemy_ambush_end"], "illustrationId": "alpha-v252-sun"},
    "three-star": {"recipe": [recipe("sacrifice_formation", definition="small-hexagram"), recipe("discard_hand")], "traits": ["unique", "quick_host_analysis_1"]},
    "two-star": {"recipe": [recipe("sacrifice_formation", definition="three-star"), recipe("discard_hand")], "traits": ["unique", "quick_host_analysis_2"]},
    "moon": {"recipe": [recipe("sacrifice_formation", definition="small-hexagram"), recipe("sacrifice_formation", definition="three-star"), recipe("sacrifice_formation", definition="two-star"), recipe("sacrifice_formation")], "traits": ["effect_immune", "moon"]},
    "storm-crest": {"rings": 1},
    "storm-heart": {"cost": 0},
    "sique-woodpile": {"cost": 3, "traits": ["max_two"]},
    "sique-campfire": {"cost": 4, "recipe": [recipe("sacrifice_formation", 2, definition="sique-woodpile"), recipe("discard_hand", fireDamage=True)]},
    "sique-cauldron": {"cost": 5, "recipe": [recipe("discard_hand", 3)]},
}


def import_art(design_dir: Path | None, cards):
    entries = []
    names = {**{name: "arcana-" + slug for name, slug in ARCANA.items()}, **FORMATION_ART}
    for name, card_id in names.items():
        family = "arcana" if name in ARCANA else "asterism"
        source = ART / "source" / family / (name + ".png")
        if design_dir:
            source.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(design_dir / family / source.name, source)
        illustration_id = cards[card_id]["illustrationId"]
        destination = ROOT / "godot/art/card" / (illustration_id + ".png")
        shutil.copyfile(source, destination)
        size = list(struct.unpack(">II", source.read_bytes()[16:24]))
        entries.append({"id": "illustration." + illustration_id, "cardId": card_id,
                        "file": source.relative_to(ART).as_posix(), "sha256": digest(source), "size": size,
                        "sampling": "linear", "origin": "User-supplied latest Alpha 2.5.2 artwork; unchanged PNG"})
    # The follow-up supplies three dedicated Sique images. Preserve their JPEG
    # originals and register deterministic transparent derivatives separately.
    for card_id in ("sique-roast", "sique-blowfire", "sique-backfire"):
        source = ART / "processed" / (card_id + ".png")
        original = ART / "source/sique" / (card_id + ".jpg")
        if not source.is_file() or not original.is_file():
            raise ValueError("Missing supplied Sique original or transparent derivative: " + card_id)
        cards[card_id]["illustrationId"] = card_id
        shutil.copyfile(source, ROOT / "godot/art/card" / source.name)
        entries.append({"id": "illustration." + card_id, "cardId": card_id,
                        "file": source.relative_to(ART).as_posix(), "sha256": digest(source),
                        "size": list(struct.unpack(">II", source.read_bytes()[16:24])),
                        "sampling": "nearest", "original": original.relative_to(ART).as_posix(),
                        "originalSha256": digest(original),
                        "origin": "User-supplied Sique JPEG; original RGB pixels with local transparency mask"})
    shared = {"earthquake": "unravel"}
    runtime_path = ROOT / "godot/art/assets.json"
    runtime = load(runtime_path)
    runtime_by_id = {entry["id"]: entry for entry in runtime["assets"]}
    for card_id, source_id in shared.items():
        cards[card_id]["illustrationId"] = source_id
        if "illustration." + source_id not in runtime_by_id:
            raise ValueError("Missing shared project illustration " + source_id)
    manifest = {"format": 1, "collection": "Alpha 2.5.2 supplied Arcana, Asterism and Sique art", "assets": entries,
                "sharedIllustrations": shared, "processing": "Arcana/Asterism unchanged; Sique JPEG backgrounds removed by tools/extract_alpha_v252_sique_art.py; no image generation"}
    save(ART / "manifest.json", manifest)
    ids = {entry["id"] for entry in entries}
    runtime["assets"] = [entry for entry in runtime["assets"] if entry["id"] not in ids]
    for entry in entries:
        illustration_id = entry["id"].removeprefix("illustration.")
        runtime["assets"].append({key: entry[key] for key in ("id", "sha256", "size", "sampling", "origin")} |
                                 {"file": "res://art/card/" + illustration_id + ".png", "source": (ART / entry["file"]).relative_to(ROOT).as_posix()})
    relative = (ART / "manifest.json").relative_to(ROOT).as_posix()
    runtime["additionalSourceManifests"] = [e for e in runtime.get("additionalSourceManifests", []) if e["file"] != relative] + [{"file": relative, "sha256": digest(ART / "manifest.json")}]
    runtime["assets"].sort(key=lambda e: e["id"])
    save(runtime_path, runtime)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--design-dir", type=Path)
    args = parser.parse_args()
    if args.design_dir:
        SNAPSHOT.mkdir(parents=True, exist_ok=True)
        for filename in SOURCES.values():
            shutil.copyfile(args.design_dir / filename, SNAPSHOT / filename)
    baseline_path = SNAPSHOT / "previous-catalog.json"
    if not baseline_path.exists():
        old_manifest = json.loads(subprocess.check_output(["git", "show", "HEAD:assets/catalog.json"], cwd=ROOT, encoding="utf-8"))
        previous_cards = [json.loads(subprocess.check_output(["git", "show", "HEAD:assets/" + entry["file"]], cwd=ROOT, encoding="utf-8")) for entry in old_manifest["definitions"]]
        save(baseline_path, {"version": "2.5.1", "metadata": old_manifest["metadata"], "cards": previous_cards})
    catalog = load(ROOT / "assets/catalog.json")
    cards = {e["id"]: load(ROOT / "assets" / e["file"]) for e in catalog["definitions"]}
    before = {card["id"]: card for card in load(baseline_path)["cards"]}
    by_name = {card["name"]: card_id for card_id, card in cards.items()}
    by_name.update({"恒风·" + card["name"]: card_id for card_id, card in cards.items() if "恒风" in card.get("tags", [])})
    by_name.update(ALIASES)
    next_number = {kind: max([int(c["number"][2:]) for c in cards.values() if c["type"] == kind] or [0]) + 1 for kind in PREFIX}
    rows = []
    for category, filename in SOURCES.items():
        for name, fields, text in sections(SNAPSHOT / filename):
            plain_name = name.replace("（待定）", "")
            card_id = by_name.get(name) or by_name.get(plain_name)
            if not card_id:
                raise ValueError("Unmapped design entry: " + name)
            pending_cross = "（待定）" in name
            if pending_cross:
                rows.append({"category": category, "designName": name, "stableId": card_id, "status": "retained-2.5.1"})
                continue
            kind = TYPES[fields["法术类型"]] if category == "spells" else "seal" if category == "seals" else "formation"
            card = cards.setdefault(card_id, {"id": card_id, "rarity": "rare", "target": "none", "illustrationId": card_id})
            if "number" not in card or card["type"] != kind:
                card["number"] = PREFIX[kind] + f"{next_number[kind]:04d}"
                next_number[kind] += 1
            card.update(name=plain_name if card_id != "ring" else "奥术圆环", type=kind, text=text or "无。")
            if category == "spells":
                cost = [int(value) for value in fields["设置/发动费用"].split("/")]
                card.update(rank=int(fields["法术阶级"]), level=int(fields["法术阶级"]), school=SCHOOLS[fields["派系"]],
                            cost=cost[0], castCost=cost[1] if len(cost) > 1 else 0,
                            analysisTurns=int(fields["解析时间"]), speed=int(fields["咒文速度"]))
                card.pop("immediate", None)
                if "选择1个目标" in text or "选择一个目标" in text:
                    card["target"] = "player"
            elif category == "seals":
                card.update(cost=int(fields["附着费用"]))
            else:
                for key, field in {"level": "阵法等级", "body": "阵体", "rings": "环位", "capacity": "荷载上限", "income": "魔素收入"}.items():
                    card[key] = int(re.match(r"\d+", fields[field])[0])
                card.update(maxRank=card["level"], summonCondition=fields["召唤条件"])
                card.setdefault("cost", 0)
                card.setdefault("recipe", [])
            card.update(copy.deepcopy(OVERRIDES.get(card_id, {})))
            if card_id in {"magic-missile", "protective-flame", "shield", "sique-aroma-lure"}:
                card.pop("responses", None)

            rows.append({"category": category, "designName": name, "stableId": card_id,
                         "status": "updated" if card_id in before else "added"})
    import_art(args.design_dir, cards)
    for row in rows:
        card = cards[row["stableId"]]
        row.update(number=card["number"], illustrationId=card["illustrationId"],
                   changedFields=sorted(key for key in set(card) | set(before.get(card["id"], {})) if card.get(key) != before.get(card["id"], {}).get(key)))
    old_entries = {entry["id"]: entry for entry in catalog["definitions"]}
    entries = []
    for card_id, card in cards.items():
        relative = f"cards/{card['type']}/{card_id}.json"
        save(ROOT / "assets" / relative, card)
        if card_id in old_entries and old_entries[card_id]["file"] != relative:
            (ROOT / "assets" / old_entries[card_id]["file"]).unlink()
        entries.append({"id": card_id, "file": relative})
    catalog["definitions"] = entries
    catalog["metadata"].update(rulesVersion="2.5.2", cardSetVersion="2.5.2", mechanicsRevision=5)
    save(ROOT / "assets/catalog.json", catalog)
    report = {"version": "2.5.2", "status": "imported", "designEntries": len(rows), "catalogDefinitions": len(cards),
              "sources": {key: {"file": (SNAPSHOT / value).relative_to(ROOT).as_posix(), "sha256": digest(SNAPSHOT / value)} for key, value in SOURCES.items()},
              "compatibilityOnlyIds": sorted(set(cards) - {row["stableId"] for row in rows}), "rows": rows}
    save(ROOT / "docs/reports/alpha-v2.5.2-card-pool-migration.json", report)
    lines = ["# Alpha v2.5.2 卡池迁移", "", f"设计快照 {len(rows)} 项全部映射至稳定 ID；正式卡池 {len(cards)} 张。三种十字阵法沿用 2.5.1，恩格亚斯四角星保留用于旧卡组兼容。", "", "| 设计名称 | ID | 编号 | 状态 | 插图 |", "|---|---|---|---|---|"]
    lines += [f"| {row['designName']} | `{row['stableId']}` | {row['number']} | {row['status']} | `{row['illustrationId']}` |" for row in rows]
    lines += ["", "原始设计快照与 SHA-256 见同名 JSON。运行 `python tools/import_alpha_v252_content.py` 可在仓库内重现导入；用 `--design-dir` 显式刷新输入。", ""]
    (ROOT / "docs/reports/alpha-v2.5.2-card-pool-migration.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"Imported {len(rows)} design entries; {len(cards)} catalog cards; {len(cards)-len(before)} new IDs")


if __name__ == "__main__":
    main()
