"""Apply and validate the reviewed preview.2 spell/formation contract.

Stable native IDs and unlisted cards are retained. The supplied prose is card
specification data, not commands to the build or delivery workflow.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"

def effect(kind: str, amount: int = 0, **extra: object) -> dict:
    return {"kind": kind, **({"amount": amount} if amount else {}), **extra}

SPELLS = {
    "fireball": ("火球术", "analytic", 3, "evocation", 3, 1, 1, 1, "opponent", [effect("damage", 10, damageType="fire")]),
    "ray-of-frost": ("寒霜射线", "analytic", 2, "evocation", 2, 1, 1, 1, "opponent", [effect("damage", 6, damageType="cold"), effect("delay_enemy_analysis", 1)]),
    "lightning-bolt": ("闪电束", "analytic", 3, "evocation", 3, 2, 1, 1, "opponent", [effect("damage", 8, damageType="thunder"), effect("destroy_enemy_analysis")]),
    "magic-missile": ("魔法飞弹", "analytic", 2, "evocation", 2, 2, 1, 2, "opponent", [effect("damage", 2, damageType="force"), effect("destroy_ambush")]),
    "chromatic-orb": ("繁彩球", "analytic", 2, "evocation", 2, 2, 1, 1, "opponent", [effect("choose_damage", 8)]),
    "guiding-bolt": ("曳光弹", "analytic", 2, "evocation", 2, 2, 1, 1, "opponent", [effect("damage", 8, damageType="radiant"), effect("reveal_enemy_ambush")]),
    "mend": ("缝合生命", "analytic", 3, "necromancy", 2, 1, 1, 2, "player", [effect("heal", 4)]),
    "false-life": ("虚假生命", "analytic", 3, "necromancy", 2, 1, 1, 2, "self", [effect("temporary_life", 6)]),
    "scorching-ray": ("炽热射线", "analytic", 5, "evocation", 4, 2, 2, 1, "opponent", [effect("damage", 5, damageType="fire") for _ in range(3)]),
    "ward": ("炽燃结界", "analytic", 2, "evocation", 2, 0, 1, 1, "opponent", [effect("damage", 4, damageType="fire")]),
    "sunburst": ("阳炎爆", "analytic", 6, "evocation", 20, 2, 2, 3, "opponent", [effect("damage", 15, damageType="fire"), effect("destroy_formation_spells")]),
    "healing-circle": ("治疗之环", "analytic", 2, "necromancy", 3, 0, 2, 1, "self", []),
    "spark": ("火花术", "word", 1, "evocation", 2, 0, 0, 2, "opponent", [effect("damage", 3, damageType="fire")]),
    "disrupt": ("魔力扰动", "word", 2, "conjuration", 2, 0, 0, 2, "opponent", [effect("temporary", 3, recipient="opponent"), effect("temporary", 1, recipient="owner")]),
    "true-strike": ("克敌机先", "word", 3, "divination", 3, 0, 0, 2, "self", [effect("search_same_school")]),
    "recall": ("奥术回想", "word", 1, "divination", 3, 0, 0, 2, "self", [effect("draw", 2), effect("temporary", 2)]),
    "barbs": ("银光锐语", "talisman", 3, "enchantment", 5, 0, 0, 3, "self", [effect("cancel"), effect("return_enemy_analysis")]),
    "shield": ("护盾术", "talisman", 3, "abjuration", 3, 0, 0, 2, "self", [effect("shield_for_turn", 5)]),
    "counter-spell": ("法术反制", "talisman", 5, "abjuration", 5, 0, 0, 4, "self", [effect("counter_spell")]),
    "protective-flame": ("避火咒", "talisman", 4, "abjuration", 2, 0, 0, 2, "self", [effect("grant_resistance")]),
    "funny-box": ("滑稽魔盒", "talisman", 5, "enchantment", 4, 0, 0, 3, "enemy_ash_card", [effect("recover_enemy_ash_spell")]),
}

TEXTS = {
    "fireball": "对对方玩家造成10点火焰伤害。",
    "ray-of-frost": "对对方玩家造成6点寒冷伤害。那之后，可以使对方解析区一张解析法术的解析进度-1。",
    "lightning-bolt": "对对方玩家造成8点雷鸣伤害。那之后，可以销毁对方解析区一张解析法术。",
    "magic-missile": "对对方玩家造成2点力场伤害。那之后，可以销毁对方解析区一张盖伏法术。",
    "chromatic-orb": "对对方玩家造成8点自选伤害，从强酸/寒冷/火焰/闪电/毒素/雷鸣中选择。",
    "guiding-bolt": "对对方玩家造成8点光耀伤害。那之后，可以展示对方解析区一张盖伏法术（不反转）。",
    "mend": "选择己方或对方玩家，为其恢复4点生命值。",
    "false-life": "获得6点临时生命值（不叠加，取较高值）。",
    "scorching-ray": "对对方玩家造成5点火焰伤害，重复三次。",
    "ward": "专注1。对对方玩家造成4点火焰伤害；成功维持专注时对对方玩家造成2点火焰伤害。",
    "sunburst": "对对方玩家造成15点火焰伤害。那之后，可以销毁对方一个阵法承载的所有法术。己方灰烬区每有一张塑能学派法术，此卡设置费用-1（最低0）；发动费用不变。",
    "healing-circle": "计时3。己方结束阶段开始时，恢复2点生命值。",
    "spark": "对对方玩家造成3点火焰伤害。",
    "disrupt": "对方玩家获得3点临时荷载，己方玩家获得1点临时荷载。",
    "true-strike": "将另一张手牌中的法术送入灰烬区作为发动成本。从牌库检索一张同学派法术加入手牌，然后洗切牌库。",
    "recall": "抽两张卡，获得2点临时荷载。",
    "barbs": "对方准备施法时才能发动。将此次准备施法无效。那之后，可以将对方解析区一张解析法术弹回持有者手牌。",
    "shield": "此回合，每次受到伤害时降低至多5点；力场伤害降低至多10点。",
    "counter-spell": "对方发动法术时才能发动。将其无效并销毁。那之后，获得与其原本设置费用等量的临时荷载。",
    "protective-flame": "专注2。受到的火焰伤害减半，火焰抗性不叠加。",
    "funny-box": "选择对方灰烬区一张5级及以下解析法术，将其在己方有合法空环位的阵法上盖伏。",
}

FORMATIONS = {
    "small-hexagram": ("运行之三角星", 1, 1, 1, 5, 1, [effect("mana", 2)], "此卡在己方阵法槽内仅可存在一张。"),
    "hexagram": ("均衡之六芒星", 3, 2, 3, 8, 2, [], "此卡不会被效果破坏。"),
    "pentagram-of-life": ("生命之五芒星", 5, 2, 3, 8, 3, [], "己方准备阶段开始时，获得1点独立荷载。"),
    "octagram": ("秩序之八芒星", 6, 2, 4, 10, 4, [], "此卡不会被效果破坏。己方每有一张解析法术解析完成，清除1点荷载。"),
    "sun": ("至圣之太阳星", 7, 4, 7, 15, 8, [], "此卡不受其他卡的效果影响。在己方阵法槽内存在时，可随时查看对方解析区盖伏法术。"),
    "elemental-ring": ("元素之环", 3, 2, 2, 8, 1, [], "在此阵法上解析的塑能法术伤害+1。"),
    "chaos-ring": ("混沌之环", 3, 2, 5, 12, 1, [], "此阵法不能同时解析相同学派法术。"),
    "sulfur": ("硫磺", 6, 2, 6, 6, 3, [], "此阵法只能解析火焰伤害法术。每当己方法术从施法区进入灰烬区，对对方玩家造成1点火焰伤害。"),
}

def align(cards: list[dict]) -> None:
    by_id = {card["id"]: card for card in cards}
    for identifier, (name, kind, level, school, setting, cast, turns, speed, target, effects) in SPELLS.items():
        card = by_id[identifier]
        card.update(name=name, type=kind, rank=level, level=level, school=school,
                    cost=setting, castCost=cast, analysisTurns=turns, speed=speed,
                    target=target, effects=effects, text=TEXTS[identifier])
        for response in card.get("responses", []):
            response["effects"] = effects
            if identifier in ("spark", "mend", "magic-missile"):
                response["target"] = target
            if kind == "talisman":
                response["sources"] = ["ambush"]
    by_id["true-strike"]["traits"] = ["discard_spell_search"]
    by_id["ward"].update(concentration=True, concentrationUpkeep=1, onCast=[effect("damage", 2, damageType="fire")])
    circle = by_id["healing-circle"]
    circle.pop("onDraw", None)
    circle.update(duration=3, onEnd=[effect("heal", 2)], traits=["end_phase_start"])
    by_id["shield"].update(responseOnly=False)
    for response in by_id["shield"].get("responses", []):
        response.update(target="self", name="本回合护盾")
        response.pop("eventOwner", None)
    for identifier in ("barbs", "counter-spell"):
        for response in by_id[identifier]["responses"]:
            response["eventOwner"] = "opponent"
            response["windows"] = ["prepare" if identifier == "barbs" else "cast"]
    by_id["protective-flame"].update(responseOnly=False, concentrationUpkeep=2)
    by_id["arcane-recovery"]["text"] = by_id["arcane-recovery"]["text"].replace("不超过12", "不超过20")
    for identifier, (name, level, body, rings, capacity, income, _, text) in FORMATIONS.items():
        by_id[identifier].update(name=name, level=level, maxRank=level, body=body,
                                 rings=rings, capacity=capacity, income=income, text=text)
    by_id["hexagram"]["summonCondition"] = "【运行之三角星】+魔素×2。"
    by_id["octagram"]["summonCondition"] = "【运行之三角星】+【均衡之六芒星】"
    by_id["sun"].update(summonCondition="【生命之五芒星】+【秩序之八芒星】", traits=["effect_immune", "inspect_enemy_ambush"])
    by_id["sun"].pop("onEnd", None)
    # Move the former word into the talisman range without renumbering other cards.
    by_id["protective-flame"]["number"] = "040005"

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    manifest = json.loads((ASSETS / "catalog.json").read_text(encoding="utf-8"))
    cards = [json.loads((ASSETS / entry["file"]).read_text(encoding="utf-8")) for entry in manifest["definitions"]]
    align(cards)
    outputs = {}
    for entry, card in zip(manifest["definitions"], cards, strict=True):
        entry["file"] = f"cards/{card['type']}/{card['id']}.json"
        outputs[ASSETS / entry["file"]] = card
    outputs[ASSETS / "catalog.json"] = manifest
    outputs[ASSETS / "cards.json"] = dict(manifest["metadata"], cards=cards)
    for path, value in outputs.items():
        data = json.dumps(value, ensure_ascii=False, indent=2) + "\n"
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != data:
                raise SystemExit(f"Alignment stale: {path.relative_to(ROOT)}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(data, encoding="utf-8")
    print(f"Preview.2 alignment verified: {len(SPELLS)} spells, {len(FORMATIONS)} formations; {len(cards)} cards retained")

if __name__ == "__main__":
    main()
