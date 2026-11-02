"""Build the authoritative 2.0 card pool from reviewed designs and legacy identities.

The old 1.0 pool remains frozen under tests/fixtures/alpha-v1-production.
This script is deterministic; use --check in CI to detect hand-edited generated data.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from import_card_previews_v2 import build_catalogue
from align_preview2_catalog import align

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / "tests/fixtures/alpha-v1-production/cards.json"
ASSETS = ROOT / "assets"
PREVIEW = build_catalogue()["cards"]


def effect(kind: str, amount: int = 0, **extra: object) -> dict:
    return {"kind": kind, **({"amount": amount} if amount else {}), **extra}


def migrated_cards() -> list[dict]:
    old = json.loads(LEGACY.read_text(encoding="utf-8"))["cards"]
    by_id = {card["id"]: card for card in old}
    migration = json.loads((ROOT / "docs/proposals/alpha-v2/migration.json").read_text(encoding="utf-8"))
    for row in migration["cards"]:
        card = by_id[row["id"]]
        card["type"] = row["proposedType"]
        card["cost"] = row["cost"]
        card["castCost"] = row["castCost"]
        card["rank"] = row["rank"] or (1 if card["type"] in ("word", "talisman") else 0)
        card["speed"] = row["speed"]
        card.pop("baseEligible", None)
        if row.get("level") is not None:
            card["level"] = row["level"]
        if card["type"] == "formation":
            card["baseEligible"] = card["level"] == 1 and card["rings"] > 0
            card["maxRank"] = card["level"]
            card["recipe"] = [
                {"kind": "mana", "amount": item["amount"]} if item["kind"] == "mana"
                else {"kind": "sacrifice_spell", "amount": item["count"], "readyOnly": True}
                for item in row.get("recipe", [])
            ]
        elif card["type"] in ("analytic", "word", "talisman"):
            card["level"] = card["rank"]
        else:
            card["level"] = 0
        if card["type"] == "word" and row["oldType"] == "action":
            card["tags"] = list(dict.fromkeys(card.get("tags", []) + ["former-action"]))
            card["immediate"] = True
        if card["type"] == "talisman":
            card.pop("immediate", None)
            card["responseOnly"] = True
            for ability in card.get("responses", []):
                ability["sources"] = ["ambush"]
        if card["type"] == "word" and card.get("immediate"):
            card["rank"] = card["level"] = max(1, card["rank"])
        if card["type"] == "word":
            card.pop("immediate", None)
        if card["type"] == "formation":
            card["cost"] = 0
    # Card text/effects shared with the new designs are aligned to their numeric source.
    overrides = {
        "fireball": {"rank": 3, "cost": 3, "castCost": 1, "effects": [effect("damage", 10, damageType="fire")],
                     "text": "对一个目标造成10点火焰伤害。"},
        "recall": {"cost": 2},
        "conduit": {"cost": 2},
        "ray-of-frost": {"rank": 2, "cost": 2, "castCost": 1, "effects": [effect("damage", 6, damageType="cold"), effect("delay_enemy_analysis", 1)],
                         "text": "对对方玩家造成6点寒冷伤害。选择对方一张解析法术，使其解析进度后退1回合。"},
        "lightning-bolt": {"rank": 3, "effects": [effect("damage", 8, damageType="thunder"), effect("conceal_enemy_spell")],
                           "text": "对对方玩家造成8点雷鸣伤害。可以将对方解析区一张法术盖伏。"},
        "magic-missile": {"rank": 2, "effects": [effect("damage", 2, damageType="force"), effect("destroy_ambush")],
                          "text": "对对方玩家造成2点力场伤害，然后可以摧毁一张对方盖伏的法术。"},
        "mend": {"rank": 3, "cost": 2, "castCost": 1, "effects": [effect("heal", 4)], "text": "恢复自身4点生命。"},
        "false-life": {"rank": 3, "cost": 2, "castCost": 1, "effects": [effect("temporary_life", 6)],
                       "text": "获得6点临时生命（不叠加，取较高值）。"},
        "ward": {"rank": 2, "cost": 2, "castCost": 0, "effects": [effect("damage", 4, damageType="fire")],
                 "onCast": [effect("damage", 2, damageType="fire")],
                 "text": "专注1。首次发动对对方造成4点火焰伤害；维持时造成2点火焰伤害。"},
        "spark": {"cost": 2, "rank": 1, "text": "对对方玩家造成2点火焰伤害。"},
        "disrupt": {"cost": 2, "rank": 2, "effects": [effect("temporary", 3, recipient="opponent"), effect("temporary", 1, recipient="owner")],
                    "text": "对方获得3点临时荷载，自己获得1点临时荷载。"},
        "true-strike": {"cost": 3, "rank": 3, "effects": [effect("search_same_school")],
                        "text": "选择自己解析区一张法术送入灰烬区，从牌库检索一张同学派法术加入手牌，然后洗切牌库。"},
        "barbs": {"cost": 5, "rank": 3, "speed": 3, "effects": [effect("cancel"), effect("return_enemy_analysis")],
                  "text": "对方准备施法时可发动：取消该次准备，然后可以将对方解析区一张解析法术返回手牌。"},
        "shield": {"rank": 3, "effects": [effect("reduce_spell_damage", 5)],
                   "text": "响应对方造成伤害的法术：该链节的一次伤害降低至多5点；力场伤害降低至多10点。"},
        "counter-spell": {"rank": 5, "cost": 5, "text": "对方发动法术时可发动：无效并摧毁该法术；自身获得其实际魔素消耗等量的临时荷载。"},
        "protective-flame": {"rank": 4, "text": "专注2。自身受到的火焰伤害减半，火焰抗性不叠加。"},
        "reservoir": {"name": "阿克乌姆之弧", "level": 1, "body": 1, "capacity": 15,
                      "income": 0, "rings": 0, "maxRank": 1, "opponentDestroyProtected": False,
                      "baseEligible": False, "recipe": [{"kind": "discard_hand", "amount": 2}],
                      "summonCondition": "2张手牌。", "text": "以两张手牌为材料设置。没有环位；无特殊效果。"},
        "pentagram-of-life": {"summonCondition": "【均衡之六芒星】+已解析的法术。"},
        "engeas-four-point-star": {"summonCondition": "3魔素+已解析的法术。"},
        "unravel": {"text": "指定对方场上一张没有承载法术的阵法。将己方副卡组一张阵法送入灰烬区，然后销毁目标阵法。"},
    }
    for identifier, change in overrides.items():
        by_id[identifier].update(change)
        if "rank" in change and by_id[identifier]["type"] != "formation":
            by_id[identifier]["level"] = by_id[identifier]["rank"]
        if "effects" in change:
            for ability in by_id[identifier].get("responses", []):
                ability["effects"] = change["effects"]
    for preview in PREVIEW[:20]:
        identifier = preview["id"].removeprefix("v2-")
        if identifier not in by_id:
            continue
        card = by_id[identifier]
        card["rank"] = card["level"] = preview["level"]
        card["cost"] = preview["costParts"][0]
        card["castCost"] = preview["costParts"][1] if len(preview["costParts"]) > 1 else 0
        card["speed"] = preview["speed"]
        card["analysisTurns"] = preview["analysisTurns"]
        card["school"] = SCHOOLS[preview["school"]]
    by_id["reservoir"]["level"] = 0
    by_id["ward"]["concentrationUpkeep"] = 1
    by_id["protective-flame"]["concentrationUpkeep"] = 2
    by_id["instantly-sleep"]["text"] = "对方下一个自己的回合不能从手牌发动言灵法术；已盖伏的法术不受影响。"
    by_id["aid"]["text"] = "立即完成解析。获得5点临时生命（不叠加，取较高值），在下次己方准备阶段清除。"
    by_id["conduit"]["text"] = "阵法附着：该阵法在抽牌阶段提供的魔素收入+2。法术附着：无额外效果。"
    by_id["uplift"]["text"] = "阵法附着：等级上限+1、环位+1、抽牌阶段魔素收入-1。法术附着：实际施法费用+1，成功发动时额外对对方造成2点力场伤害。"
    by_id["uplift"].pop("capacityBonus", None)
    by_id["uplift"]["ringBonus"] = 1
    by_id["instant-circuit-overload"]["text"] = "立即完成己方一张正在解析的法术；自身获得其实际解析费用等量的临时荷载，在下次己方准备阶段清除。"
    by_id["barbs"]["responses"][0]["target"] = "preparation_root"
    by_id["shield"]["responses"][0]["target"] = "enemy_spell_link"
    by_id["shield"]["target"] = "self"
    by_id["shield"]["responseOnly"] = True
    by_id["ward"]["concentration"] = True
    by_id["ward"]["onCast"] = [effect("damage", 2, damageType="fire")]
    by_id["counter-spell"]["school"] = "abjuration"
    by_id["true-strike"]["target"] = "self"
    by_id["true-strike"].pop("immediate", None)
    by_id["barbs"]["responses"][0]["name"] = "银光反制"
    by_id["shield"]["responses"][0]["name"] = "降低法术伤害"
    by_id["shield"]["responses"][0]["eventOwner"] = "opponent"

    # The old balance formation and the reviewed hexagram represented the same
    # six-point formation in the release pool. Keep one authoritative card ID;
    # the newer definition keeps its mechanics and illustration while inheriting
    # the established name and flavor text from the old card.
    # Drop the legacy definition from the generated list. The reviewed
    # formation with id ``hexagram`` receives the inherited display text in
    # ``fresh_formations`` below.
    by_id.pop("balance")
    # Old abilities remain mechanically active, with 2.0 terminology.
    for card in old:
        if card["id"] not in overrides and card["type"] in ("word", "talisman"):
            card["rank"] = card["level"] = max(1, card["rank"])
        if card["id"] in ("recall", "arcane-recovery", "clarity", "disrupt"):
            card["text"] = card["text"].replace("本回合结束阶段", "下一次己方准备阶段").replace("下一个自己的结束阶段", "下一次己方准备阶段")
        if card["id"] == "pentagram-of-life":
            card["level"] = 5
            card["maxRank"] = 5
            card["body"] = 2
            card["rings"] = 3
            card["capacity"] = 10
            card["income"] = 2
            card["baseEligible"] = False
            card["baseAnalysisDiscount"] = False
            card["recipe"] = [{"kind": "sacrifice_formation", "amount": 1, "definition": "hexagram"},
                              {"kind": "sacrifice_spell", "amount": 1, "readyOnly": True}]
            card["onPrepare"] = [effect("independent", 1)]
            card["text"] = "己方准备阶段开始时，自己获得1点独立荷载。"
        if card["id"] == "engeas-four-point-star":
            card["income"] = 3
            card["recipe"] = [{"kind": "mana", "amount": 3},
                              {"kind": "sacrifice_spell", "amount": 1, "readyOnly": True}]
    return [card for card in old if card["id"] != "balance"]


NEW_SPELLS = {
    "chromatic-orb": ("analytic", "evocation", "opponent", [effect("choose_damage", 8)], {}),
    "guiding-bolt": ("analytic", "evocation", "opponent", [effect("damage", 8, damageType="radiant"), effect("reveal_enemy_ambush")], {}),
    "scorching-ray": ("analytic", "necromancy", "opponent", [effect("damage", 5, damageType="fire") for _ in range(4)], {}),
    "sunburst": ("analytic", "evocation", "opponent", [effect("damage", 15, damageType="fire"), effect("destroy_formation_spells")],
                 {"traits": ["ash_evocation_discount"]}),
    "healing-circle": ("analytic", "abjuration", "self", [], {"duration": 3, "onDraw": [effect("heal", 2)]}),
    "funny-box": ("talisman", "enchantment", "self", [effect("recover_enemy_ash_spell")], {}),
}

# New RC cards use an explicit rarity curve instead of inheriting one blanket
# value. Legacy cards keep the reviewed rarities from the migrated source.
NEW_SPELL_RARITIES = {
    "chromatic-orb": "rare",
    "guiding-bolt": "rare",
    "scorching-ray": "epic",
    "sunburst": "legendary",
    "healing-circle": "rare",
    "funny-box": "epic",
}

SCHOOLS = {"塑能": "evocation", "死灵": "necromancy", "防护": "abjuration", "咒法": "conjuration",
           "预言": "divination", "惑控": "enchantment"}


def fresh_spells() -> list[dict]:
    cards = []
    for preview in PREVIEW[:20]:
        identifier = preview["id"].removeprefix("v2-")
        if identifier not in NEW_SPELLS:
            continue
        kind, school, target, effects, extra = NEW_SPELLS[identifier]
        costs = preview["costParts"]
        card = {
            "id": identifier, "name": preview["name"], "type": kind,
            "rarity": NEW_SPELL_RARITIES[identifier],
            "rank": preview["level"], "level": preview["level"], "speed": preview["speed"],
            "school": school, "cost": costs[0], "castCost": costs[1] if len(costs) > 1 else 0,
            "analysisTurns": preview["analysisTurns"], "target": target,
            "effects": effects, "text": preview["text"], **extra,
        }
        if kind == "talisman":
            card["responseOnly"] = False
        cards.append(card)
    return cards


FORMATION_DATA = {
    "small-hexagram": {"recipe": [{"kind": "mana", "amount": 2}], "traits": ["unique"]},
    "hexagram": {"recipe": [{"kind": "sacrifice_formation", "amount": 1, "definition": "small-hexagram"},
                             {"kind": "mana", "amount": 2}], "traits": ["effect_indestructible"]},
    "octagram": {"recipe": [{"kind": "sacrifice_formation", "amount": 1, "definition": "small-hexagram"},
                             {"kind": "sacrifice_formation", "amount": 1, "definition": "hexagram"}],
                  "traits": ["effect_indestructible", "clear_load_on_analysis"]},
    "sun": {"recipe": [{"kind": "sacrifice_formation", "amount": 1, "definition": "pentagram-of-life"},
                       {"kind": "sacrifice_formation", "amount": 1, "definition": "octagram"}],
            "traits": ["effect_immune"], "onEnd": [effect("reveal_all_ambush")]},
    "upright-cross": {"recipe": [{"kind": "mana", "amount": 1},
                                 {"kind": "sacrifice_spell", "amount": 1, "school": "necromancy"}],
                      "traits": ["host_heal_plus_one"]},
    "inverted-cross": {"recipe": [{"kind": "mana", "amount": 1},
                                  {"kind": "sacrifice_spell", "amount": 1, "school": "necromancy"}],
                       "traits": ["host_cast_life_for_temporary"]},
    "ankh": {"recipe": [{"kind": "mana", "amount": 1},
                        {"kind": "sacrifice_spell", "amount": 1, "school": "necromancy", "readyOnly": True}],
             "traits": ["return_formation_from_ash", "self_ambush_on_leave"]},
    "elemental-ring": {"recipe": [{"kind": "mana", "amount": 1},
                                  {"kind": "sacrifice_spell", "amount": 1, "school": "evocation", "readyOnly": True}],
                       "traits": ["host_evocation_damage_plus_one"]},
    "chaos-ring": {"recipe": [{"kind": "sacrifice_spell", "amount": 2, "distinctSchools": True, "readyOnly": True}],
                   "traits": ["distinct_host_schools"]},
    "sulfur": {"recipe": [{"kind": "sacrifice_spell", "amount": 2, "readyOnly": True, "fireDamage": True}],
               "traits": ["host_fire_only", "ping_on_spell_leave"]},
}

NEW_FORMATION_RARITIES = {
    "small-hexagram": "common",
    "hexagram": "uncommon",
    "octagram": "epic",
    "sun": "legendary",
    "upright-cross": "uncommon",
    "inverted-cross": "uncommon",
    "ankh": "rare",
    "elemental-ring": "uncommon",
    "chaos-ring": "rare",
    "sulfur": "epic",
}


def fresh_formations() -> list[dict]:
    cards = []
    for preview in PREVIEW[20:]:
        identifier = preview["id"].removeprefix("v2-")
        if identifier in ("pentagram", "void"):
            continue
        card = {
            "id": identifier, "name": preview["name"], "type": "formation",
            "rarity": NEW_FORMATION_RARITIES[identifier], "level": preview["level"], "body": preview["body"],
            "rings": preview["rings"], "capacity": preview["capacity"],
            "income": preview["income"], "maxRank": preview["level"],
            "baseEligible": preview["level"] == 1 and preview["rings"] > 0,
            "cost": 0, "text": preview["text"], "summonCondition": preview["summonCondition"],
            **FORMATION_DATA[identifier],
        }
        if identifier == "hexagram":
            card["name"] = "均衡之六芒星"
            card["flavor"] = "魔法界最流行的一种法阵，你几乎可以在任何一种复杂术式中看到它的身影。六芒星诉说着它的信念：秩序不偏向任何一边。"
        cards.append(card)
    return cards


def build() -> tuple[dict, dict, dict, dict]:
    cards = migrated_cards() + fresh_spells() + fresh_formations()
    # Prefixes encode type, while the suffix is an independent stable sequence in that type.
    prefixes = {"formation": "01", "analytic": "02", "word": "03", "talisman": "04", "seal": "05"}
    count = {key: 0 for key in prefixes}
    for card in cards:
        count[card["type"]] += 1
        card["number"] = f"{prefixes[card['type']]}{count[card['type']]:04d}"
        card["illustrationId"] = card["id"] if card["id"] in {c["id"] for c in json.loads(LEGACY.read_text(encoding="utf-8"))["cards"]} else "v2-" + card["id"]
        if card["id"] == "hexagram":
            card["illustrationId"] = "balance"
    align(cards)
    by_id = {card["id"]: card for card in cards}
    by_id["messy-wave"]["text"] = "计时3。首次发动时对方获得5点来源关联临时荷载；之后每个己方施法阶段对方获得3点来源关联临时荷载。这些荷载不在抽牌阶段清除，在本法术离场时清除。"
    by_id["ankh"]["text"] = "设置时，可以将己方灰烬区一张表侧阵法卡加入副卡组。本卡进入灰烬区时背面朝上放置。"
    for card in cards:
        card["text"] = card["text"].replace("下一次己方准备阶段", "下一次己方抽牌阶段").replace("下次己方准备阶段", "下次己方抽牌阶段")
    # Keep the old manifest order and append reviewed new designs in source order.
    manifest = {"format": 1, "metadata": {"rulesVersion": "2.5.1",
                "cardSetVersion": "2.5.1", "baseLoadCapacity": 2, "mechanicsRevision": 4},
                "definitions": [{"id": c["id"], "file": f"cards/{c['type']}/{c['id']}.json"} for c in cards]}
    reviewed = json.loads((ROOT / "docs/proposals/alpha-v2/presets.json").read_text(encoding="utf-8"))
    # The runtime always supplies the canonical built-in ``default`` preset
    # from assets/deck.json. Keep the reviewed file's copy as a renamed,
    # selectable preset so every reviewed list remains available.
    presets = []
    for p in reviewed["presets"]:
        # ``default`` is also the runtime's built-in deck from assets/deck.json.
        # Preserve the reviewed legacy list as a separate selectable preset so
        # the release still exposes every reviewed 40+10 list without an ID
        # collision during Application::loadPresets().
        preset_id = "default-legacy" if p["id"] == "default" else p["id"]
        preset_name = "默认预设（旧版）" if p["id"] == "default" else p["name"]
        presets.append({"id": preset_id, "name": preset_name, "description": p.get("description", ""),
                        "baseFormation": p["initialFormation"], "cards": p["main"], "sideCards": p["side"]})
    side_cards = [{"id": identifier, "count": amount} for identifier, amount in
                  [("small-hexagram", 3), ("hexagram", 2), ("pentagram-of-life", 1),
                   ("octagram", 1), ("reservoir", 1), ("engeas-four-point-star", 1), ("elemental-ring", 1)]]
    for preset in presets:
        preset["baseFormation"] = "small-hexagram"
        preset["sideCards"] = side_cards
    # Representative default: 40 main cards, 10 side cards, initial formation in side deck.
    deck = presets[0]["cards"]
    side = {"initialFormation": presets[0]["baseFormation"], "cards": presets[0]["sideCards"]}
    aggregate = dict(manifest["metadata"], cards=cards)
    return manifest, aggregate, deck, side, presets


def write(path: Path, value: object, check: bool) -> None:
    data = json.dumps(value, ensure_ascii=False, indent=2) + "\n"
    if check:
        if not path.exists() or path.read_text(encoding="utf-8") != data:
            raise SystemExit(f"Stale generated catalog: {path.relative_to(ROOT)}")
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(data, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    check = parser.parse_args().check
    manifest, aggregate, deck, side, presets = build()
    write(ASSETS / "catalog.json", manifest, check)
    for entry, card in zip(manifest["definitions"], aggregate["cards"], strict=True):
        write(ASSETS / entry["file"], card, check)
    write(ASSETS / "cards.json", aggregate, check)
    write(ASSETS / "deck.json", deck, check)
    write(ASSETS / "side-deck.json", side, check)
    write(ASSETS / "presets.json", presets, check)
    print(f"Authoritative 2.0 catalog: {len(aggregate['cards'])} cards, {len(presets)} presets")


if __name__ == "__main__":
    main()
