"""Reproducible feedback corrections over the archived v2.6 workbook snapshot."""
from __future__ import annotations

import json
from pathlib import Path

METADATA = {"rulesVersion": "2.6.2", "cardSetVersion": "2.6.2", "mechanicsRevision": 11, "status": "released"}


def correct(card: dict) -> dict:
    name = card["name"]
    # Keep the published 2.6.0 card corrections when promoting the archived workbook.
    rune_text = {
        "v260-seal-0001": "附着于阵法：魔素收入+1。 附着于法术：无效果。",
        "v260-seal-0002": "附着于阵法：环位+2。 附着于法术：发动时，获得其实际设置费用的魔素。",
        "v260-seal-0003": "附着于阵法：阵法等级+1，环位+1，魔素收入-1。 附着于法术：发动费用+1，额外造成5点力场伤害。",
    }
    if card["id"] in rune_text:
        card["text"] = rune_text[card["id"]]
    if card["id"] == "v260-word-0005":
        card["target"] = "own_analyzing_spell"
    def effect(kind: str, amount: int = 0, **kwargs) -> dict:
        return {"kind": kind, "amount": amount, **kwargs}
    replacements = {
        "步履维艰": [effect("setup_delay", 1)],
        "扰神曲": [effect("block_concentration")],
        "降咒": [effect("word_cast_damage", 2, damageType="necrotic")],
        "加速术": [effect("extra_turn", 1), effect("double_load", 2)],
        "昏昏欲睡": [effect("block_analytic")],
        "脆弱诅咒": [effect("damage_taken_bonus", 3)],
        "朦胧术": [effect("choose_resistance")],
        "油腻术": [effect("half_rings")],
        "法师之手": [effect("borrow_analysis")],
        "异界探知": [effect("inspect_enemy_deck", 3)],
        "命令术": [effect("command_choice")],
        "侦测善恶": [effect("inspect_and_route")],
        "克敌机先": [effect("search_same_school", 1)],
        "次级复原": [effect("heal", 2), effect("clear_any_load", 6, recipient="owner")],
        "心灵尖刺": [effect("damage", 10, damageType="psychic"), effect("inspect_enemy_ambush_all")],
        "碎环术": [effect("destroy", 1)],
        "弹射术": [effect("damage", 10, damageType="bludgeoning")],
        "七彩喷射": [effect("choose_damage", 5), effect("protect_player")],
    }
    passive = {"死佑护甲", "吸收元素", "防死结界", "恐惧术", "疯狂冠冕", "荆棘丛生"}
    delayed = {"臭云术", "地震术", "精神监狱", "祝福术", "灾祸术"}
    touched = name in replacements or name in passive or name in delayed or name in {
        "炼狱叱喝", "解除魔法", "法术反制", "阳炎爆", "云雾术", "致病射线"}
    if name in replacements:
        card["effects"] = replacements[name]
    if name in passive or name in delayed:
        card["effects"] = []
    if name == "臭云术":
        card["traits"] = ["both_prepare"]
    if name == "精神监狱":
        card["traits"] = ["hand_or_psychic", "both_end", "end_phase_start"]
        card["onEnd"] = [effect("hand_or_damage", 10, damageType="psychic")]
    if name in {"祝福术", "灾祸术"}:
        card["traits"] = ["end_phase_start"]
        card["onEnd"] = [effect("blessing_choice" if name == "祝福术" else "calamity_choice")]
        card.pop("onEndChoice", None)
    if name == "恐惧术":
        card["traits"] = ["fear"]
    if name == "云雾术":
        card["traits"] = ["cloud_mist"]
    if name == "致病射线":
        card["effects"][1] = effect("setup_damage", 1, damageType="poison")
    targets = {"油腻术": "enemy_formation", "法师之手": "enemy_analysis_spell", "侦测善恶": "enemy_ambush",
               "碎环术": "empty_enemy_formation", "弹射术": "player", "七彩喷射": "player", "朦胧术": "none"}
    if name in targets:
        card["target"] = targets[name]
    if name == "侦测善恶":
        card["responseOnly"] = False
    if name == "碎环术":
        card["extraCost"] = "side_formation"
    if name == "弹射术":
        card["extraCost"] = "analysis_spell"
    if name == "阳炎爆":
        card["speed"] = 2
    if name == "防死结界":
        card["text"] = "永续。此卡在己方解析区仅可存在1张。发动后，己方受到致命伤害时，将此法术卡抹除，免疫那次伤害。那之后，己方生命值变为1。"
    if name == "步履维艰":
        card["text"] = "此卡发动后，此回合对方从手卡设置的解析法术卡的解析时间+1。"
    if name in {"炼狱叱喝", "解除魔法", "法术反制"}:
        kind, window, target = {"炼狱叱喝": ("reflect_damage", "damage", "none"),
            "解除魔法": ("negate_setup_return", "prepare", "enemy_spell_link"),
            "法术反制": ("counter_spell", "cast", "enemy_spell_link")}[name]
        effects = [effect(kind)]
        card["effects"] = []
        card["responses"] = [{"id": "respond", "name": name, "sources": ["ambush", "casting"],
            "windows": [window], "eventOwner": "self" if name == "炼狱叱喝" else "opponent",
            "target": target, "effects": effects}]
        if name == "法术反制":
            card["traits"] = ["counter_spell"]
        card.pop("responseEffects", None)
    if touched:
        card["effectStatus"] = card["implementationStatus"] = "implemented"
        card["effectBlockers"] = card["implementationBlockers"] = []
    return card


def apply_release(directory: Path) -> None:
    catalog_path = directory / "catalog.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    catalog["metadata"].update(METADATA)
    cards = []
    for entry in catalog["definitions"]:
        path = directory / entry["file"]
        card = correct(json.loads(path.read_text(encoding="utf-8")))
        path.write_text(json.dumps(card, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        cards.append(card)
    catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    aggregate = {**catalog["metadata"], "cards": cards}
    (directory / "cards.json").write_text(json.dumps(aggregate, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    release_path = directory / "RELEASE.json"
    if release_path.exists():
        release = json.loads(release_path.read_text(encoding="utf-8"))
        release.update(METADATA)
        release["corrections"] = "Alpha v2.6.2 feedback 2026-11-04 through 2026-11-05"
        release_path.write_text(json.dumps(release, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    apply_release(Path(__file__).resolve().parents[1] / "assets/release-v260")
