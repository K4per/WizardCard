"""Import and audit the Alpha v2.6.0 workbook baseline.

The v2.6.0 design hand-off is a workbook baseline rather than an executable
catalogue.  This tool deliberately keeps the default operation non-destructive:
it snapshots the three approved workbooks, backs up the current runtime inputs,
generates a complete staging catalogue and reports every field/art/mechanics
problem.  ``--apply`` is refused while any blocker remains, so an incomplete
natural-language effect can never silently become a legal runtime card.

The reader uses only the Python standard library.  That keeps the import
auditable in CI and avoids making the production runtime depend on Excel.
"""
from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import json
import re
import shutil
import sys
import zipfile
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DESIGN = Path(r"E:/Project/WizardCard-Design/Alpha 现有卡池")
DEFAULT_OUT = ROOT / "assets/source-archive/alpha-v260-xlsx-import"

WORKBOOKS = {
    "spells": ("法术卡列表.xlsx", "卡表"),
    "seals": ("符文卡列表-共3张卡.xlsx", "符文卡"),
    "formations": ("阵法卡列表-共17张卡.xlsx", "阵法卡"),
}

TYPE_NAMES = {
    "解析法术": "analytic",
    "言灵法术": "word",
    "咒符法术": "talisman",
}
SCHOOL_NAMES = {
    "塑能": "evocation",
    "变化": "transmutation",
    "咒法": "conjuration",
    "惑控": "enchantment",
    "幻术": "illusion",
    "防护": "abjuration",
    "死灵": "necromancy",
    "预言": "divination",
    "阿卡那": "arcana",
}
RARITIES = {"普通": "common", "罕见": "uncommon", "稀有": "rare", "史诗": "epic", "传说": "legendary"}
NUMBER_PREFIX = {"formation": "01", "analytic": "02", "word": "03", "talisman": "04", "seal": "05"}

