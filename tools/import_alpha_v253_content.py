"""Snapshot and reconcile the reviewed Alpha v2.5.3 card sources.

Use --design-dir once to snapshot the 10.22 source folder. Routine runs read only
the checked-in snapshot. --apply-reviewed-fields imports explicit source fields
into existing stable card definitions; it never invents executable effects.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from design_snapshot_archive import materialize_snapshot, snapshot_dir

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT = snapshot_dir(ROOT, "alpha-v253")
SOURCE_FILES = {
    "spells": "法术列表-共29+14+6张卡.md",
    "sique": "【味契】系列-共19张卡.md",
    "storm": "【恒风】系列-共15张卡.md",
    "formations": "阵法列表-共16张卡.md",
    "seals": "符文列表-共3张卡.md",
    "rules": "WizardCard玩家规则书.md",
    "changes": "10.22改动报告.md",
    "requirements": "Alpha v2.5.3更新需求报告.md",
}
REQUIREMENTS_PATH = Path(r"E:/Project/WizardCard-Design/Alpha v2.5.3更新需求报告.md")
ALIASES = {
    "四芒星": "small-hexagram", "六芒星": "hexagram", "五芒星": "pentagram-of-life",
    "八芒星": "octagram", "太阳": "sun", "月亮": "moon",
    "恩格亚斯之星": "engeas-four-point-star", "奥数圆环": "ring",
    "正十字（待定）": "upright-cross", "逆十字（待定）": "inverted-cross",
    "安卡十字（待定）": "ankh",
    "皇后": "arcana-empress", "皇帝": "arcana-emperor", "愚者": "arcana-fool",
    "魔术师": "arcana-magician", "女祭司": "arcana-priestess", "教皇": "arcana-hierophant",
    "恋人": "arcana-lovers", "战车": "arcana-chariot", "力量": "arcana-strength",
    "隐士": "arcana-hermit", "正义": "arcana-justice", "倒吊人": "arcana-hanged-man",
    "死神": "arcana-death", "节制": "arcana-temperance", "恶魔": "arcana-devil",
    "高塔": "arcana-tower", "世界": "arcana-world",
}
SCHOOL = dict(zip(
    "塑能 变化 咒法 惑控 幻术 防护 死灵 预言 阿卡那".split(),
    "evocation transmutation conjuration enchantment illusion abjuration necromancy divination arcana".split(),
))
TYPE = {"阵法卡": "formation", "符文卡": "seal", "解析法术卡": "analytic",
        "言灵法术卡": "word", "咒符法术卡": "talisman"}
TYPE_BY_SOURCE = {"formations": "formation", "seals": "seal"}
PENDING = {"upright-cross", "inverted-cross", "ankh"}
CONFIRMED_FIELDS = {
    "storm-crest": {"level": 1, "rings": 1, "capacity": 5, "income": 1},
}
REVIEWED_TEXT = {
    "sique-cauldron": "己方场上存在【味契·篝火】才能设置。此卡存在期间，己方只能发动或附着【味契】卡；其他解析法术仍可设置和准备施法。己方结束阶段开始时，将此卡上附着的全部符文送去灰烬区。",
    "sique-campfire": "己方场上存在【味契·魔力坩埚】时，此卡不受其他卡的效果影响。其上成功设置火焰伤害法术时，对对方造成1点火焰伤害。",
    "sique-mushroom-soup": "将己方场上1张【味契】符文送去灰烬区才能发动。恢复4点生命并获得6点临时生命。",
    "sique-pancakes": "将己方场上2张不同的【味契】符文送去灰烬区才能发动。直到下个己方准备阶段开始时，每次受到的伤害至多为4。",
    "sique-divine-stew": "将己方场上3张不同的【味契】符文送去灰烬区才能发动。本回合仅以对方普通生命替代己方魔素费用，按1:1在宣告时支付；对方至少保留1点普通生命。",
    "sique-flavor-shift": "将己方场上1张【味契】符文送去灰烬区才能发动。从卡组选择1张【味契】符文，附着到己方【味契·魔力坩埚】。",
    "sique-refire": "将己方灰烬区1至5张【味契】卡按类型返回主卡组或副卡组，之后可以抽1张卡。没有合法卡时跳过返回。",
    "sique-backfire": "己方场上存在【味契·魔力坩埚】才能发动。将对方灰烬区1至5张卡按类型返回主卡组或副卡组，对对方造成返回数量的火焰伤害，己方恢复返回数量×2的生命。没有合法卡时跳过返回且数量为0。",
    "sique-aroma-lure": "己方场上存在【味契·魔力坩埚】才能发动。双方手牌返回各自卡组并洗牌，双方分别可以抽取各自返回数量的卡牌。",
    "arcana-hanged-man": "宣告发动时，以普通生命为基数支付向上取整的一半生命。结算时双方分别交换各自灰烬区与卡组中的非阵法卡，阵法卡留在原区域；交换后的卡组洗牌。",
}
BEHAVIOR_TESTS = {
    "sique-mushroom-soup": "tests/sique.cpp", "sique-pancakes": "tests/sique.cpp",
    "sique-divine-stew": "tests/sique.cpp", "sique-flavor-shift": "tests/sique.cpp",
    "sique-refire": "tests/sique.cpp", "sique-backfire": "tests/sique.cpp",
    "sique-aroma-lure": "tests/sique.cpp", "sique-ingredients": "tests/sique.cpp",
    "sique-sugar": "tests/sique.cpp", "sique-cauldron": "tests/sique.cpp",
    "sique-campfire": "tests/sique.cpp",
    "arcana-hanged-man": "tests/sique.cpp [alpha-v253] (ordinary-life cost, declaration timing, nonformation exchange, debug immunity)",
    "elemental-ring": "tests/alpha_v253_rules.cpp (evocation damage source restriction)",
    "storm-annihilation-crest": "tests/alpha_v253_rules.cpp (both host types restricted)",
    "storm-crest": "tests/storm.cpp (counter tiers, activation and search); tests/alpha_v252_pool.cpp (formal recipe)",
    "arcana-magician": "tests/sique.cpp [alpha-v253] (opponent chooses among three distinct revealed names, rejects forged choices)",
}


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def snapshot_sources(design_dir: Path) -> None:
    source_dir = SNAPSHOT / "sources"
    source_dir.mkdir(parents=True, exist_ok=True)
    rows = []
    for key, filename in SOURCE_FILES.items():
        source = REQUIREMENTS_PATH if key == "requirements" else design_dir / filename
        if not source.is_file():
            if key == "requirements":
                continue
            raise FileNotFoundError(source)
        destination = source_dir / f"{key}.md"
        shutil.copyfile(source, destination)
        rows.append({"key": key, "original": str(source), "snapshot": f"sources/{key}.md",
                     "sha256": sha256(destination)})
    write_json(SNAPSHOT / "source-manifest.json", {"format": 1, "sources": rows})
    # Freeze the actual pre-update runtime pool, not an older snapshot's baseline.
    prior = SNAPSHOT / "previous-catalog.json"
    if not prior.exists():
        prior.write_bytes(subprocess.check_output(["git", "show", "HEAD:assets/cards.json"], cwd=ROOT))


def parse_cards() -> list[dict]:
    rows = []
    source_dir = SNAPSHOT / "sources"
    for key, filename in SOURCE_FILES.items():
        path = source_dir / f"{key}.md"
        if not path.is_file() or key in {"rules", "changes", "requirements"}:
            continue
        text = path.read_text(encoding="utf-8-sig")
        headings = list(re.finditer(r"(?m)^###\s+(.+?)\s*$", text))
        for index, heading in enumerate(headings):
            prefix = text[:heading.start()]
            sections = re.findall(r"(?m)^#\s+(.+?)\s*$", prefix)
            section = sections[-1].strip() if sections else ""
            body_start = heading.end()
            body_end = headings[index + 1].start() if index + 1 < len(headings) else len(text)
            name = heading.group(1).strip()
            body = text[body_start:body_end]
            fields = dict(re.findall(r"(?m)^\*\s*([^：\n]+)：([^\n]*)", body))
            effect = body.partition("* 效果：")[2]
            effect = re.sub(r"(?m)^#.*$", "", effect)
            effect_lines = [re.sub(r"^\s*[-*]\s*", "", re.sub(r"\*+", "", row)).strip()
                            for row in effect.splitlines()]
            effect_text = " ".join(row for row in effect_lines if row)
            card_type = TYPE_BY_SOURCE.get(key, TYPE.get(section))
            rows.append({"source": key, "section": card_type, "name": name,
                         "fields": fields, "effect": re.sub(r"\s+", " ", effect_text).strip()})
    return rows


def card_definitions() -> tuple[dict, dict]:
    catalog = read_json(ROOT / "assets/catalog.json")
    by_id = {}
    by_name = {}
    for row in catalog["definitions"]:
        card = read_json(ROOT / "assets" / row["file"])
        by_id[card["id"]] = card
        by_name[card["name"]] = card
    for path in (SNAPSHOT / "drafts/cards").glob("*.json"):
        card = read_json(path)
        by_id[card["id"]] = card
        by_name[card["name"]] = card
    return catalog, by_id


def resolve_id(row: dict, by_name: dict, by_id: dict) -> str | None:
    if row["name"] in ALIASES:
        return ALIASES[row["name"]]
    if row["name"] in by_name:
        return by_name[row["name"]]["id"]
    return None


def normalized(value: str) -> str:
    return re.sub(r"[\s。；，,.!?！？]+", "", value).replace("【", "").replace("】", "")


def desired_fields(row: dict, current: dict) -> dict:
    wanted = {"type": row["section"]}
    fields = row["fields"]

    def number(name):
        value = fields.get(name, "")
        match = re.search(r"\d+", value)
        return int(match.group()) if match and not re.search(r"[?？]", value) else None

    if current["type"] == "formation":
        for source, target in (("阵法等级", "level"), ("阵体", "body"), ("环位", "rings"),
                               ("荷载上限", "capacity"), ("魔素收入", "income")):
            value = number(source)
            if value is not None:
                wanted[target] = value
    elif current["type"] == "seal":
        value = number("附着费用")
        if value is not None:
            wanted["cost"] = value
    else:
        for source, target in (("法术阶级", "rank"), ("咒文速度", "speed"), ("解析时间", "analysisTurns")):
            value = number(source)
            if value is not None:
                wanted[target] = value
        school = fields.get("派系")
        if school in SCHOOL:
            wanted["school"] = SCHOOL[school]
        fee = fields.get("设置/发动费用", "")
        values = re.findall(r"\d+", fee)
        if values:
            wanted["cost"] = int(values[0])
            if current["type"] == "analytic" and len(values) > 1:
                wanted["castCost"] = int(values[1])
    wanted.update(CONFIRMED_FIELDS.get(current["id"], {}))
    wanted["text"] = REVIEWED_TEXT.get(current["id"], row["effect"])
    return wanted


def compare(rows: list[dict], catalog: dict, by_id: dict) -> list[dict]:
    by_name = {card["name"]: card for card in by_id.values()}
    seen = set()
    result = []
    for row in rows:
        card_id = resolve_id(row, by_name, by_id)
        if not card_id or card_id not in by_id:
            result.append({**row, "id": "UNMAPPED", "delta": ["stable ID"], "status": "BLOCKED: no stable ID"})
            continue
        card = by_id[card_id]
        deltas = []
        if card_id in seen:
            deltas.append("duplicate source mapping")
        seen.add(card_id)
        wanted = desired_fields(row, card)
        for key, value in wanted.items():
            if key == "text":
                if normalized(card.get(key, "")) != normalized(value):
                    deltas.append("text")
            elif key in card and card[key] != value:
                deltas.append(key)
        if card["name"] != row["name"]:
            deltas.append("name")
        name_confirm = row["name"] in {"奥数圆环", "恩格亚斯之星"}
        unresolved = card_id not in CONFIRMED_FIELDS and any(re.search(r"[?？]", value) for value in row["fields"].values())
        if card_id in PENDING:
            status = "DRAFT: source explicitly marks待定; excluded from runtime pool"
        elif unresolved:
            status = "BLOCKED: unresolved source fields"
        elif name_confirm:
            status = "CONFIRM: source/current display-name mismatch; stable ID preserved"
        else:
            status = "UPDATED" if deltas else "MATCHED"
        result.append({**row, "id": card_id, "delta": deltas, "status": status,
                       "number": card.get("number", ""), "current_name": card.get("name", ""),
                       "test": BEHAVIOR_TESTS.get(card_id, "tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership)")})
    return result


def render_report(rows: list[dict], compared: list[dict], by_id: dict) -> str:
    counts = {}
    for row in rows:
        counts[row["source"]] = counts.get(row["source"], 0) + 1
    catalog = read_json(ROOT / "assets/catalog.json")
    active = {row["id"] for row in catalog["definitions"]}
    previous = read_json(SNAPSHOT / "previous-catalog.json")
    previous_cards = {card["id"]: card for card in previous["cards"]}
    changed = {card_id: sorted(key for key in set(previous_cards[card_id]) | set(by_id[card_id])
                              if previous_cards[card_id].get(key) != by_id[card_id].get(key))
               for card_id in active & previous_cards.keys()}
    changed = {card_id: fields for card_id, fields in changed.items() if fields}
    duplicates = sorted({row["id"] for row in compared if sum(other["id"] == row["id"] for other in compared) > 1})
    art = {entry["id"]: entry["file"] for entry in read_json(ROOT / "godot/art/assets.json")["assets"]}
    def art_status(card_id):
        if card_id not in active:
            return "草稿，不发布"
        image = art.get("illustration." + by_id[card_id].get("illustrationId", ""))
        if not image or not (ROOT / "godot" / image.removeprefix("res://")).is_file():
            raise ValueError(f"{card_id}/illustrationId: missing mapped runtime image {image}")
        return f"完整 `{image}`"
    lines = [
        "# Alpha v2.5.3 卡池逐卡差异与来源记录", "",
        "来源快照位于本目录 `sources/`，其原始文件路径与 SHA-256 在 `source-manifest.json`。工具 `tools/import_alpha_v253_content.py` 从快照读取，只有显式传入 `--apply-reviewed-fields` 才修改卡牌 JSON；该导入器不从效果文案推导可执行效果。", "",
        f"本次源文件标题数：{len(rows)}；正式卡牌：{len(active)}；待定草稿：{len(PENDING)}。源文件计数：" + "；".join(f"{key} {value}" for key, value in counts.items()) + "。", "",
        f"对比基线为更新前已提交的规则/卡池 {previous['rulesVersion']}/{previous['cardSetVersion']}，其 102 张定义保存在 `previous-catalog.json`。新增：" + ("、".join(sorted(active - previous_cards.keys())) or "无") + f"；更新：{len(changed)} 张；退池：" + "、".join(sorted(previous_cards.keys() - active)) + "（三张待定草稿）；正式重命名：无；重复来源 ID：" + ("、".join(duplicates) or "无") + "。", "",
        "## 待确认与阻断", "",
        "- 正十字、逆十字、安卡十字：源表标为待定且效果栏为空，不进入正式卡池。",
        "- 恒风·风暴纹章：源字段问号已由用户确认沿用现行档位；初始0指示物，0–2为等级1/环1/载5/收1，3–4为等级2/环2/载8/收1，5以上为等级3/环3/载10/收2。",
        "- 奥数圆环/奥术圆环、恩格亚斯之星/恩格亚斯四角星：来源与现行名称不同，稳定 ID 保留，等待设计方确认名称。",
        "- 神恩炖肉：用户确认仅替代魔素，按1:1在宣告时支付对方普通生命且至少保留1生命；不替代符文、生命等额外成本。倒吊人：用户确认按普通生命向上取整一半并在宣告时支付；交换仅影响非阵法卡，沿用现行洗牌。", "",
        "- 糖浆煎饼按已确认通用结算：每个普通伤害事件先处理免疫、抗性及减伤，再限制到最多4点，最后临时生命吸收、普通生命扣减；多段伤害分别处理，过载和枯竭真实伤害不受本上限影响。下个己方准备阶段开始时失效。",
        "- 飘香诱惑按已确认通用结算：双方手牌分别返回并洗牌，再分别选择是否抽取各自实际返回数量；每次抽牌统一经过手牌上限、抽牌触发及卡组不足时的枯竭规则。完整发布口径见 `docs/card-migration-2.5.3.md`。", "",
        "## 逐卡记录", "",
        "| 来源/卡名 | ID | 编号 | 状态/效果实现 | 相对2.5.2变更 | 来源尚存差异 | 图像状态 | 测试/处理记录 |", "|---|---|---:|---|---|---|---|---|",
    ]
    for row in compared:
        source = row["source"]
        display = row["name"].replace("|", "\\|")
        card_id = row["id"]
        effect = "草稿，禁用" if card_id in PENDING else "权威结构化效果/机制"
        baseline = "退池" if card_id in PENDING else ", ".join(changed.get(card_id, [])) or "无"
        lines.append(f"| `sources/{source}.md` {display} | `{card_id}` | {row.get('number', '')} | {row['status']}；{effect} | {baseline} | {', '.join(row.get('delta', [])) or '无'} | {art_status(card_id)} | {row.get('test', 'unmapped')} |")
    lines += ["", "## 计数核对", "", "| 来源 | 标题数 |", "|---|---:|"]
    lines += [f"| {key} | {value} |" for key, value in counts.items()]
    lines += ["| 去重前总条目 | " + str(len(rows)) + " |", "| 待定不发布 | 3 |", "| 正式候选（同 ID 去重） | " + str(len(active)) + " |", ""]
    return "\n".join(lines)


def apply_fields(compared: list[dict], by_id: dict) -> None:
    for row in compared:
        card_id = row["id"]
        if card_id not in by_id or card_id in PENDING or row["status"].startswith("BLOCKED"):
            continue
        card = by_id[card_id]
        wanted = desired_fields(row, card)
        card.update(wanted)
        manifest = read_json(ROOT / "assets/catalog.json")
        definition = next(entry for entry in manifest["definitions"] if entry["id"] == card_id)
        if card_id == "snow-rain-storm":
            # Stable ID/art identity survive the source's explicit type change.
            card["number"] = "040013"
            old = ROOT / "assets" / definition["file"]
            definition["file"] = "cards/talisman/snow-rain-storm.json"
            if old != ROOT / "assets" / definition["file"]:
                old.unlink()
            write_json(ROOT / "assets/catalog.json", manifest)
        write_json(ROOT / "assets" / definition["file"], card)


def exclude_pending() -> None:
    catalog_path = ROOT / "assets/catalog.json"
    catalog = read_json(catalog_path)
    removed = []
    retained = []
    (SNAPSHOT / "drafts/cards").mkdir(parents=True, exist_ok=True)
    for row in catalog["definitions"]:
        if row["id"] in PENDING:
            destination = SNAPSHOT / "drafts/cards" / Path(row["file"]).name
            shutil.copyfile(ROOT / "assets" / row["file"], destination)
            removed.append(row)
        else:
            retained.append(row)
    existing_drafts = {read_json(path)["id"] for path in (SNAPSHOT / "drafts/cards").glob("*.json")}
    if {row["id"] for row in removed} | existing_drafts != PENDING:
        raise SystemExit("Pending formation definitions changed; review the draft list before exclusion")
    for filename in ("assets/side-deck.json", "assets/deck.json", "assets/presets.json"):
        data = read_json(ROOT / filename)
        encoded = json.dumps(data, ensure_ascii=False)
        if any(card_id in encoded for card_id in PENDING):
            raise SystemExit(f"Cannot exclude pending cards referenced by {filename}")
    catalog["definitions"] = retained
    write_json(catalog_path, catalog)
    previous_drafts = read_json(SNAPSHOT / "draft-cards.json").get("definitions", []) if (SNAPSHOT / "draft-cards.json").exists() else []
    definitions = {row["id"]: row for row in previous_drafts + removed}
    write_json(SNAPSHOT / "draft-cards.json", {"status": "draft", "excludedFromCatalog": sorted(PENDING),
                                                "definitions": [definitions[key] for key in sorted(definitions)]})


def main() -> None:
    global SNAPSHOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--design-dir", type=Path, help="copy raw 10.22 sources into the reviewed snapshot")
    parser.add_argument("--apply-reviewed-fields", action="store_true", help="import explicit simple source fields and card text")
    parser.add_argument("--exclude-pending-cards", action="store_true", help="remove the three confirmed drafts from the formal catalog")
    args = parser.parse_args()
    if args.design_dir:
        SNAPSHOT = materialize_snapshot(ROOT, "alpha-v253")
        snapshot_sources(args.design_dir)
    if args.exclude_pending_cards:
        exclude_pending()
    rows = parse_cards()
    if not rows:
        raise SystemExit("No card source snapshots found; run with --design-dir once")
    catalog, by_id = card_definitions()
    compared = compare(rows, catalog, by_id)
    if args.apply_reviewed_fields:
        apply_fields(compared, by_id)
        catalog, by_id = card_definitions()
        compared = compare(rows, catalog, by_id)
    (SNAPSHOT / "card-diff-report.md").write_text(render_report(rows, compared, by_id), encoding="utf-8")
    if not (SNAPSHOT / "draft-cards.json").is_file():
        write_json(SNAPSHOT / "draft-cards.json", {"status": "draft", "excludedFromCatalog": sorted(PENDING),
                                                    "cards": [row for row in compared if row["id"] in PENDING]})
    print(f"Reviewed {len(rows)} source entries; {len(catalog['definitions'])} formal cards; report: {SNAPSHOT / 'card-diff-report.md'}")


if __name__ == "__main__":
    main()
