"""Import the reviewed 2.0 design documents as display-only Godot card data.

This never changes the native playable catalogue. PNGs are copied byte-for-byte
by prepare_godot_frontend_assets.py; the original art remains authoritative.
"""

from __future__ import annotations

import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/card-designs/v2"
OUTPUT = ROOT / "godot/data/card-previews-v2.json"
SPELL_IDS = [
    "fireball", "ray-of-frost", "lightning-bolt", "magic-missile", "chromatic-orb",
    "guiding-bolt", "mend", "false-life", "scorching-ray", "ward", "sunburst",
    "healing-circle", "spark", "disrupt", "true-strike", "barbs", "shield",
    "counter-spell", "protective-flame", "funny-box",
]
FORMATION_IDS = [
    "small-hexagram", "hexagram", "pentagram", "octagram", "sun", "upright-cross",
    "inverted-cross", "ankh", "elemental-ring", "chaos-ring", "void", "sulfur",
]
REUSED = {
    **{key: key for key in (
        "fireball", "ray-of-frost", "lightning-bolt", "magic-missile", "mend",
        "false-life", "ward", "spark", "disrupt", "true-strike", "barbs", "shield",
        "counter-spell", "protective-flame",
    )},
    "hexagram": "balance",
    "pentagram": "pentagram-of-life",
}


def parse_document(filename: str) -> list[tuple[str, dict[str, str]]]:
    document = (SOURCE / filename).read_text(encoding="utf-8-sig")
    cards = []
    for block in re.split(r"(?m)^###\s+", document)[1:]:
        lines = block.splitlines()
        title = lines[0].strip()
        fields = dict(re.findall(r"(?m)^-\s*([^：\n]+)：([^\n]*)", "\n".join(lines[1:])))
        cards.append((title, {key.strip(): value.strip() for key, value in fields.items()}))
    return cards


def build_catalogue() -> dict:
    spells = parse_document("新法术设计.md")
    formations = parse_document("新阵法设计.md")
    if len(spells) != 20 or len(formations) != 12:
        raise ValueError("Expected 20 spells and 12 formations in the source documents")
    cards = []
    for identifier, (title, fields) in zip(SPELL_IDS, spells, strict=True):
        kind = fields.get("法术类型", fields.get("类型"))
        kind = {"解析法数": "解析法术"}.get(kind, kind)
        type_key = {"解析法术": "analysis", "言灵法术": "incantation", "咒符法术": "talisman"}[kind]
        costs = [int(value) for value in fields["费用"].split("/")]
        card = {
            "id": "v2-" + identifier, "name": title, "typeKey": type_key, "typeLabel": kind,
            "level": int(fields["法术等级"]), "school": fields["学派"],
            "costText": fields["费用"], "costParts": costs,
            "analysisTurns": int(fields.get("解析时间", fields.get("解析回合"))),
            "speed": int(fields["速度"]), "text": fields["效果"].replace("**", "").removesuffix("、"),
            "illustrationId": REUSED.get(identifier, "v2-" + identifier), "sourceFields": fields,
        }
        cards.append(card)
    for identifier, (title, fields) in zip(FORMATION_IDS, formations, strict=True):
        cards.append({
            "id": "v2-" + identifier, "name": title, "typeKey": "formation", "typeLabel": "阵法",
            "level": int(fields.get("阵法等级", fields.get("施法等级"))),
            "body": int(fields.get("阵体大小", fields.get("阵法大小"))),
            "rings": int(fields["最大解析数"]), "capacity": int(fields["荷载上限"]),
            "income": int(fields["魔素收入"]), "summonCondition": fields["召唤条件"],
            "text": fields["效果"], "illustrationId": REUSED.get(identifier, "v2-" + identifier),
            "sourceFields": fields,
        })
    return {
        # Preview design data is separate from the authoritative 2.0.0-rc.1
        # playable catalogue and cannot enter a native deck draft.
        "format": 1, "cardSetVersion": "2.0.0-rc.1-preview", "usage": "illustrated-preview",
        "sources": ["assets/card-designs/v2/新法术设计.md", "assets/card-designs/v2/新阵法设计.md"],
        "normalizations": {
            "解析法数": "解析法术", "解析回合": "解析时间", "施法等级（太阳）": "阵法等级",
            "阵法大小": "阵体大小", "效果文本": "Remove Markdown emphasis and trailing stray punctuation only",
        },
        "artReuseNotes": {
            "缝合生命": "生命缝合 / mend", "六芒星": "均衡之六芒星 / balance",
            "五芒星": "生命之五芒星 / pentagram-of-life",
        },
        "unspecifiedFields": ["rarity", "formation speed"],
        "costDisplay": "Preserve the supplied single or slash-separated cost notation without inventing new labels",
        "cards": cards,
    }


if __name__ == "__main__":
    catalogue = build_catalogue()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(catalogue, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Imported {len(catalogue['cards'])} 2.0 preview cards; reused illustrations: {len(REUSED)}")
