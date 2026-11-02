"""Import the reviewed 2026-10-24 pool with stable identities and art provenance.

Source snapshots, artwork and the pre-update catalog are frozen once. Later runs
use repository inputs only. Executable capabilities are explicit definitions in
this file; this importer does not infer mechanics from natural language.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
from design_snapshot_archive import materialize_snapshot, snapshot_dir
from import_alpha_v253_content import ALIASES as OLD_ALIASES, SCHOOL, TYPE

SNAPSHOT = snapshot_dir(ROOT, "alpha-v254")
ART = ROOT / "assets/art/alpha-v254-20261024"
SOURCE_FILES = {
    "spells": "法术卡列表-共29+14+6张卡.md",
    "seals": "符文卡列表-共3张卡.md",
    "formations": "阵法卡列表-共16张卡.md",
    "sique": "【味契】系列-共19张卡.md",
    "storm": "【风契】系列-共15张卡.md",
    "qixing": "【七星】系列.md",
    "blood": "【血契】系列.md",
    "changes": "10.24改动报告.md",
}
EXPECTED_COUNTS = {"spells": 49, "seals": 3, "formations": 16, "sique": 19,
                   "storm": 15, "qixing": 16, "blood": 20}
QIXING_NAMES = ["七星·天枢", "七星·天璇", "七星·天玑", "七星·天权", "七星·玉衡", "七星·开阳", "七星·瑶光",
                "七星星帜", "七星灯盏", "七星归墟", "七星启曜", "七星遥鉴", "七星星移", "七星的注视", "七星惩戒", "七星归迹"]
QIXING_IDS = ["qixing-tianshu", "qixing-tianxuan", "qixing-tianji", "qixing-tianquan", "qixing-yuheng", "qixing-kaiyang", "qixing-yaoguang",
              "qixing-banner", "qixing-lamp", "qixing-ruin", "qixing-awaken", "qixing-divination", "qixing-shift", "qixing-gaze", "qixing-punishment", "qixing-return"]
BLOOD_NAMES = ["血契·祭坛", "血契·输血管", "血契·祭品台", "血契·血池", "血契·心脏", "血契·血印", "血契·血脉", "血契·血咒", "血契·心印",
               "血契·血刃", "血契·血债", "血契·续命", "血契·血偿", "血契·放血", "血契·割腕", "血契·祭品", "血契·背水", "血契·凝血", "血契·血雨", "血契·同源"]
BLOOD_IDS = ["blood-altar", "blood-transfusion", "blood-sacrificial-table", "blood-pool", "blood-heart", "blood-mark", "blood-vein", "blood-curse", "blood-heartmark",
             "blood-blade", "blood-debt", "blood-renewal", "blood-repayment", "blood-letting", "blood-wrist", "blood-offering", "blood-last-stand", "blood-coagulation", "blood-rain", "blood-same-origin"]
NEW_IDS = dict(zip(QIXING_NAMES + BLOOD_NAMES, QIXING_IDS + BLOOD_IDS))
PENDING = {"upright-cross", "inverted-cross", "ankh"}
PREFIXES = {"formation": "01", "analytic": "02", "word": "03", "talisman": "04", "seal": "05"}
ALIASES = dict(OLD_ALIASES, **NEW_IDS)
ALIASES.update({"沉默术": "instantly-sleep", "奥术圆环": "ring", "三芒星": "three-star", "二芒星": "two-star"})

# Explicit executable declarations, agreed with the engine implementation.
# ChangeLifeLimit is signed; the loader permits negative amounts only for this
# effect. A recipe life cost is paid directly and is never a damage event.
CAPABILITIES = {
    "qixing-tianshu": {"recipe": [{"kind": "discard_hand", "amount": 1}]},
    "qixing-tianxuan": {"recipe": [{"kind": "sacrifice_formation", "amount": 1, "definition": "qixing-tianshu"}, {"kind": "mana", "amount": 2}]},
    "qixing-tianji": {"traits": ["seven_stars_effect_only"], "recipe": []},
    "qixing-tianquan": {"traits": ["seven_stars_effect_only"], "recipe": []},
    "qixing-yuheng": {"traits": ["seven_stars_effect_only"], "recipe": []},
    "qixing-kaiyang": {"traits": ["seven_stars_effect_only", "seven_stars_kaiyang"], "onPrepare": [{"kind": "seven_kaiyang"}], "recipe": []},
    "qixing-yaoguang": {"traits": ["seven_stars_effect_only", "seven_stars_yaoguang"], "recipe": []},
    "qixing-banner": {"traits": ["seven_stars_banner"]},
    "qixing-lamp": {"traits": ["seven_stars_lamp"], "duration": 7},
    "qixing-ruin": {"effects": [{"kind": "seven_stars_ruin", "amount": 20}]},
    "qixing-awaken": {"effects": [{"kind": "seven_stars_awakening"}]},
    "qixing-divination": {"effects": [{"kind": "seven_stars_insight"}]},
    "qixing-shift": {"effects": [{"kind": "seven_stars_shift"}]},
    "qixing-gaze": {"responseOnly": True, "effects": [{"kind": "seven_stars_gaze"}],
                    "responses": [{"id": "seven_stars_gaze", "name": "无效并盖伏连锁法术", "sources": ["ambush"],
                                   "windows": ["cast"], "eventOwner": "any", "target": "enemy_spell_link",
                                   "effects": [{"kind": "seven_stars_gaze"}]}]},
    "qixing-punishment": {"effects": [{"kind": "seven_stars_ruin", "amount": 12, "recipient": "opponent"}]},
    "qixing-return": {"effects": [{"kind": "seven_stars_return", "amount": 7}]},
    "blood-altar": {"traits": ["blood_altar"], "recipe": [{"kind": "mana", "amount": 3}]},
    "blood-transfusion": {"traits": ["unique", "blood_transfusion"], "recipe": [{"kind": "mana", "amount": 3}]},
    "blood-sacrificial-table": {"traits": ["blood_sacrifice_reward"], "recipe": [{"kind": "mana", "amount": 4}, {"kind": "life", "amount": 5}]},
    "blood-pool": {"traits": ["blood_pool"], "recipe": [{"kind": "sacrifice_formation", "amount": 1, "definition": "blood-altar"}, {"kind": "mana", "amount": 3}]},
    "blood-heart": {"traits": ["blood_heart"], "recipe": [{"kind": "sacrifice_formation", "amount": 1, "definition": "blood-pool"}, {"kind": "mana", "amount": 5}]},
    "blood-mark": {"traits": ["blood_mark"], "incomeBonus": 2},
    "blood-vein": {"traits": ["blood_vein"]},
    "blood-curse": {"traits": ["blood_curse"], "rankBonus": 1, "ringBonus": 1},
    "blood-heartmark": {"traits": ["blood_heartmark"]},
    "blood-blade": {"target": "player", "effects": [{"kind": "blood_pact_damage", "amount": 4, "damageType": "necrotic"}]},
    "blood-debt": {"target": "player", "effects": [{"kind": "blood_pact_damage", "amount": 8, "damageType": "necrotic"}, {"kind": "blood_pact_search"}]},
    "blood-renewal": {"duration": 3, "traits": ["end_phase_start"],
                      "onEnd": [{"kind": "heal", "amount": 1, "recipient": "owner"}, {"kind": "change_life_limit", "amount": 2, "recipient": "owner"}]},
    "blood-repayment": {"target": "player", "effects": [{"kind": "blood_pact_damage", "amount": 16, "damageType": "necrotic"}, {"kind": "blood_pact_search"}]},
    "blood-letting": {"effects": [{"kind": "change_life_limit", "amount": -6, "recipient": "owner"}, {"kind": "mana", "amount": 6, "recipient": "owner"}]},
    "blood-wrist": {"traits": ["blood_wrist_cost"], "effects": [{"kind": "draw", "amount": 2, "recipient": "owner"}]},
    "blood-offering": {"traits": ["blood_offering_cost"], "effects": [{"kind": "blood_pact_search"}]},
    "blood-last-stand": {"traits": ["blood_last_stand"], "effects": [{"kind": "mana", "amount": 2, "recipient": "owner"}, {"kind": "draw", "amount": 2, "recipient": "owner"}]},
    "blood-coagulation": {"responseOnly": True, "effects": [{"kind": "blood_pact_clot", "amount": 4}],
                          "responses": [{"id": "blood_clot", "name": "免疫此次伤害", "sources": ["ambush"], "windows": ["cast", "trigger"],
                                         "eventOwner": "opponent", "target": "none", "effects": [{"kind": "blood_pact_clot", "amount": 4}]}]},
    "blood-rain": {"effects": [{"kind": "change_life_limit", "amount": -4, "recipient": "owner"}, {"kind": "damage", "amount": 12, "damageType": "necrotic", "recipient": "opponent"}]},
    "blood-same-origin": {"traits": ["blood_same_origin"], "perpetual": True},
}


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def image_dimensions(path: Path) -> list[int]:
    data = path.read_bytes()
    if data.startswith(b"\x89PNG\r\n\x1a\n") and data[12:16] == b"IHDR":
        return list(struct.unpack(">II", data[16:24]))
    if data.startswith(b"\xff\xd8"):
        offset = 2
        while offset + 4 <= len(data):
            if data[offset] != 255:
                raise ValueError(f"Invalid JPEG marker: {path}")
            while data[offset] == 255:
                offset += 1
            marker = data[offset]
            offset += 1
            if marker in {1, 0xD8, 0xD9} or 0xD0 <= marker <= 0xD7:
                continue
            length = int.from_bytes(data[offset:offset + 2], "big")
            if marker in {0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF}:
                height, width = struct.unpack(">HH", data[offset + 3:offset + 7])
                return [width, height]
            if length < 2:
                break
            offset += length
    raise ValueError(f"Unsupported or invalid supplied image: {path}")


def snapshot_sources(design_dir: Path):
    rows = []
    for key, filename in SOURCE_FILES.items():
        source = design_dir / filename
        destination = SNAPSHOT / "sources" / (key + ".md")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        rows.append({"key": key, "original": str(source), "snapshot": "sources/" + key + ".md", "sha256": sha256(destination)})
    requirement = design_dir.parent / "Alpha v2.5.4更新需求报告.md"
    if requirement.is_file():
        destination = SNAPSHOT / "sources/requirements.md"
        shutil.copyfile(requirement, destination)
        rows.append({"key": "requirements", "original": str(requirement), "snapshot": "sources/requirements.md", "sha256": sha256(destination)})
    write_json(SNAPSHOT / "source-manifest.json", {"format": 1, "sources": rows})
    previous = SNAPSHOT / "previous-catalog.json"
    if not previous.exists():
        previous.write_bytes(subprocess.check_output(["git", "show", "HEAD:assets/cards.json"], cwd=ROOT))
    for filename in PENDING:
        source = snapshot_dir(ROOT, "alpha-v253") / "drafts/cards" / (filename + ".json")
        destination = SNAPSHOT / "drafts/cards" / source.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)


def parse_cards():
    rows = []
    for key in EXPECTED_COUNTS:
        text = (SNAPSHOT / "sources" / (key + ".md")).read_text(encoding="utf-8-sig")
        # 七星星帜 is deliberately a level-two heading in the supplied source.
        headings = list(re.finditer(r"(?m)^#{2,3}\s+(.+?)\s*$", text))
        for index, heading in enumerate(headings):
            preceding = re.findall(r"(?m)^#\s+(.+?)\s*$", text[:heading.start()])
            section = preceding[-1].strip() if preceding else ""
            body = text[heading.end():headings[index + 1].start() if index + 1 < len(headings) else len(text)]
            fields = dict(re.findall(r"(?m)^\*\s*([^：\n]+)：([^\n]*)", body))
            effect = body.partition("* 效果：")[2]
            effect = re.sub(r"(?m)^#.*$", "", effect)
            lines = [re.sub(r"^\s*[-*]\s*", "", re.sub(r"\*+", "", line)).strip() for line in effect.splitlines()]
            effect = " ".join(line for line in lines if line)
            effect = effect.replace("七星·星移", "七星星移").replace("早成", "造成")
            card_type = {"formations": "formation", "seals": "seal"}.get(key, TYPE.get(section))
            if card_type is None:
                raise ValueError((key, heading.group(1), section))
            rows.append({"source": key, "name": heading.group(1).strip(), "type": card_type,
                         "fields": fields, "text": effect})
    counts = Counter(row["source"] for row in rows)
    if counts != Counter(EXPECTED_COUNTS):
        raise ValueError(f"Source title counts differ: {counts}")
    return rows


def resolve_id(row, cards):
    name = row["name"]
    if name in ALIASES:
        return ALIASES[name]
    # Stable Storm IDs survive the series display-name migration.
    names = {card["name"].replace("恒风", "风契"): card_id for card_id, card in cards.items()}
    if name not in names:
        raise ValueError(f"Unmapped source card: {name}")
    return names[name]


def fields_for(row, card):
    fields = row["fields"]
    result = {"name": row["name"], "text": row["text"], "type": row["type"]}
    def numeric(source, target):
        match = re.search(r"\d+", fields.get(source, ""))
        if match:
            result[target] = int(match.group())
    if row["type"] == "formation":
        for source, target in (("阵法等级", "level"), ("阵体", "body"), ("环位", "rings"), ("荷载上限", "capacity"), ("魔素收入", "income")):
            numeric(source, target)
        result["maxRank"] = result.get("level", card.get("level", 1))
        result["summonCondition"] = fields.get("召唤条件", card.get("summonCondition", ""))
        if card["id"] == "storm-crest":
            result.update(level=1, rings=1, capacity=5, income=1, maxRank=1)
    elif row["type"] == "seal":
        numeric("附着费用", "cost")
    else:
        numeric("法术阶级", "rank")
        numeric("法术阶级", "level")
        numeric("咒文速度", "speed")
        numeric("解析时间", "analysisTurns")
        if fields.get("派系") in SCHOOL:
            result["school"] = SCHOOL[fields["派系"]]
        fees = re.findall(r"\d+", fields.get("设置/发动费用", ""))
        if fees:
            result["cost"] = int(fees[0])
            result["castCost"] = int(fees[1]) if row["type"] == "analytic" and len(fees) > 1 else 0
    return result


def new_definition(row, number):
    card_id = NEW_IDS[row["name"]]
    definition = {"id": card_id, "name": row["name"], "type": row["type"], "rarity": "rare",
                  "cost": 0, "castCost": 0, "rank": 1, "level": 1, "speed": 1,
                  "number": number, "illustrationId": card_id, "target": "none",
                  "tags": ["七星" if card_id.startswith("qixing-") else "血契"], "effects": []}
    definition.update(fields_for(row, definition))
    if row["type"] == "formation":
        definition.update(rank=0, baseEligible=definition["level"] == 1)
    elif row["type"] == "seal":
        definition.update(rank=1, level=0)
    return definition


def synchronize_art(design_dir: Path):
    """Keep original bytes and map every new/replacement illustration directly."""
    art_manifest = {"format": 1, "collection": "Alpha 2.5.4 supplied Qixing, Blood and replacement artwork", "assets": []}
    mappings = [(name, card_id, "七星卡图") for name, card_id in zip(QIXING_NAMES, QIXING_IDS)]
    mappings += [(name, card_id, "血契卡图") for name, card_id in zip(BLOOD_NAMES, BLOOD_IDS)]
    mappings += [("恩格亚斯之星", "engeas-four-point-star", "卡图替换"), ("地震术", "earthquake", "卡图替换")]
    runtime_manifest = read_json(ROOT / "godot/art/assets.json")
    by_id = {entry["id"]: entry for entry in runtime_manifest["assets"]}
    for name, card_id, folder in mappings:
        candidates = sorted((design_dir / folder).glob(name + ".*"))
        candidates = [path for path in candidates if path.suffix.lower() in {".png", ".jpg", ".jpeg"}]
        if len(candidates) != 1:
            raise ValueError(f"Expected one source image for {name}: {candidates}")
        source = candidates[0]
        relative = "source/" + folder + "/" + source.name
        original = ART / relative
        original.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, original)
        runtime = ROOT / "godot/art/cards" / (card_id + source.suffix.lower())
        runtime.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, runtime)
        size = image_dimensions(source)
        entry = {"id": "illustration." + card_id, "cardId": card_id, "file": relative,
                 "sha256": sha256(source), "size": size, "sampling": "linear",
                 "origin": "User-supplied 10.24 artwork; original bytes preserved"}
        art_manifest["assets"].append(entry)
        by_id[entry["id"]] = {**entry, "file": "res://" + runtime.relative_to(ROOT / "godot").as_posix(),
                               "source": original.relative_to(ROOT).as_posix()}
    write_json(ART / "manifest.json", art_manifest)
    runtime_manifest["assets"] = sorted(by_id.values(), key=lambda row: row["id"])
    manifest_file = (ART / "manifest.json").relative_to(ROOT).as_posix()
    sources = {row["file"]: row for row in runtime_manifest["additionalSourceManifests"]}
    sources[manifest_file] = {"file": manifest_file, "sha256": sha256(ART / "manifest.json")}
    runtime_manifest["additionalSourceManifests"] = list(sources.values())
    write_json(ROOT / "godot/art/assets.json", runtime_manifest)


def apply_pool(rows):
    catalog = read_json(ROOT / "assets/catalog.json")
    entries = {entry["id"]: entry for entry in catalog["definitions"]}
    cards = {card_id: read_json(ROOT / "assets" / entry["file"]) for card_id, entry in entries.items()}
    next_numbers = {kind: max([int(card["number"][2:]) for card in cards.values() if card["type"] == kind] or [0]) + 1 for kind in PREFIXES}
    seen = set()
    for row in rows:
        card_id = resolve_id(row, cards)
        if card_id in seen:
            raise ValueError(f"Duplicate card identity in sources: {card_id}")
        seen.add(card_id)
        if card_id in PENDING:
            draft = read_json(SNAPSHOT / "drafts/cards" / (card_id + ".json"))
            draft.update(fields_for(row, draft))
            write_json(SNAPSHOT / "drafts/cards" / (card_id + ".json"), draft)
            continue
        if card_id not in cards:
            kind = row["type"]
            card = new_definition(row, PREFIXES[kind] + f"{next_numbers[kind]:04d}")
            next_numbers[kind] += 1
            entries[card_id] = {"id": card_id, "file": f"cards/{kind}/{card_id}.json"}
            cards[card_id] = card
        card = cards[card_id]
        card.update(fields_for(row, card))
        card.update(CAPABILITIES.get(card_id, {}))
        if card_id.startswith("qixing-"):
            card["tags"] = ["七星"]
        elif card_id.startswith("blood-"):
            card["tags"] = ["血契"]
        if row["source"] == "storm":
            card["tags"] = list(dict.fromkeys([tag for tag in card.get("tags", []) if tag not in {"恒风", "风暴"}] + ["风契"]))
        # Replace literal references only; Storm Guidance keeps Cloud Mist/Thunder Summon.
        card["text"] = card["text"].replace("昏昏入睡", "沉默术").replace("恒风指示物", "风契指示物").replace("恒风", "风契").replace("奥数圆环", "奥术圆环")
        if card_id in {"blood-blade", "blood-debt", "blood-repayment"}:
            card["text"] = card["text"].replace("等量的伤害", "等量的暗蚀伤害")
        if card_id == "blood-rain":
            card["text"] = card["text"].replace("造成12点伤害", "造成12点暗蚀伤害")
        write_json(ROOT / "assets" / entries[card_id]["file"], card)
    if seen - PENDING != cards.keys():
        raise ValueError(f"Source coverage differs from pool: missing={cards.keys() - seen}, unexpected={seen - PENDING - cards.keys()}")
    catalog["metadata"].update(rulesVersion="2.5.4", cardSetVersion="2.5.4", mechanicsRevision=8)
    catalog["definitions"] = list(entries.values())
    write_json(ROOT / "assets/catalog.json", catalog)
    write_json(ROOT / "assets/cards.json", dict(catalog["metadata"], cards=list(cards.values())))
    write_json(SNAPSHOT / "draft-cards.json", {"status": "draft", "excludedFromCatalog": sorted(PENDING),
                                               "reason": "10.24 source explicitly marks待定; empty effects are not finalized"})
    return cards


def report(rows, cards):
    previous = {card["id"]: card for card in read_json(SNAPSHOT / "previous-catalog.json")["cards"]}
    artwork = {row["id"]: row for row in read_json(ROOT / "godot/art/assets.json")["assets"]}
    records = []
    for row in rows:
        card_id = resolve_id(row, cards)
        card = cards.get(card_id)
        if card is None:
            records.append({"id": card_id, "name": row["name"], "source": row["source"], "status": "draft", "changes": [], "image": "not published", "effectStatus": "待定，禁止构筑", "tests": []})
            continue
        old = previous.get(card_id)
        changes = [key for key in sorted(set(old or {}) | set(card)) if not old or old.get(key) != card.get(key)]
        image = artwork["illustration." + card.get("illustrationId", card_id)]
        image_path = ROOT / "godot" / image["file"].removeprefix("res://")
        if sha256(image_path) != image["sha256"]:
            raise ValueError(f"Runtime illustration hash mismatch: {card_id}")
        test_files = []
        for filename in (ROOT / "tests").glob("*.cpp"):
            if card_id in filename.read_text(encoding="utf-8-sig"):
                test_files.append(filename.relative_to(ROOT).as_posix())
        # This status only describes the actual loaded structure. It deliberately
        # makes no unsupported claim that every behavioral scenario has passed.
        implemented = bool(card.get("effects") or card.get("traits") or card.get("responses") or
                           any(card.get(key) for key in ("incomeBonus", "ringBonus", "rankBonus", "refundSetting", "quickRankOne", "onEnd", "onPrepare")))
        effect_status = "结构化效果/机制已定义，行为验收见测试报告" if implemented else "需规则内核实现/确认"
        records.append({"id": card_id, "name": card["name"], "source": row["source"], "status": "new" if old is None else "updated" if changes else "matched",
                        "changes": changes, "image": image["file"], "imageSha256": image["sha256"],
                        "effectStatus": effect_status, "tests": test_files})
    write_json(SNAPSHOT / "card-differences.json", {"format": 1, "baselineRulesVersion": "2.5.3-hotfix.1", "targetVersion": "2.5.4", "records": records})
    lines = ["# Alpha v2.5.4 逐卡差异报告", "",
             "来源快照、原始路径及 SHA-256 见 `source-manifest.json`；更新前正式卡池见 `previous-catalog.json`。稳定 ID 和已有编号全部保留。七星星帜的二级标题按卡牌解析；七星·星移统一引用七星星移，早成修正为造成。", "",
             f"源条目 {len(rows)}；正式卡 {len(cards)}；新增 {len(cards.keys() - previous.keys())}；待定不发布 {len(PENDING)}。所有正式卡均有唯一 ID、唯一编号与有效插图映射。", "",
             "本报告中的结构化定义状态不等同于完整实装或测试通过；规则裁定、行为验证及已知限制以本次交付报告为准。三张十字阵法保留待定草稿。", "",
             "| 卡名 | 稳定 ID | 状态 | 字段变化 | 效果状态 | 图像 | 对应测试文件 |", "|---|---|---|---|---|---|---|"]
    for record in records:
        lines.append("| " + " | ".join([record["name"], "`" + record["id"] + "`", record["status"], ", ".join(record["changes"]) or "无", record["effectStatus"], record["image"], ", ".join(record["tests"]) or "待补充"]) + " |")
    lines += ["", "## 来源计数", "", "| 来源 | 张数 |", "|---|---:|"]
    lines += [f"| {source} | {amount} |" for source, amount in EXPECTED_COUNTS.items()]
    (SNAPSHOT / "card-diff-report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    global SNAPSHOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--design-dir", type=Path, help="freeze the supplied 10.24 source directory and original artwork")
    parser.add_argument("--apply", action="store_true", help="apply reviewed fields and explicit new definitions")
    args = parser.parse_args()
    if args.design_dir:
        SNAPSHOT = materialize_snapshot(ROOT, "alpha-v254")
        snapshot_sources(args.design_dir)
        synchronize_art(args.design_dir)
    rows = parse_cards()
    if args.apply:
        cards = apply_pool(rows)
    else:
        catalog = read_json(ROOT / "assets/catalog.json")
        cards = {entry["id"]: read_json(ROOT / "assets" / entry["file"]) for entry in catalog["definitions"]}
    report(rows, cards)
    print(f"Reviewed {len(rows)} source entries; {len(cards)} formal cards; 36 new cards and 38 supplied illustrations.")


if __name__ == "__main__":
    main()