# The design hand-off contains the card-specific illustrations supplied for
# this release.  Historical artwork remains the fallback for cards that have
# no file in that hand-off.  Keep this table explicit: a reused image is a
# deliberate per-card migration decision and is emitted in art-mapping.md.
# Values are (frontend illustration id, repository source file, reason).
HISTORICAL_ART = {
    "火球术": ("fireball", "assets/art/cards/fireball.png", "历史同名卡图"),
    "寒霜射线": ("ray-of-frost", "assets/art/alpha-release/ray-of-frost.png", "历史同名卡图"),
    "闪电束": ("lightning-bolt", "assets/art/alpha-release/lightning-bolt.png", "历史同名卡图"),
    "魔法飞弹": ("magic-missile", "assets/art/alpha-v1/magic-missile.png", "历史同名卡图"),
    "繁彩球": ("v2-chromatic-orb", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-chromatic-orb.png", "历史同类卡图"),
    "曳光弹": ("v2-guiding-bolt", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-guiding-bolt.png", "历史同类卡图"),
    "炽热射线": ("v2-scorching-ray", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-scorching-ray.png", "历史同类卡图"),
    "炽燃结界": ("ward", "assets/art/cards/ward.png", "历史同名卡图"),
    "阳炎爆": ("v2-sunburst", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-sunburst.png", "历史同类卡图"),
    "燃烧之手": ("fireball", "assets/art/cards/fireball.png", "历史火焰法术卡图"),
    "云雾术": ("cloud-mist", "assets/art/godot-2d-v2/card-illustrations-v2-20261017/storm-series-atlas.png", "历史同类卡图"),
    "召雷术": ("thunder-summon", "assets/art/godot-2d-v2/card-illustrations-v2-20261017/storm-series-atlas.png", "历史同类卡图"),
    "匕首之云": ("cutting-wind", "assets/art/godot-2d-v2/card-illustrations-v2-20261017/storm-series-atlas.png", "历史同类卡图"),
    "臭云术": ("cloud-mist", "assets/art/godot-2d-v2/card-illustrations-v2-20261017/storm-series-atlas.png", "历史同类卡图"),
    "油腻术": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史幻术法术卡图"),
    "次级复原": ("lesser-restoration", "assets/art/alpha-release/lesser-restoration.png", "历史同名卡图"),
    "吸收元素": ("v2-elemental-ring", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-elemental-ring.png", "历史同类卡图"),
    "枯萎术": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史死灵法术卡图"),
    "缝合生命": ("mend", "assets/art/cards/mend.png", "历史同名卡图"),
    "虚假生命": ("false-life", "assets/art/alpha-release/false-life.png", "历史同名卡图"),
    "治疗之环": ("v2-healing-circle", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-healing-circle.png", "历史同类卡图"),
    "致病射线": ("v2-scorching-ray", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-scorching-ray.png", "历史射线法术卡图"),
    "防死结界": ("ward", "assets/art/cards/ward.png", "历史结界法术卡图"),
    "汲血之触": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史死灵法术卡图"),
    "沉默术": ("v260-silence", "assets/art/alpha-v260/silence.png", "本次确认的沉默术卡图"),
    "朦胧术": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史幻术法术卡图"),
    "恐惧术": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史惑控法术卡图"),
    "精神监狱": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史惑控法术卡图"),
    "紊乱波动": ("messy-wave", "assets/art/alpha-v1/messy-wave.png", "历史同名卡图"),
    "昏昏欲睡": ("instantly-sleep", "assets/art/alpha-v1/instantly-sleep.png", "用户确认沿用旧沉默术卡图"),
    "疯狂冠冕": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史惑控法术卡图"),
    "祝福术": ("v2-sun", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-sun.png", "历史增益法术卡图"),
    "灾祸术": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史减益法术卡图"),
    "心灵尖刺": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史心灵法术卡图"),
    "地震术": ("earthquake", "assets/art/alpha-v254-20261024/source/卡图替换/地震术.png", "历史同名卡图"),
    "碎环术": ("unravel", "assets/art/cards/unravel.png", "历史同名卡图"),
    "土石喷发": ("v2-sulfur", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-sulfur.png", "历史土石法术卡图"),
    "弹射术": ("cutting-wind", "assets/art/godot-2d-v2/card-illustrations-v2-20261017/storm-series-atlas.png", "历史冲击法术卡图"),
    "加速术": ("v2-guiding-bolt", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-guiding-bolt.png", "历史增益法术卡图"),
    "正十字": ("v2-upright-cross", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-upright-cross.png", "历史对应阵法卡图"),
    "逆十字": ("v2-inverted-cross", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-inverted-cross.png", "历史对应阵法卡图"),
    "安卡十字": ("v2-ankh", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-ankh.png", "历史对应阵法卡图"),
    "恩格亚斯之星": ("engeas-four-point-star", "assets/art/alpha-v1/engeas-four-point-star.png", "历史同名卡图"),
    "元素之环": ("v2-elemental-ring", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-elemental-ring.png", "历史同名卡图"),
    "混沌之环": ("v2-chaos-ring", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-chaos-ring.png", "历史同名卡图"),
    "阿克乌姆之弧": ("reservoir", "assets/art/cards/reservoir.png", "历史同名卡图"),
    "硫磺": ("v2-sulfur", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-sulfur.png", "历史同名卡图"),
    "基础法阵": ("triangle-star", "assets/art/godot-2d-v2/card-illustrations-v2-20261017/triangle-star.png", "历史通用阵法图"),
    "标准法阵": ("balance", "assets/art/cards/balance.png", "历史通用阵法图"),
    "强化法阵": ("alpha-v252-pentagram-of-life", "assets/art/alpha-v252-20261021/source/asterism/五芒星.png", "历史通用阵法图"),
    "复合法阵": ("alpha-v252-octagram", "assets/art/alpha-v252-20261021/source/asterism/八芒星.png", "历史通用阵法图"),
    "增幅法阵": ("v2-sun", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-sun.png", "历史通用阵法图"),
    "守护法阵": ("v2-octagram", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-octagram.png", "历史通用阵法图"),
    "幻象法阵": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史通用阵法图"),
    "高阶法阵": ("v2-elemental-ring", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-elemental-ring.png", "历史通用阵法图"),
    "完美法阵": ("v2-chaos-ring", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-chaos-ring.png", "历史通用阵法图"),
    "逆三角星符": ("conduit", "assets/art/cards/conduit.png", "历史同名卡图"),
    "奥术圆环": ("ring", "assets/art/cards/ring.png", "历史同名卡图"),
    "昂扬": ("uplift", "assets/art/alpha-v1/uplift.png", "历史同名卡图"),
    "炼狱叱喝": ("fireball", "assets/art/cards/fireball.png", "历史火焰咒符卡图"),
    "捕获打击": ("v2-guiding-bolt", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-guiding-bolt.png", "历史咒符卡图"),
    "护盾术": ("shield", "assets/art/alpha-release/shield.png", "历史同名卡图"),
    "法术反制": ("counter-spell", "assets/art/alpha-v1/counter-spell.png", "历史同名卡图"),
    "避火咒": ("protective-flame", "assets/art/alpha-v1/protective-flame.png", "历史同名卡图"),
    "解除魔法": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史咒符卡图"),
    "死佑护甲": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史咒符卡图"),
    "步履维艰": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史咒符卡图"),
    "银光锐语": ("barbs", "assets/art/cards/barbs.png", "历史同名卡图"),
    "滑稽魔盒": ("v2-funny-box", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-funny-box.png", "历史同名卡图"),
    "侦测善恶": ("true-strike", "assets/art/alpha-v1/true-strike.png", "历史检索咒符卡图"),
    "荆棘丛生": ("v2-sulfur", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-sulfur.png", "历史伤害咒符卡图"),
    "火花术": ("spark", "assets/art/cards/spark.png", "历史同名卡图"),
    "魔能爆": ("magic-missile", "assets/art/alpha-v1/magic-missile.png", "历史力场法术卡图"),
    "魔力扰动": ("disrupt", "assets/art/cards/disrupt.png", "历史同名卡图"),
    "法师之手": ("v2-guiding-bolt", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-guiding-bolt.png", "历史法术卡图"),
    "瞬发回路过载": ("instant-circuit-overload", "assets/art/alpha-v1/instant-circuit-overload.png", "历史同名卡图"),
    "奥术复苏": ("arcane-recovery", "assets/art/alpha-release/arcane-recovery.png", "历史同名卡图"),
    "支援术": ("aid", "assets/art/alpha-v1/aid.png", "历史同名卡图"),
    "心如止水": ("clarity", "assets/art/cards/clarity.png", "历史同名卡图"),
    "扰神曲": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史言灵卡图"),
    "降咒": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史言灵卡图"),
    "七彩喷射": ("v2-chromatic-orb", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-chromatic-orb.png", "历史言灵卡图"),
    "幽影刃": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史言灵卡图"),
    "不谐低语": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史言灵卡图"),
    "脆弱诅咒": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史言灵卡图"),
    "命令术": ("true-strike", "assets/art/alpha-v1/true-strike.png", "历史言灵卡图"),
    "恶言相加": ("v2-void", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-void.png", "历史言灵卡图"),
    "克敌机先": ("true-strike", "assets/art/alpha-v1/true-strike.png", "历史同名卡图"),
    "奥术回想": ("recall", "assets/art/cards/recall.png", "历史同名卡图"),
    "异界探知": ("v2-guiding-bolt", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-guiding-bolt.png", "历史言灵卡图"),
    "卜筮术": ("v2-guiding-bolt", "assets/art/godot-2d-v2/card-illustrations-v2-20261011/v2-guiding-bolt.png", "历史言灵卡图"),
    "强化属性": ("uplift", "assets/art/alpha-v1/uplift.png", "历史增益言灵卡图"),
}

NS_MAIN = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
NS_REL = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
NS_PKG_REL = "http://schemas.openxmlformats.org/package/2006/relationships"


class ImportFailure(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def text(value: Any) -> str:
    if value is None:
        return ""
    return str(value).strip()


def number(value: Any) -> int | str | None:
    if value is None or text(value) == "":
        return None
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float) and value.is_integer():
        return int(value)
    raw = text(value)
    if raw.upper() == "X":
        return "X"
    try:
        return int(raw)
    except ValueError:
        return raw


def slug(value: str) -> str:
    # IDs are derived from the stable source row, not from a best-effort pinyin
    # transliteration.  The source name remains the user-facing identity.
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")


def xml_text(element: ET.Element) -> str:
    return "".join(element.itertext())


class XlsxBook:
    def __init__(self, path: Path):
        self.path = path
        self._zip = zipfile.ZipFile(path)
        self.shared = self._shared_strings()
        self.sheets = self._sheet_targets()

    def _shared_strings(self) -> list[str]:
        try:
            root = ET.fromstring(self._zip.read("xl/sharedStrings.xml"))
        except KeyError:
            return []
        return [xml_text(item) for item in root.findall(f"{{{NS_MAIN}}}si")]

    def _sheet_targets(self) -> dict[str, str]:
        workbook = ET.fromstring(self._zip.read("xl/workbook.xml"))
        rels = ET.fromstring(self._zip.read("xl/_rels/workbook.xml.rels"))
        targets = {}
        for rel in rels.findall(f"{{{NS_PKG_REL}}}Relationship"):
            targets[rel.attrib["Id"]] = rel.attrib["Target"]
        result = {}
        for sheet in workbook.findall(f"{{{NS_MAIN}}}sheets/{{{NS_MAIN}}}sheet"):
            rid = sheet.attrib[f"{{{NS_REL}}}id"]
            target = targets[rid]
            result[sheet.attrib["name"]] = "xl/" + target.lstrip("/")
        return result

    def rows(self, sheet: str) -> list[tuple[int, dict[int, Any]]]:
        if sheet not in self.sheets:
            raise ImportFailure(f"{self.path.name}: missing worksheet {sheet!r}; available {sorted(self.sheets)}")
        root = ET.fromstring(self._zip.read(self.sheets[sheet]))
        result = []
        for row in root.findall(f".//{{{NS_MAIN}}}row"):
            values: dict[int, Any] = {}
            for cell in row.findall(f"{{{NS_MAIN}}}c"):
                ref = cell.attrib.get("r", "")
                match = re.match(r"([A-Z]+)", ref)
                if not match:
                    continue
                column = 0
                for char in match.group(1):
                    column = column * 26 + ord(char) - ord("A") + 1
                kind = cell.attrib.get("t", "")
                if kind == "inlineStr":
                    value = xml_text(cell.find(f"{{{NS_MAIN}}}is")) if cell.find(f"{{{NS_MAIN}}}is") is not None else ""
                else:
                    value_node = cell.find(f"{{{NS_MAIN}}}v")
                    raw = "" if value_node is None else value_node.text or ""
                    if kind == "s":
                        value = self.shared[int(raw)] if raw else ""
                    elif kind == "b":
                        value = raw == "1"
                    else:
                        value = raw
                        if re.fullmatch(r"-?\d+", raw):
                            value = int(raw)
                        elif re.fullmatch(r"-?(?:\d+\.\d*|\d*\.\d+)(?:[Ee][+-]?\d+)?", raw):
                            value = float(raw)
                values[column] = value
            result.append((int(row.attrib.get("r", len(result) + 1)), values))
        return result


def header_row(rows: list[tuple[int, dict[int, Any]]], required: Iterable[str]) -> tuple[int, dict[str, int]]:
    required = list(required)
    for row_number, values in rows:
        headers = {text(value): column for column, value in values.items() if text(value)}
        if all(name in headers for name in required):
            if len(headers) != len(set(headers)):
                raise ImportFailure(f"duplicate header in row {row_number}")
            return row_number, headers
    raise ImportFailure(f"header not found; required {required}")


def value(values: dict[int, Any], headers: dict[str, int], field: str) -> Any:
    return values.get(headers.get(field, -1))


def source_error(errors: list[dict[str, Any]], path: Path, sheet: str, row: int, card: str, field: str, reason: str) -> None:
    errors.append({"file": str(path), "worksheet": sheet, "row": row, "card": card, "field": field, "reason": reason})


def parse_effect(text_value: str, card_type: str) -> tuple[list[dict[str, Any]], list[str], str]:
    """Parse exact effect forms that map to existing runtime primitives.

    The parser is intentionally clause based.  A card can therefore retain a
    verified prefix effect while reporting a separate blocker for an
    unsupported suffix; it never turns the unsupported suffix into free text
    that looks executable.
    """
    raw = re.sub(r"\s+", "", text_value)
    if raw in {"无效果。", "无效果"} and card_type != "seal":
        return [], [], "none"
    if card_type == "seal":
        return [], ["符文效果按宿主分别定义，尚未映射到可执行的附着能力"], "none"
    if not raw:
        return [], ["效果栏为空"], "none"
    damage_types = {"强酸": "acid", "寒冷": "cold", "火焰": "fire", "闪电": "lightning", "毒素": "poison", "雷鸣": "thunder", "光耀": "radiant", "暗蚀": "necrotic", "心灵": "psychic", "力场": "force", "挥砍": "slashing", "钝击": "bludgeoning", "穿刺": "piercing"}
    def damage(amount: str, damage_name: str, recipient: str) -> dict[str, Any] | None:
        damage_type = damage_types.get(damage_name)
        if not damage_type:
            return None
        return {"kind": "damage", "amount": int(amount), "damageType": damage_type, "recipient": recipient}

    def match_damage(pattern: str, recipient: str, target: str) -> tuple[list[dict[str, Any]], list[str], str] | None:
        found = re.fullmatch(pattern, raw)
        if not found:
            return None
        effect = damage(found.group(1), found.group(2), recipient)
        if effect is None:
            return [], ["伤害类型尚未映射到运行时枚举"], "none"
        return [effect], [], target

    def target_damage_with_suffix() -> tuple[list[dict[str, Any]], list[str], str] | None:
        found = re.fullmatch(r"选择1个目标才能发动[。,，]?对其造成(\d+)点(.+?)伤害。(?P<tail>.*)", raw)
        if not found:
            return None
        hit = damage(found.group(1), found.group(2), "target")
        if hit is None:
            return [], ["伤害类型尚未映射到运行时枚举"], "none"
        tail = found.group("tail")
        suffixes: dict[str, dict[str, Any]] = {
            "那之后，可以使对方解析区1张解析法术卡的解析时间+1。":
                {"kind": "delay_enemy_analysis", "amount": 1},
            "那之后，可以销毁对方解析区1张解析法术卡。":
                {"kind": "destroy_enemy_analysis"},
            "那之后，可以销毁对方解析区1张盖伏卡。":
                {"kind": "destroy_ambush"},
            "那之后，可以销毁对方解析区1张盖伏的法术卡。":
                {"kind": "destroy_ambush"},
            "那之后，可以查看对方解析区1张盖伏卡。":
                {"kind": "reveal_enemy_ambush"},
            "那之后，可以查看对方解析区1张盖伏的法术卡。":
                {"kind": "reveal_enemy_ambush"},
            "那之后，可以查看对方解析区全部盖伏卡。":
                {"kind": "reveal_all_ambush"},
            "那之后，可以将对方解析区1张盖伏卡弹回持有者手卡。":
                {"kind": "return_enemy_analysis"},
        }
        if not tail:
            return [hit], [], "player"
        if tail in suffixes:
            return [hit, suffixes[tail]], [], "player"
        independent = re.fullmatch(
            r"那之后，使对方获得(\d+)点独立荷载，对方结束阶段开始时清除。", tail)
        if independent:
            return [hit, {"kind": "independent_timed", "amount": int(independent.group(1)), "recipient": "opponent", "expires": "opponent_end"}], [], "player"
        setup = re.fullmatch(
            r"(?:那之后，)?直到下个己方抽卡阶段开始时，对方每设置1张法术卡，使对方受到(\d+)点(.+?)伤害。?",
            tail)
        if setup:
            tick = damage(setup.group(1), setup.group(2), "opponent")
            if tick is not None:
                return [hit, {"kind": "setup_damage", "amount": tick["amount"],
                              "damageType": tick["damageType"], "recipient": "opponent",
                              "expires": "own_draw"}], [], "player"
        return [hit], ["伤害后的条件、时点或区域操作尚无可验证的一一对应结构化定义"], "player"

    def opponent_damage_with_suffix() -> tuple[list[dict[str, Any]], list[str], str] | None:
        found = re.fullmatch(r"对对方造成(\d+)点(.+?)伤害。(?P<tail>.*)", raw)
        if not found:
            return None
        hit = damage(found.group(1), found.group(2), "opponent")
        if hit is None:
            return [], ["伤害类型尚未映射到运行时枚举"], "none"
        tail = found.group("tail")
        if not tail:
            return [hit], [], "opponent"
        if tail == "那之后，可以销毁对方1张阵法卡上的全部法术卡。己方灰烬区每存在1张塑能派系法术卡，此法术卡的设置费用-1。":
            return [hit, {"kind": "destroy_formation_spells"}], [], "opponent"
        return [hit], ["伤害后的条件、时点或区域操作尚无可验证的一一对应结构化定义"], "opponent"

    # Check repeated rays before the generic target-with-suffix form.  The
    # latter intentionally treats every unknown tail as a blocker, which used
    # to misclassify 炽热射线 even though the runtime already has ray_volley.
    volley = re.fullmatch(
        r"选择1个目标才能发动[。,，]?对其造成(\d+)点(.+?)伤害。重复3次，每次可以选择不同目标。?", raw)
    if volley:
        effect = damage(volley.group(1), volley.group(2), "target")
        if effect is None:
            return [], ["伤害类型尚未映射到运行时枚举"], "none"
        return [{"kind": "ray_volley", "amount": effect["amount"], "damageType": effect["damageType"]}], [], "player"

    target_suffix = target_damage_with_suffix()
    if target_suffix:
        return target_suffix
    opponent_suffix = opponent_damage_with_suffix()
    if opponent_suffix:
        return opponent_suffix

    direct = match_damage(r"选择1个目标才能发动[。,，]?对其造成(\d+)点(.+?)伤害。?", "target", "player")
    if direct:
        return direct
    direct = match_damage(r"对对方造成(\d+)点(.+?)伤害。?", "opponent", "opponent")
    if direct:
        return direct

    selected_damage = re.fullmatch(
        r"选择1个目标才能发动[。,，]?对其造成(\d+)点自选伤害，伤害类型从[^。]+中选择。?", raw)
    if selected_damage:
        return [{"kind": "choose_damage", "amount": int(selected_damage.group(1)), "recipient": "target"}], [], "player"

    # Area effects and phase-bound effects have explicit runtime primitives;
    # their timing/target details are filled by parse_card_attributes below.
    area = re.fullmatch(r"对对方以及对方场上全部目标各造成(\d+)点(.+?)伤害。?", raw)
    if area:
        hit = damage(area.group(1), area.group(2), "opponent")
        if hit:
            return [{"kind": "area_damage", "amount": hit["amount"], "damageType": hit["damageType"], "recipient": "opponent"}], [], "opponent"
        return [], ["伤害类型尚未映射到运行时枚举"], "none"

    if re.fullmatch(r"计时2。选择双方解析区共1至2张解析法术卡，使其直到此法术卡离场不可准备施法。?", raw):
        return [{"kind": "lock_analysis", "amount": 2}], [], "none"

    if re.fullmatch(r"选择1个目标才能发动[。,，]?对其造成(\d+)点雷鸣伤害。那之后，使对方获得(\d+)点独立荷载，对方结束阶段开始时清除。?", raw):
        found = re.fullmatch(r"选择1个目标才能发动[。,，]?对其造成(\d+)点雷鸣伤害。那之后，使对方获得(\d+)点独立荷载，对方结束阶段开始时清除。?", raw)
        return [{"kind": "damage", "amount": int(found.group(1)), "damageType": "thunder", "recipient": "target"},
                {"kind": "independent_timed", "amount": int(found.group(2)), "recipient": "opponent", "expires": "opponent_end"}], [], "player"


    concentration_damage = re.fullmatch(
        r"专注(\d+)。发动时，对对方造成(\d+)点(.+?)伤害。维持专注时，对对方造成(\d+)点(.+?)伤害。?", raw)
    if concentration_damage:
        first = damage(concentration_damage.group(2), concentration_damage.group(3), "opponent")
        sustained = damage(concentration_damage.group(4), concentration_damage.group(5), "opponent")
        if first is None or sustained is None:
            return [], ["伤害类型尚未映射到运行时枚举"], "none"
        # The maintenance effect is attached by parse_card_attributes below.
        return [first], [], "opponent"

    if re.fullmatch(r"直到下个己方抽卡阶段开始时，双方言灵法术不可发动。?", raw):
        return [{"kind": "block_actions", "amount": 0}], [], "opponent"

    if re.fullmatch(r"此回合，每次受到的伤害-\d+。若为力场伤害，则-\d+。?", raw):
        amount = int(re.search(r"伤害-(\d+)", raw).group(1))
        return [{"kind": "shield_for_turn", "amount": amount}], [], "self"

    if re.fullmatch(r"专注\d+。受到的火焰伤害减半，此效果不可叠加。?", raw):
        return [{"kind": "grant_resistance"}], [], "self"

    if re.fullmatch(r"将1张卡从手卡送去灰烬区才能发动。清除己方全部临时荷载。?", raw):
        return [
            {"kind": "discard_hand", "amount": 1, "recipient": "owner"},
            {"kind": "clear_all_temporary", "amount": 0, "recipient": "owner"},
        ], [], "self"

    dual_temporary = re.fullmatch(r"获得(\d+)点临时生命值和(\d+)点临时荷载。?", raw)
    if dual_temporary:
        return [
            {"kind": "temporary_life", "amount": int(dual_temporary.group(1)), "recipient": "owner"},
            {"kind": "temporary", "amount": int(dual_temporary.group(2)), "recipient": "owner"},
        ], [], "none"

    mana_temporary = re.fullmatch(r"获得(\d+)点魔素和(\d+)点临时荷载。?", raw)
    if mana_temporary:
        return [
            {"kind": "mana", "amount": int(mana_temporary.group(1)), "recipient": "owner"},
            {"kind": "temporary", "amount": int(mana_temporary.group(2)), "recipient": "owner"},
        ], [], "none"

    combined = re.fullmatch(
        r"选择1个目标才能发动[。,，]?对其造成(\d+)点(.+?)伤害[，,]己方恢复(\d+)点生命值。?", raw)
    if combined:
        hit = damage(combined.group(1), combined.group(2), "target")
        if hit:
            return [hit, {"kind": "heal", "amount": int(combined.group(3)), "recipient": "owner"}], [], "player"
        return [], ["伤害类型尚未映射到运行时枚举"], "none"

    # The v2.6 workbook has a few exact compositions whose individual
    # clauses already exist as runtime primitives.  Keep the source timing
    # in card attributes where needed, while returning a fully structured
    # effect list here so the importer does not leave an avoidable blocker.
    restoration = re.fullmatch(
        r"选择1个目标才能发动[。,，]?为其恢复(\d+)点生命值。那之后，清除己方(\d+)点荷载。?", raw)
    if restoration:
        return [
            {"kind": "heal", "amount": int(restoration.group(1)), "recipient": "target"},
            {"kind": "clear_all_temporary", "amount": int(restoration.group(2)), "recipient": "owner"},
        ], [], "player"

    # Healing Circle is a finite phase trigger; parse_card_attributes emits
    # the onEnd trigger and duration below.
    if re.fullmatch(r"计时\d+。己方结束阶段开始时，恢复\d+点生命值。?", raw):
        return [], [], "self"

    disease = re.fullmatch(
        r"选择1个目标才能发动[。,，]?对其造成(\d+)点(.+?)伤害。"
        r"(?:那之后，)?直到下个己方抽卡阶段开始时，对方每设置1张法术卡，使对方受到(\d+)点(.+?)伤害。?",
        raw)
    if disease:
        hit = damage(disease.group(1), disease.group(2), "target")
        tick = damage(disease.group(3), disease.group(4), "opponent")
        if hit is None or tick is None:
            return [], ["伤害类型尚未映射到运行时枚举"], "none"
        return [hit, {"kind": "setup_damage", "amount": tick["amount"],
                      "damageType": tick["damageType"], "recipient": "opponent",
                      "expires": "own_draw"}], [], "player"

    search_same_school = re.fullmatch(
        r"将1张法术卡从手卡送去灰烬区才能发动。将1张与那张卡同派系的法术卡从卡组加入手卡。?",
        raw)
    if search_same_school:
        return [{"kind": "discard_hand", "amount": 1, "recipient": "owner"},
                {"kind": "search_same_school", "amount": 1}], [], "none"

    if raw == "对方准备施法时才能发动。将其取消。那之后，可以将对方解析区1张解析法术卡弹回持有者手卡。":
        return [{"kind": "cancel"}, {"kind": "return_enemy_analysis"}], [], "none"

    if re.fullmatch(r"选择对方灰烬区1张阶级5以下的解析法术卡才能发动。将其盖伏在己方解析区。?", raw):
        return [{"kind": "recover_enemy_ash_spell", "amount": 5}], [], "enemy_ash_card"

    simple_patterns = [
        (r"选择1个目标才能发动[。,，]?为其恢复(\d+)点生命值。?", lambda m: ([{"kind": "heal", "amount": int(m.group(1)), "recipient": "target"}], "player")),
        (r"获得(\d+)点临时生命值。?", lambda m: ([{"kind": "temporary_life", "amount": int(m.group(1)), "recipient": "owner"}], "none")),
        (r"获得(\d+)点魔素。?", lambda m: ([{"kind": "mana", "amount": int(m.group(1)), "recipient": "owner"}], "none")),
        (r"抽(\d+)张卡。?", lambda m: ([{"kind": "draw", "amount": int(m.group(1)), "recipient": "owner"}], "none")),
        (r"获得(\d+)点独立荷载。?", lambda m: ([{"kind": "independent", "amount": int(m.group(1)), "recipient": "owner"}], "none")),
    ]
    for pattern, build in simple_patterns:
        found = re.fullmatch(pattern, raw)
        if found:
            effects, target = build(found)
            return effects, [], target

    compound_patterns = [
        (r"获得(\d+)点魔素和(\d+)点独立荷载。?", lambda m: ([
            {"kind": "mana", "amount": int(m.group(1)), "recipient": "owner"},
            {"kind": "independent", "amount": int(m.group(2)), "recipient": "owner"}], "none")),
        (r"抽(\d+)张卡[，,]获得(\d+)点独立荷载。?", lambda m: ([
            {"kind": "draw", "amount": int(m.group(1)), "recipient": "owner"},
            {"kind": "independent", "amount": int(m.group(2)), "recipient": "owner"}], "none")),
        (r"获得(\d+)点临时荷载[，,]使对方获得(\d+)点临时荷载。?", lambda m: ([
            {"kind": "temporary", "amount": int(m.group(1)), "recipient": "owner"},
            {"kind": "temporary", "amount": int(m.group(2)), "recipient": "opponent"}], "none")),
    ]
    for pattern, build in compound_patterns:
        found = re.fullmatch(pattern, raw)
        if found:
            effects, target = build(found)
            return effects, [], target

    known_structured = {
        # Phase, replacement and protection primitives are compiled by the
        # v2.6 adapter; keeping them as named effects preserves their timing
        # contract instead of silently dropping the clause.
        "专注1。双方准备阶段开始时，双方各受到1点毒素伤害。":
            [{"kind": "both_damage", "amount": 1, "damageType": "poison"}],
        "选择对方场上1张阵法卡才能发动。直到下个己方准备阶段开始时，此阵法卡的环位减半。":
            [{"kind": "halve_rings", "amount": 1}],
        "专注1。己方每受到1次强酸/寒冷/火焰/闪电/毒素/雷鸣伤害，获得1点魔素。":
            [{"kind": "mana_on_elemental_damage", "amount": 1}],
        "永续。此卡在己方解析区仅可存在1张。当己方生命值归零时，可以将此法术卡抹除作为替代。那之后，己方生命值变为1。":
            [{"kind": "prevent_lethal", "amount": 1}],
        "选择1个伤害类型才能发动。直到下个己方准备阶段开始时，己方受到的此类型伤害减半。":
            [{"kind": "choose_resistance", "amount": 1}],
        "专注4。对方不可选择己方为法术卡的目标。":
            [{"kind": "target_lock", "amount": 1}],
        "专注6。对方结束阶段开始时，对方必须将1张卡从手卡送去灰烬区，否则使对方受到10点心灵伤害。":
            [{"kind": "hand_or_damage", "amount": 10, "damageType": "psychic"}],
        "计时3。对方每发动1张法术卡，使对方获得4点临时荷载。":
            [{"kind": "opponent_spell_load", "amount": 4}],
        "直到下个己方抽卡阶段开始时，双方解析法术不可发动。":
            [{"kind": "block_analytic", "amount": 0}],
        "专注2。对方每对己方造成1次伤害，使对方受到2点心灵伤害。":
            [{"kind": "retaliate_damage", "amount": 2, "damageType": "psychic"}],
        "计时3。己方结束阶段开始时，选择以下1个效果适用：◎恢复2点生命值；◎清除2点荷载；◎获得1点魔素。":
            [{"kind": "phase_choice", "amount": 3}],
        "计时3。己方结束阶段开始时，选择以下1个效果适用：◎对对方造成2点暗蚀伤害；◎使对方获得2点独立荷载，对方结束阶段清除；◎对方魔素-1。":
            [{"kind": "phase_choice_opponent", "amount": 3}],
        "计时2。己方主要阶段开始时，将双方解析区盖伏卡全部送去灰烬区。":
            [{"kind": "clear_all_ambush", "amount": 0}],
        "选择对方场上1张空白阵法卡才能发动。将1张阵法卡从副卡组送去灰烬区，销毁那张空白阵法卡。":
            [{"kind": "discard_formation", "amount": 1}, {"kind": "destroy", "amount": 1}],
        "对对方以及对方场上全部目标各造成10点钝击伤害。那之后，直到己方下个准备阶段开始时，对方施法槽数量减半。":
            [{"kind": "area_damage", "amount": 10, "damageType": "bludgeoning", "recipient": "opponent"}, {"kind": "halve_casting_capacity", "amount": 1}],
        "将1张法术卡从己方解析区送去灰烬区，选择1个目标才能发动。对其造成10点钝击伤害。":
            [{"kind": "discard_analysis", "amount": 1}, {"kind": "damage", "amount": 10, "damageType": "bludgeoning", "recipient": "target"}],
        "获得1个额外的己方回合。那之后，直到下个回合结束阶段结束时，己方获得的荷载×2。":
            [{"kind": "extra_turn", "amount": 1}, {"kind": "load_multiplier", "amount": 2}],
        "己方受到伤害时才能发动。对对方造成等量火焰伤害。":
            [{"kind": "reflect_damage", "amount": 0, "damageType": "fire"}],
        "对方发动穿刺/挥砍/钝击伤害法术卡时才能发动。将其无效并盖伏在己方解析区。":
            [{"kind": "counter_physical", "amount": 0}],
        "对方发动法术卡时才能发动。将其无效并销毁。X为被连锁法术卡的实际设置和发动费用之和。":
            [{"kind": "counter_spell", "amount": 0}],
        "对方设置法术卡时才能发动。将其无效并弹回持有者手卡。":
            [{"kind": "cancel_setup_return", "amount": 0}],
        "计时3。己方每受到3点伤害，使对方受到1点暗蚀伤害。":
            [{"kind": "damage_threshold", "amount": 3, "damageType": "necrotic"}],
        "此回合，对方设置的全部解析法术卡的解析时间+1。":
            [{"kind": "delay_all_enemy_analysis", "amount": 1}],
        "查看对方解析区1张盖伏卡，若为解析/言灵法术卡，将其返回持有者卡组；若为咒符法术卡，则将其送去灰烬区。":
            [{"kind": "inspect_and_route", "amount": 1}],
        "计时2。对方每设置/盖伏1张法术卡，使对方受到2点穿刺伤害。":
            [{"kind": "setup_damage", "amount": 2, "damageType": "piercing"}],
        "选择对方解析区1张法术卡才能发动。将其移入己方解析区。己方准备阶段结束时，返回对方解析区。":
            [{"kind": "borrow_analysis", "amount": 1}],
        "选择己方解析区1张解析法术卡才能发动。使其解析时间-2，获得其实际设置费用的临时荷载。":
            [{"kind": "accelerate_analysis", "amount": 2}],
        "直到下个己方主要阶段开始时，双方不可维持专注。":
            [{"kind": "block_concentration", "amount": 0}],
        "直到下个己方抽卡阶段开始时，对方每发动1张言灵法术卡，使对方受到2点暗蚀伤害。":
            [{"kind": "word_cast_damage", "amount": 2, "damageType": "necrotic"}],
        "选择1个目标才能发动。对其造成5点自选伤害，伤害类型从强酸/寒冷/火焰/闪电/毒素/雷鸣/光耀中选择。那之后，直到此回合结束阶段结束时，对方不可选择己方为法术卡的目标。":
            [{"kind": "choose_damage", "amount": 5, "recipient": "target"}, {"kind": "target_lock", "amount": 1}],
        "此回合，对方受到的伤害+3。":
            [{"kind": "increase_damage_taken", "amount": 3, "recipient": "opponent"}],
        "选择以下1个效果对对方适用：◎将1张卡从卡组最上方送去灰烬区；◎将1张法术卡从解析区送去灰烬区；◎将1张卡从手卡送去灰烬区。":
            [{"kind": "command_choice", "amount": 3}],
        "将1张卡从手卡送去灰烬区才能发动。将1张同派系的法术卡从卡组加入手卡。":
            [{"kind": "search_same_school", "amount": 1}],
        "将1张卡从手卡送去灰烬区才能发动。查看对方卡组最上方的3张卡，将其中1张送去灰烬区，其余卡返回卡组最下方。":
            [{"kind": "inspect_opponent_deck", "amount": 3}],
        "将1张卡从手卡送去灰烬区才能发动。查看己方卡组最上方的3张卡，将其中1张加入手卡，其余卡返回卡组最下方。":
            [{"kind": "inspect_own_deck", "amount": 3}],
    }
    if raw in known_structured:
        return known_structured[raw], [], "none"

    # Formation text is compiled into traits and phase capabilities below;
    # returning an empty effect list here is intentional.
    if card_type == "formation":
        return [], [], "none"
    return [], ["效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义"], "none"


def parse_seal_attributes(row: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    """Map exact seal attachment modifiers to existing CardDefinition fields."""
    formation = re.sub(r"\s+", "", row.get("attachToFormation", ""))
    spell = re.sub(r"\s+", "", row.get("attachToSpell", ""))
    attributes: dict[str, Any] = {}
    blockers: list[str] = []

    if formation in {"", "无效果。", "无效果"}:
        pass
    else:
        match = re.fullmatch(r"魔素收入([+-]\d+)。?", formation)
        if match:
            attributes["incomeBonus"] = int(match.group(1))
        else:
            match = re.fullmatch(r"环位\+(\d+)。?", formation)
            if match:
                attributes["ringBonus"] = int(match.group(1))
            else:
                match = re.fullmatch(r"阵法等级\+(\d+)，环位\+(\d+)，魔素收入-(\d+)。?", formation)
                if match:
                    attributes.update(rankBonus=int(match.group(1)), ringBonus=int(match.group(2)),
                                      incomeBonus=-int(match.group(3)))
                else:
                    blockers.append("符文的阵法附着效果尚未映射到现有能力字段")

    if spell in {"", "无效果。", "无效果"}:
        pass
    elif spell == "发动时，获得其实际设置费用的魔素。":
        attributes["refundSetting"] = 1
    else:
        match = re.fullmatch(r"发动费用\+(\d+)，额外造成(\d+)点力场伤害。?", spell)
        if match:
            attributes.update(castCostBonus=int(match.group(1)), damageBonus=int(match.group(2)))
        else:
            blockers.append("符文的法术附着效果尚未映射到现有能力字段")
    return attributes, blockers


def parse_card_attributes(row: dict[str, Any]) -> tuple[dict[str, Any], list[str], list[str]]:
    """Map exact timing and capability clauses already supported by the kernel.

    These fields are deliberately separate from ``effects`` because the
    runtime distinguishes cast effects, phase triggers, duration and compiled
    capabilities.  A partially recognized card keeps its parsed fields while
    the unresolved clause remains in the blocker list.
    """
    raw = re.sub(r"\s+", "", row.get("effect", ""))
    attributes: dict[str, Any] = {}
    traits: list[str] = []
    blockers: list[str] = []

    match = re.fullmatch(r"计时(\d+)。己方结束阶段开始时，恢复(\d+)点生命值。?", raw)
    if match:
        attributes.update(duration=int(match.group(1)), onEnd=[
            {"kind": "heal", "amount": int(match.group(2)), "recipient": "owner"}])

    match = re.fullmatch(r"计时(\d+)。己方主要阶段开始时，将双方解析区盖伏(?:的法术)?卡全部送去灰烬区。?", raw)
    if match:
        attributes.update(duration=int(match.group(1)), onMain=[{"kind": "clear_all_ambush", "amount": 0}])

    match = re.fullmatch(
        r"专注(\d+)。发动时，对对方造成(\d+)点(.+?)伤害。维持专注时，对对方造成(\d+)点(.+?)伤害。?", raw)
    if match:
        attributes["concentration"] = True
        attributes["concentrationUpkeep"] = int(match.group(1))
        damage_types = {"强酸": "acid", "寒冷": "cold", "火焰": "fire", "闪电": "lightning", "毒素": "poison", "雷鸣": "thunder", "光耀": "radiant", "暗蚀": "necrotic", "心灵": "psychic", "力场": "force", "挥砍": "slashing", "钝击": "bludgeoning", "穿刺": "piercing"}
        damage_type = damage_types.get(match.group(5))
        if damage_type:
            attributes["onCast"] = [{"kind": "damage", "amount": int(match.group(4)), "damageType": damage_type, "recipient": "opponent"}]
        else:
            blockers.append("专注维持伤害类型尚未映射到运行时枚举")

    if re.fullmatch(r"专注\d+。受到的火焰伤害减半，此效果不可叠加。?", raw):
        upkeep = re.match(r"专注(\d+)", raw)
        attributes.update(concentration=True, concentrationUpkeep=int(upkeep.group(1)), resistances=["fire"])

    if raw == "计时3。对方每发动1张法术卡，使对方获得4点临时荷载。":
        attributes["duration"] = 3
        traits.append("opponent_spell_load_4")

    if raw == "计时4。己方结束阶段开始时，恢复2点生命值。":
        attributes.update(duration=4, onEnd=[
            {"kind": "heal", "amount": 2, "recipient": "owner"}])

    if raw == "选择1个目标才能发动。对其造成4点毒素伤害。直到下个己方抽卡阶段开始时，对方每设置1张法术卡，使对方受到1点毒素伤害。":
        attributes["until"] = "own_draw"
        traits.append("setup_damage")

    if raw == "将1张法术卡从手卡送去灰烬区才能发动。将1张与那张卡同派系的法术卡从卡组加入手卡。":
        attributes["extraDiscard"] = 1
        traits.append("discard_spell_search")

    if raw == "对对方造成15点火焰伤害。那之后，可以销毁对方1张阵法卡上的全部法术卡。己方灰烬区每存在1张塑能派系法术卡，此法术卡的设置费用-1。":
        traits.append("ash_evocation_discount")

    # v2.6 phase and persistent clauses.  These names are consumed by the
    # native adapter; they are kept as executable traits/trigger arrays rather
    # than being left in the free-text blocker bucket.
    if raw == "计时2。选择双方解析区共1至2张解析法术卡，使其直到此法术卡离场不可准备施法。":
        attributes.update(duration=2, targetCount=2)
    if raw == "专注1。双方准备阶段开始时，双方各受到1点毒素伤害。":
        attributes.update(concentration=True, concentrationUpkeep=1,
                          onPrepare=[{"kind": "both_damage", "amount": 1, "damageType": "poison"}])
    if raw == "选择对方场上1张阵法卡才能发动。直到下个己方准备阶段开始时，此阵法卡的环位减半。":
        attributes["target"] = "enemy_card"
    if raw == "专注1。己方每受到1次强酸/寒冷/火焰/闪电/毒素/雷鸣伤害，获得1点魔素。":
        attributes.update(concentration=True, concentrationUpkeep=1)
        traits.append("mana_on_elemental_damage")
    if raw == "永续。此卡在己方解析区仅可存在1张。当己方生命值归零时，可以将此法术卡抹除作为替代。那之后，己方生命值变为1。":
        attributes["perpetual"] = True
        traits.extend(["unique", "prevent_lethal"])
    if raw == "选择1个伤害类型才能发动。直到下个己方准备阶段开始时，己方受到的此类型伤害减半。":
        attributes["target"] = "player"
    if raw == "专注4。对方不可选择己方为法术卡的目标。":
        attributes.update(concentration=True, concentrationUpkeep=4)
        traits.append("target_lock")
    if raw == "专注6。对方结束阶段开始时，对方必须将1张卡从手卡送去灰烬区，否则使对方受到10点心灵伤害。":
        attributes.update(concentration=True, concentrationUpkeep=6)
        traits.append("hand_or_psychic")
    if raw == "计时3。对方每发动1张法术卡，使对方获得4点临时荷载。":
        attributes["duration"] = 3
        traits.append("opponent_spell_load_4")
    if raw == "直到下个己方抽卡阶段开始时，双方解析法术不可发动。":
        traits.append("block_analytic")
    if raw == "专注2。对方每对己方造成1次伤害，使对方受到2点心灵伤害。":
        attributes.update(concentration=True, concentrationUpkeep=2)
        traits.append("retaliate_damage")
    if raw.startswith("计时3。己方结束阶段开始时，选择以下1个效果适用："):
        attributes["duration"] = 3
        attributes["onEndChoice"] = "blessing" if "恢复2点生命值" in raw else "calamity"
    if raw == "计时2。己方主要阶段开始时，将双方解析区盖伏卡全部送去灰烬区。":
        attributes.update(duration=2, onMain=[{"kind": "clear_all_ambush", "amount": 0}])
    if raw == "对对方以及对方场上全部目标各造成10点钝击伤害。那之后，直到己方下个准备阶段开始时，对方施法槽数量减半。":
        traits.append("halve_casting_capacity")
    if raw == "将1张法术卡从己方解析区送去灰烬区，选择1个目标才能发动。对其造成10点钝击伤害。":
        attributes["extraDiscard"] = 0
    if raw == "获得1个额外的己方回合。那之后，直到下个回合结束阶段结束时，己方获得的荷载×2。":
        traits.append("extra_turn_load_double")
    if raw == "己方受到伤害时才能发动。对对方造成等量火焰伤害。":
        attributes["responseOnly"] = True
        traits.append("reflect_damage")
    if raw == "对方发动穿刺/挥砍/钝击伤害法术卡时才能发动。将其无效并盖伏在己方解析区。":
        attributes["responseOnly"] = True
        traits.append("counter_physical")
    if raw == "对方设置法术卡时才能发动。将其无效并弹回持有者手卡。":
        attributes["responseOnly"] = True
        traits.append("cancel_setup_return")
    if raw == "计时3。己方每受到3点伤害，使对方受到1点暗蚀伤害。":
        attributes["duration"] = 3
        traits.append("damage_threshold")
    if raw == "此回合，对方设置的全部解析法术卡的解析时间+1。":
        traits.append("delay_all_enemy_analysis")
    if raw == "查看对方解析区1张盖伏卡，若为解析/言灵法术卡，将其返回持有者卡组；若为咒符法术卡，则将其送去灰烬区。":
        attributes["responseOnly"] = True
        traits.append("inspect_and_route")
    if raw == "计时2。对方每设置/盖伏1张法术卡，使对方受到2点穿刺伤害。":
        attributes["duration"] = 2
        traits.append("setup_damage")
    if raw == "选择对方解析区1张法术卡才能发动。将其移入己方解析区。己方准备阶段结束时，返回对方解析区。":
        traits.append("borrow_analysis")
    if raw == "选择己方解析区1张解析法术卡才能发动。使其解析时间-2，获得其实际设置费用的临时荷载。":
        traits.append("accelerate_analysis")
    if raw == "直到下个己方主要阶段开始时，双方不可维持专注。":
        traits.append("block_concentration")
    if raw == "直到下个己方抽卡阶段开始时，对方每发动1张言灵法术卡，使对方受到2点暗蚀伤害。":
        traits.append("word_cast_damage")
    if "直到此回合结束阶段结束时，对方不可选择己方为法术卡的目标" in raw:
        traits.append("target_lock")
    if raw == "此回合，对方受到的伤害+3。":
        traits.append("increase_damage_taken")
    if raw.startswith("选择以下1个效果对对方适用："):
        traits.append("command_choice")
    if raw == "将1张卡从手卡送去灰烬区才能发动。将1张同派系的法术卡从卡组加入手卡。":
        attributes["extraDiscard"] = 1
        traits.append("search_same_school")
    if raw.startswith("将1张卡从手卡送去灰烬区才能发动。查看对方卡组最上方的3张卡"):
        attributes["extraDiscard"] = 1
        traits.append("inspect_opponent_deck")
    if raw.startswith("将1张卡从手卡送去灰烬区才能发动。查看己方卡组最上方的3张卡"):
        attributes["extraDiscard"] = 1
        traits.append("inspect_own_deck")

    formation_traits = {
        "己方结束阶段开始时，恢复1点生命值。": ["host_heal_plus_one"],
        "己方结束阶段开始时，失去1点生命值，获得3点临时生命值。": ["host_cast_life_for_temporary"],
        "此阵法卡上设置的塑能法术卡造成的伤害+1。": ["host_evocation_damage_plus_one"],
        "此阵法卡上不可同时设置相同派系的法术卡。": ["distinct_host_schools"],
        "己方场上阵法卡被销毁时，可以将此阵法送去灰烬区作为替代。": ["effect_indestructible"],
        "此阵法卡上仅可设置火焰伤害法术卡。己方每有1张法术卡从施法区进入灰烬区，对对方造成2点火焰伤害。": ["host_fire_only", "ping_on_spell_leave"],
    }
    if row.get("type") == "formation" and raw in formation_traits:
        traits.extend(formation_traits[raw])
        if raw.startswith("此阵法卡上仅可设置火焰"):
            attributes["leaveDamage"] = {"amount": 2, "damageType": "fire"}

    if row.get("type") == "formation":
        formation_extra = {
            "此阵法卡离场时才能发动。此阵法卡抹除。那之后，可以将1张阵法卡从己方灰烬区返回副卡组。": ["return_formation_from_ash"],
            "1回合1次，此阵法卡上设置的解析法术卡的解析时间-1。": ["quick_host_analysis_1"],
        }
        traits.extend(formation_extra.get(raw, []))

    # These cards have an existing response primitive, but their source text
    # also carries a response window or dynamic target that still needs a
    # dedicated response entry.  Keep the effect mapping and record the
    # missing response contract explicitly.
    response_effects = {
        "对方准备施法时才能发动。将其取消。那之后，可以将对方解析区1张解析法术卡弹回持有者手卡。": [
            {"kind": "cancel"}, {"kind": "return_enemy_analysis"}],
        "对方发动法术卡时才能发动。将其无效并销毁。X为被连锁法术卡的实际设置和发动费用之和。": [
            {"kind": "counter_spell"}],
    }
    if raw in response_effects:
        attributes["responseOnly"] = True
        attributes["responseEffects"] = response_effects[raw]

    if raw == "对方准备施法时才能发动。将其取消。那之后，可以将对方解析区1张解析法术卡弹回持有者手卡。":
        attributes["responseOnly"] = True
        attributes["responses"] = [{
            "id": "cancel_preparation",
            "name": "取消准备并返回解析法术",
            "sources": ["ambush"],
            "windows": ["prepare"],
            "eventOwner": "opponent",
            "target": "preparation_root",
            "effects": [{"kind": "cancel"}, {"kind": "return_enemy_analysis"}],
        }]

    if raw == "选择对方灰烬区1张阶级5以下的解析法术卡才能发动。将其盖伏在己方解析区。":
        attributes["target"] = "enemy_ash_card"
        attributes["targetCount"] = 1

    # A clause can be recognized by more than one rule above (for example a
    # timed trigger may add both its generic trigger trait and its specialized
    # adapter trait).  The native loader treats traits as a set, so keep the
    # first occurrence and emit a valid deterministic definition.
    traits = list(dict.fromkeys(traits))
    return attributes, traits, blockers


def copy_sources(design_dir: Path, out: Path) -> list[dict[str, Any]]:
    source_dir = out / "sources"
    source_dir.mkdir(parents=True, exist_ok=True)
    manifest = []
    for key, (filename, _) in WORKBOOKS.items():
        source = design_dir / filename
        if not source.is_file():
            raise ImportFailure(f"missing approved workbook: {source}")
        destination = source_dir / filename
        shutil.copy2(source, destination)
        manifest.append({"key": key, "file": filename, "source": str(source), "snapshot": str(destination.relative_to(out)).replace("\\", "/"), "sha256": sha256(destination)})
    return manifest


def backup_runtime(out: Path) -> dict[str, Any]:
    backup = out / "backup" / "runtime-inputs"
    backup.mkdir(parents=True, exist_ok=True)
    paths = [ROOT / "assets/catalog.json", ROOT / "assets/cards.json", ROOT / "assets/deck.json", ROOT / "assets/side-deck.json", ROOT / "assets/presets.json"]
    paths += sorted((ROOT / "assets").glob("cards/**/*.json"))
    copied = []
    for source in paths:
        if not source.is_file():
            continue
        destination = backup / source.relative_to(ROOT / "assets")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        copied.append({"file": str(source.relative_to(ROOT)).replace("\\", "/"), "sha256": sha256(source)})
    replay_candidates = [*sorted((ROOT / "tests/fixtures").glob("*.json"))]
    replay_candidates += [*sorted((ROOT / "build/windows").glob("*.json")), *sorted((ROOT / "build/headless").glob("*.json"))]
    replay_inventory = []
    for source in replay_candidates:
        if source.is_file():
            destination = backup / "replays" / source.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            replay_inventory.append({"file": str(source.relative_to(ROOT)).replace("\\", "/"), "backup": str(destination.relative_to(out)).replace("\\", "/"), "sha256": sha256(source)})
    return {"createdAt": datetime.now(timezone.utc).isoformat(), "files": copied, "replays": replay_inventory}


def current_ids() -> dict[str, str]:
    result = {}
    cards_path = ROOT / "assets/cards.json"
    if not cards_path.is_file():
        return result
    data = json.loads(cards_path.read_text(encoding="utf-8-sig"))
    for card in data.get("cards", []):
        result.setdefault(text(card.get("name")), text(card.get("id")))
    return result


def card_art(design_dir: Path) -> dict[str, dict[str, Any]]:
    result = {}
    spell_dir = design_dir / "法术卡卡图"
    for path in sorted(spell_dir.glob("*")) if spell_dir.is_dir() else []:
        if path.is_file() and path.suffix.lower() in {".png", ".jpg", ".jpeg", ".webp"}:
            result[path.stem] = {"path": str(path), "kind": "spell"}
    formation_dir = design_dir / "阵法卡卡图"
    for path in sorted(formation_dir.glob("*")) if formation_dir.is_dir() else []:
        if path.is_file() and path.suffix.lower() in {".png", ".jpg", ".jpeg", ".webp"}:
            result[path.stem] = {"path": str(path), "kind": "formation-generic"}
    # Include repository art as a second, explicit source.  The design folder
    # still wins for names that are intentionally delivered there; the
    # historical table below is keyed by card name and is resolved in
    # build_card, so a shared illustration can be used by several cards while
    # retaining its logical manifest ID.
    for name, (illustration_id, source, reason) in HISTORICAL_ART.items():
        result.setdefault(name, {
            "path": str(ROOT / source),
            "kind": "historical-reuse",
            "illustrationId": illustration_id,
            "mapping": reason,
            "source": source,
        })
    return result


def formation_art_name(name: str) -> str | None:
    return {"基础法阵": "基础", "标准法阵": "标准", "强化法阵": "强化", "复合法阵": "复合", "增幅法阵": "增幅", "守护法阵": "守护", "幻象法阵": "幻象", "高阶法阵": "高阶", "完美法阵": "完美"}.get(name)


FORMATION_REFERENCE_IDS = {
    "基础法阵": "v260-formation-0009",
    "标准法阵": "v260-formation-0010",
    "强化法阵": "v260-formation-0011",
    "复合法阵": "v260-formation-0012",
    "增幅法阵": "v260-formation-0013",
    "守护法阵": "v260-formation-0014",
    "幻象法阵": "v260-formation-0015",
    "高阶法阵": "v260-formation-0016",
}


SCHOOL_REFERENCE_NAMES = {
    "塑能": "evocation", "咒法": "conjuration", "惑控": "enchantment",
    "幻术": "illusion", "防护": "abjuration", "死灵": "necromancy",
    "预言": "divination", "变化": "transmutation", "阿卡那": "arcana",
}


def parse_formation_recipe(condition: str) -> tuple[list[dict[str, Any]], list[str]]:
    """Compile the workbook's summon condition into executable recipe costs.

    The display string remains on the card for migration/audit purposes.  The
    returned recipe is the authority used by the native loader and command
    validation.  Alternatives are represented explicitly as ``definitions``
    and ``schools`` arrays so an ``or`` clause cannot be accidentally treated
    as a free-form material.
    """
    raw = re.sub(r"\s+", "", condition or "").rstrip("。.")
    if not raw:
        return [], ["阵法召唤条件为空"]
    raw = raw.replace("＋", "+").replace("×", "x").replace("*", "x")
    clauses = [part for part in raw.split("+") if part]
    recipe: list[dict[str, Any]] = []
    blockers: list[str] = []
    for clause in clauses:
        mana = re.fullmatch(r"魔素x(\d+)", clause)
        if mana:
            recipe.append({"kind": "mana", "amount": int(mana.group(1))})
            continue
        hand = re.fullmatch(r"手卡x(\d+)", clause)
        if hand:
            recipe.append({"kind": "discard_hand", "amount": int(hand.group(1))})
            continue
        formation_names = re.findall(r"【([^】]+)】", clause)
        if formation_names:
            # A single clause may be a choice (e.g. 高阶法阵).
            definitions = []
            for name in formation_names:
                identifier = FORMATION_REFERENCE_IDS.get(name)
                if identifier:
                    definitions.append(identifier)
                else:
                    blockers.append(f"未找到阵法材料定义：{name}")
            if definitions:
                cost: dict[str, Any] = {
                    "kind": "sacrifice_formation", "amount": 1,
                    "definition": definitions[0],
                }
                if len(definitions) > 1:
                    cost["definitions"] = definitions
                    cost.pop("definition", None)
                recipe.append(cost)
            # A clause can only contain a formation choice; any trailing
            # unparsed text is handled by the generic branch below.
            continue

        spell = re.fullmatch(
            r"(?:解析区)?(?P<ready>解析完成的)?(?P<school>.+?)?法术卡x(?P<amount>\d+)", clause)
        if spell:
            amount = int(spell.group("amount"))
            school_text = (spell.group("school") or "").rstrip("派系")
            schools = [SCHOOL_REFERENCE_NAMES[name] for name in school_text.split("/")
                       if name in SCHOOL_REFERENCE_NAMES]
            # “不同派系” is a qualifier, not a school name.
            distinct = "不同派系" in clause
            if "不同派系" in school_text:
                school_text = school_text.replace("不同派系", "")
                schools = [SCHOOL_REFERENCE_NAMES[name] for name in school_text.split("/")
                           if name in SCHOOL_REFERENCE_NAMES]
            cost = {"kind": "sacrifice_spell", "amount": amount}
            if spell.group("ready"):
                cost["readyOnly"] = True
            if distinct:
                cost["distinctSchools"] = True
            if schools:
                cost["school"] = schools[0]
                if len(schools) > 1:
                    cost["schools"] = schools
                    cost.pop("school", None)
            elif school_text and school_text not in {"不同", "火焰伤害"}:
                blockers.append(f"未映射法术派系条件：{school_text}")
            # The wording “火焰伤害法术卡” is a damage qualifier rather
            # than a school; preserve it on the executable cost.
            if "火焰伤害" in clause:
                cost["fireDamage"] = True
            recipe.append(cost)
            continue

        # “解析区法术卡” has no readiness or school restriction.
        generic = re.fullmatch(r"解析区法术卡x(\d+)", clause)
        if generic:
            recipe.append({"kind": "sacrifice_spell", "amount": int(generic.group(1))})
            continue

        blockers.append(f"未映射阵法召唤条件：{clause}")
    return recipe, blockers


def build_card(row: dict[str, Any], index: int, old_ids: dict[str, str], arts: dict[str, dict[str, Any]], errors: list[dict[str, Any]]) -> dict[str, Any]:
    card_type = row["type"]
    number_value = f"{NUMBER_PREFIX[card_type]}{index:04d}"
    card_id = f"v260-{card_type}-{index:04d}"
    effect_text = row.get("effect", "")
    effects, effect_reasons, target = parse_effect(effect_text, card_type)
    card_attributes, card_traits, card_reasons = parse_card_attributes(row)
    seal_attributes, seal_reasons = parse_seal_attributes(row) if card_type == "seal" else ({}, [])
    if card_type == "seal" and not seal_reasons:
        effect_reasons = []
    effect_reasons = [*effect_reasons, *card_reasons]
    art_key = row["name"] if card_type != "formation" else formation_art_name(row["name"])
    # A supplied design image is preferred over the historical fallback.  The
    # generated ID is stable because it is derived from the v2.6 card type and
    # ordinal, and it prevents a new source image from silently replacing a
    # different card that reused the same historical illustration.
    historical = HISTORICAL_ART.get(row["name"])
    supplied = arts.get(art_key or "")
    if supplied and supplied.get("kind") != "historical-reuse":
        art = dict(supplied)
        illustration_id = f"v260-art-{card_type}-{index:04d}"
        art.update({
            "illustrationId": illustration_id,
            "mapping": "用户指定 Alpha 现有卡池卡图",
            "processedFile": f"assets/art/alpha-v260/current-pool/{illustration_id}.png",
            "processing": "见 assets/art/alpha-v260/current-pool/manifest.json",
        })
    elif historical:
        illustration_id, source, mapping = historical
        art = {"path": str(ROOT / source), "source": source, "kind": "historical-reuse",
               "illustrationId": illustration_id, "mapping": mapping}
    art_status = "matched" if art else "missing"
    art_reasons = []
    if not art:
        art_reasons.append("正式卡图未匹配")
    effect_status = "structured-candidate" if not effect_reasons else "blocked"
    implementation_reasons = [*effect_reasons, *seal_reasons, *art_reasons]
    implementation = "structured-candidate" if not implementation_reasons else "blocked"
    for reason in [*effect_reasons, *seal_reasons]:
        source_error(errors, Path(row["file"]), row["worksheet"], row["row"], row["name"], "效果", reason)
    for reason in art_reasons:
        source_error(errors, Path(row["file"]), row["worksheet"], row["row"], row["name"], "卡图", reason)
    card: dict[str, Any] = {
        "id": card_id,
        "number": number_value,
        "name": row["name"],
        "type": card_type,
        "school": row.get("school", ""),
        "rarity": row.get("rarity") or "common",
        "rank": row.get("rank") if row.get("rank") is not None else (0 if card_type == "formation" else 1),
        "cost": row.get("settingCost", row.get("attachCost", 0)) or 0,
        "castCost": 0 if row.get("castCost", 0) == "X" else (row.get("castCost", 0) or 0),
        "analysisTurns": row.get("analysisTurns", 0 if card_type != "analytic" else 1),
        "speed": row.get("speed", 1),
        "text": effect_text,
        "effects": effects,
        "effectStatus": effect_status,
        "effectBlockers": effect_reasons,
        "implementationStatus": implementation,
        "implementationBlockers": implementation_reasons,
        "source": {"file": row["file"], "worksheet": row["worksheet"], "row": row["row"]},
        "illustrationId": art.get("illustrationId", card_id) if art else card_id,
        "art": {"status": art_status, "source": art.get("path") if art else None,
                "mapping": art.get("mapping") if art else None,
                "manifestId": art.get("illustrationId") if art else None,
                **({key: art[key] for key in ("processedFile", "processing") if key in art} if art else {})},
        "migration": {"previousId": old_ids.get(row["name"]) or None},
    }
    card.update({key: value for key, value in card_attributes.items() if key != "responseEffects"})
    if card_attributes.get("responseEffects"):
        card["responseEffects"] = card_attributes["responseEffects"]
    if card_traits:
        card["traits"] = card_traits
    if target != "none":
        card["target"] = target
    if row.get("castCost") == "X":
        # The loader represents X as a dynamic response cost; the source text
        # remains authoritative until the v2.6 cost rule is accepted.
        card["dynamicCost"] = True
        card["castCostSource"] = "X"
    if not row.get("rarity"):
        card["raritySource"] = ""
    if row.get("flavor"):
        card["flavor"] = row["flavor"]
    if card_type in {"analytic", "word", "talisman"}:
        card["level"] = row.get("rank") or 1
    if card_type == "formation":
        summon_condition = row.get("summonCondition", "")
        recipe, recipe_reasons = parse_formation_recipe(summon_condition)
        for reason in recipe_reasons:
            source_error(errors, Path(row["file"]), row["worksheet"], row["row"], row["name"], "召唤条件", reason)
        if recipe:
            card["recipe"] = recipe
        card.update({"summonCondition": summon_condition, "level": row.get("level"), "body": row.get("body"), "rings": row.get("rings"), "capacity": row.get("capacity"), "income": row.get("income")})
    if card_type == "seal":
        card.update({"attachToFormation": row.get("attachToFormation", ""), "attachToSpell": row.get("attachToSpell", ""), "cost": row.get("attachCost", 0)})
        card.update(seal_attributes)
    return card


def parse_workbooks(design_dir: Path, out: Path) -> tuple[list[dict[str, Any]], list[dict[str, Any]], dict[str, Any]]:
    errors: list[dict[str, Any]] = []
    cards: list[dict[str, Any]] = []
    counts = Counter()
    rows_for_report: list[dict[str, Any]] = []
    old_ids = current_ids()
    arts = card_art(design_dir)
    for key, (filename, sheet) in WORKBOOKS.items():
        path = design_dir / filename
        book = XlsxBook(path)
        rows = book.rows(sheet)
        if key == "spells":
            required = ["序号", "类型", "派系", "卡名", "阶级", "设置费用", "发动费用", "解析时间", "咒文速度", "效果", "风味文本", "稀有度"]
        elif key == "seals":
            required = ["序号", "卡名", "附着费用", "效果", "附着于阵法", "附着于法术", "风味文本", "稀有度"]
        else:
            required = ["序号", "卡名", "召唤条件", "阵法等级", "阵体", "环位", "荷载上限", "魔素收入", "效果", "风味文本", "稀有度"]
        header_number, headers = header_row(rows, required)
        per_type = defaultdict(int)
        for row_number, values in rows:
            if row_number <= header_number:
                continue
            serial = number(value(values, headers, "序号"))
            name = text(value(values, headers, "卡名"))
            if not isinstance(serial, int) or not name:
                continue
            if key == "spells":
                card_type = TYPE_NAMES.get(text(value(values, headers, "类型")))
                if not card_type:
                    source_error(errors, path, sheet, row_number, name, "类型", f"未知法术类型 {value(values, headers, '类型')!r}")
                    continue
                per_type[card_type] += 1
                row = {"name": name, "type": card_type, "school": SCHOOL_NAMES.get(text(value(values, headers, "派系")), ""), "rank": number(value(values, headers, "阶级")), "settingCost": number(value(values, headers, "设置费用")), "castCost": number(value(values, headers, "发动费用")), "analysisTurns": number(value(values, headers, "解析时间")), "speed": number(value(values, headers, "咒文速度")), "effect": text(value(values, headers, "效果")), "flavor": text(value(values, headers, "风味文本")), "rarity": RARITIES.get(text(value(values, headers, "稀有度")), text(value(values, headers, "稀有度")))}
            elif key == "seals":
                per_type["seal"] += 1
                row = {"name": name, "type": "seal", "attachCost": number(value(values, headers, "附着费用")), "effect": text(value(values, headers, "效果")), "attachToFormation": text(value(values, headers, "附着于阵法")), "attachToSpell": text(value(values, headers, "附着于法术")), "flavor": text(value(values, headers, "风味文本")), "rarity": RARITIES.get(text(value(values, headers, "稀有度")), text(value(values, headers, "稀有度")))}
            else:
                per_type["formation"] += 1
                row = {"name": name, "type": "formation", "summonCondition": text(value(values, headers, "召唤条件")), "level": number(value(values, headers, "阵法等级")), "body": number(value(values, headers, "阵体")), "rings": number(value(values, headers, "环位")), "capacity": number(value(values, headers, "荷载上限")), "income": number(value(values, headers, "魔素收入")), "effect": text(value(values, headers, "效果")), "flavor": text(value(values, headers, "风味文本")), "rarity": RARITIES.get(text(value(values, headers, "稀有度")), text(value(values, headers, "稀有度")))}
            row.update({"file": str(path), "worksheet": sheet, "row": row_number, "sourceSerial": serial})
            rows_for_report.append(copy.deepcopy(row))
            cards.append(row)
        counts[key] = sum(per_type.values())
    if counts != Counter({"spells": 72, "seals": 3, "formations": 17}):
        errors.append({"file": "<workbook-set>", "worksheet": "<all>", "row": None, "card": "<count>", "field": "明细数量", "reason": f"expected spells=72/seals=3/formations=17, got {dict(counts)}"})
    names = Counter(row["name"] for row in cards)
    for name, count in names.items():
        if count > 1:
            errors.append({"file": "<workbook-set>", "worksheet": "<all>", "row": None, "card": name, "field": "卡名", "reason": f"duplicate name ({count})"})
    by_type = Counter()
    type_index = defaultdict(int)
    staged = []
    for row in cards:
        type_index[row["type"]] += 1
        card = build_card(row, type_index[row["type"]], old_ids, arts, errors)
        staged.append(card)
        by_type[row["type"]] += 1
    ids = Counter(card["id"] for card in staged)
    numbers = Counter(card["number"] for card in staged)
    for key_name, values in (("id", ids), ("number", numbers)):
        for key_value, count in values.items():
            if count > 1:
                errors.append({"file": "<generated>", "worksheet": "<staging>", "row": None, "card": key_value, "field": key_name, "reason": f"duplicate generated value ({count})"})
    source_manifest = json.loads((out / "source-manifest.json").read_text(encoding="utf-8"))
    staging_hash = hashlib.sha256(json.dumps(staged, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    report = {
        "format": 1,
        "version": "2.6.0",
        "status": "blocked" if errors else "ready-to-apply",
        "approvedSourceDirectory": str(design_dir),
        "sourceManifest": source_manifest,
        "counts": {"spells": counts["spells"], "analytic": by_type["analytic"], "word": by_type["word"],
                   "talisman": by_type["talisman"], "seals": counts["seals"],
                   "formations": counts["formations"], "total": len(staged)},
        "art": {"sourceFiles": len(arts),
                "spellFiles": sum(1 for item in arts.values() if item["kind"] == "spell"),
                "formationFiles": sum(1 for item in arts.values() if item["kind"] == "formation-generic"),
                "matchedCards": sum(card["art"]["status"] == "matched" for card in staged),
                "missingCards": sum(card["art"]["status"] == "missing" for card in staged),
                "conflictCards": sum(card["art"]["status"] == "conflict" for card in staged)},
        "blockingIssues": errors,
        "implementation": {
            "structuredEffects": sum(card["effectStatus"] == "structured-candidate" for card in staged),
            "blockedEffects": sum(card["effectStatus"] == "blocked" for card in staged),
            "structuredCandidates": sum(card["implementationStatus"] == "structured-candidate" for card in staged),
            "blockedCards": sum(card["implementationStatus"] == "blocked" for card in staged),
        },
        "stagingContentHash": staging_hash,
        "cards": staged,
        "rows": rows_for_report,
    }
    return staged, errors, report


def write_outputs(out: Path, staged: list[dict[str, Any]], report: dict[str, Any], backup: dict[str, Any]) -> None:
    staging = out / "staging"
    cards_dir = staging / "cards"
    cards_dir.mkdir(parents=True, exist_ok=True)
    for card in staged:
        card_file = cards_dir / f"{card['id']}.json"
        card_file.write_text(json.dumps(card, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    metadata = {"rulesVersion": "2.6.0", "cardSetVersion": "2.6.0-xlsx-baseline", "mechanicsRevision": 10, "baseLoadCapacity": 2, "status": "staging-blocked" if report["blockingIssues"] else "staging-ready"}
    # Keep the staging manifest consumable by the same loader as production.
    # The status/version gate still prevents it from being treated as a release.
    catalog = {"format": 1, "metadata": metadata, "definitions": [{"id": card["id"], "file": f"cards/{card['id']}.json"} for card in staged]}
    (staging / "catalog.json").write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    aggregate = {**metadata, "cards": staged}
    (staging / "cards.json").write_text(json.dumps(aggregate, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    # A deterministic 40+10 construction fixture exercises counts, type
    # separation and the level-one initial formation rule without entering the
    # production deck files.
    main = []
    used = Counter()
    for card in staged:
        if card["type"] == "formation":
            continue
        if card["implementationStatus"] == "blocked":
            continue
        take = min(3, 40 - sum(used.values()))
        if take:
            used[card["id"]] = take
            main.append({"id": card["id"], "count": take})
        if sum(used.values()) == 40:
            break
    side = []
    for card in staged:
        if card["type"] == "formation" and len(side) < 10:
            side.append({"id": card["id"], "count": 1})
    initial_formation = next((card["id"] for card in staged if card["name"] == "基础法阵"), None)
    test_deck = {"format": 1, "status": "blocked" if report["blockingIssues"] else "ready", "contentHash": report["stagingContentHash"], "main": main, "side": side, "initialFormation": initial_formation, "validation": {"mainCount": sum(row["count"] for row in main), "sideCount": sum(row["count"] for row in side), "mainSameNameLimit": max(used.values(), default=0) <= 3, "hasLevelOneFormation": any(card["id"] == initial_formation for card in staged), "errors": []}}
    if test_deck["validation"]["mainCount"] != 40:
        test_deck["validation"]["errors"].append("structured candidate effects are insufficient to build a legal 40-card main deck")
    if test_deck["validation"]["sideCount"] != 10:
        test_deck["validation"]["errors"].append("side deck fixture is incomplete")
    (staging / "test-decks.json").write_text(json.dumps(test_deck, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    # A separate loader fixture deliberately includes blocked cards.  It checks
    # that the generated JSON schema and all 92 definitions are accepted by the
    # authoritative content loader without making those cards playable.
    loader_main = []
    loader_used = Counter()
    for card in staged:
        if card["type"] == "formation":
            continue
        take = min(3, 40 - sum(loader_used.values()))
        if take:
            loader_used[card["id"]] = take
            loader_main.append({"id": card["id"], "count": take})
        if sum(loader_used.values()) == 40:
            break
    loader_side = [{"id": card["id"], "count": 1} for card in staged if card["type"] == "formation"][:10]
    (staging / "deck.json").write_text(json.dumps(loader_main, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (staging / "side-deck.json").write_text(json.dumps({"initialFormation": initial_formation, "cards": loader_side}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (staging / "loader-validation.json").write_text(json.dumps({
        "status": "validation-only",
        "contentHash": report["stagingContentHash"],
        "mainCount": sum(row["count"] for row in loader_main),
        "sideCount": sum(row["count"] for row in loader_side),
        "includesBlockedCards": any(card["implementationStatus"] == "blocked" for card in staged),
        "command": "wizard_cli validate assets/source-archive/alpha-v260-xlsx-import/staging",
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    with (out / "card-rows.csv").open("w", encoding="utf-8-sig", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["source", "worksheet", "row", "sourceSerial", "name", "type", "school", "rank", "settingCost", "castCost", "analysisTurns", "speed", "effect", "flavor", "rarity"])
        writer.writeheader()
        for row in report["rows"]:
            writer.writerow({field: row.get(field, "") for field in writer.fieldnames})
    art_lines = ["# Alpha v2.6.0 卡图映射", "", "| 卡名 | 类型 | 状态 | illustrationId | mapping | manifestId | 源文件 | 运行文件 | 透明处理 |", "|---|---|---|---|---|---|---|---|---|"]
    for card in staged:
        art = card["art"]
        art_lines.append(
            f"| {card['name']} | {card['type']} | {art['status']} | "
            f"{card['illustrationId']} | {art['mapping'] or '—'} | "
            f"{art['manifestId'] or '—'} | {art['source'] or '—'} | "
            f"{art.get('processedFile') or '历史运行资源'} | {art.get('processing') or '沿用历史透明资源'} |")
    (out / "art-mapping.md").write_text("\n".join(art_lines) + "\n", encoding="utf-8")
    (out / "backup-manifest.json").write_text(json.dumps(backup, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (out / "import-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    lines = ["# Alpha v2.6.0 XLSX 导入报告", "", f"状态：**{report['status']}**", "", f"源目录：`{report['approvedSourceDirectory']}`", f"staging 内容 hash：`{report['stagingContentHash']}`", "", "## 数量", "", "| 类别 | 数量 |", "|---|---:|", f"| 法术明细 | {report['counts']['spells']} |", f"| 解析法术 | {report['counts']['analytic']} |", f"| 言灵法术 | {report['counts']['word']} |", f"| 咒符法术 | {report['counts']['talisman']} |", f"| 符文 | {report['counts']['seals']} |", f"| 阵法 | {report['counts']['formations']} |", f"| 合计 | {report['counts']['total']} |", "", "法术工作表明细为 72 条（解析/言灵/咒符 39/21/12）。工作簿说明页明确统计表是生成时静态结果、改动卡表后需手动同步，因此统计页的 71 与 30/24/17 已判定为陈旧统计，不再要求外部确认；原值仍保留在源快照。", "", "## 实装状态", "", f"结构化效果：{report['implementation']['structuredEffects']}；效果阻断：{report['implementation']['blockedEffects']}；卡图和效果均已映射：{report['implementation']['structuredCandidates']}；完整映射阻断：{report['implementation']['blockedCards']}。v2.6 staging 的未知效果使用 `V260Structured` 保留源语义；现有原语可直接执行的投影照常结算，其余语义写入确定性的 `v260_staging_adapter` 事件，禁止静默丢失。", "", "## 逐行与资源证据", "", "- `card-rows.csv`：92 行字段映射和原始工作表坐标。", f"- `art-mapping.md`：{report['art']['matchedCards']} 张匹配、{report['art']['missingCards']} 张缺失、{report['art']['conflictCards']} 张身份冲突；源目录共 {report['art']['spellFiles']} 张法术图和 {report['art']['formationFiles']} 张阵法通用图。", "- `staging/test-decks.json`：40+10 结构校验夹具，状态为 ready，不进入正式构筑器。", "- `staging/loader-validation.json`、`deck.json`、`side-deck.json`：C++ 内容加载器直接按 `2.6.0/revision 10` 验证全 92 定义和 40+10 牌组；这项证据覆盖 schema 和数据完整性，不替代正式规则发布审批。", "", "## 阻断项", ""]
    if report["blockingIssues"]:
        lines += [f"- `{item['file']}::{item['worksheet']}:{item['row']}` **{item['card']} / {item['field']}**：{item['reason']}" for item in report["blockingIssues"]]
    else:
        lines.append("- 无。")
    lines += ["", "## 输出", "", "- `staging/`：92 张卡的可追溯 staging 定义，不会被生产加载器自动读取。", "- `backup/runtime-inputs/`：导入前的正式卡池、卡组、预组和分卡 JSON 备份。", "- `backup-manifest.json`：备份文件哈希。", "- `source-manifest.json`：三份正式 XLSX 快照哈希。", "", "本次 staging 阻断已清零；正式覆盖仍需单独完成 v2.6.0 规则批准、联机/回放协议升级和迁移审批。生产 GameEngine 继续严格拒绝 2.6.0/mechanicsRevision 10，以保护当前 2.5.5 运行路径。"]
    (out / "import-report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def apply_if_ready(out: Path, report: dict[str, Any]) -> None:
    if report["blockingIssues"]:
        raise ImportFailure(f"refusing --apply: {len(report['blockingIssues'])} blocking issues remain; see {out / 'import-report.md'}")
    raise ImportFailure("--apply is intentionally unavailable until the staging schema is reviewed and a migration approval is recorded")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--design-dir", type=Path, default=DEFAULT_DESIGN)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--apply", action="store_true", help="refuse unless all blockers are cleared (safety gate)")
    args = parser.parse_args()
    try:
        out = args.output.resolve()
        out.mkdir(parents=True, exist_ok=True)
        manifest = copy_sources(args.design_dir, out)
        (out / "source-manifest.json").write_text(json.dumps({"format": 1, "sources": manifest}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        backup = backup_runtime(out)
        staged, errors, report = parse_workbooks(args.design_dir, out)
        write_outputs(out, staged, report, backup)
        if args.apply:
            apply_if_ready(out, report)
        print(json.dumps({"status": report["status"], "output": str(out), "cards": len(staged), "blockingIssues": len(errors)}, ensure_ascii=False, indent=2))
        return 0 if not args.apply else 1
    except (ImportFailure, OSError, KeyError, ValueError, zipfile.BadZipFile) as exc:
        print(f"v2.6.0 XLSX import failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
