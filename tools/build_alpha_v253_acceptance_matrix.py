"""Generate an evidence index; written tests are not reported as passed tests.

Coverage below is reviewed against assertions in current-catalogue regressions.
Legacy fixtures are listed only as reading leads, never as current acceptance.
"""
from __future__ import annotations
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/reports"
TESTS = {
    "253-campfire": ("alpha_v253_rules.cpp", "campfire triggers once"),
    "253-ring": ("alpha_v253_rules.cpp", "elemental ring adds one"),
    "253-crest": ("alpha_v253_rules.cpp", "Storm Crest confirmed counter thresholds"),
    "253-cauldron": ("alpha_v253_rules.cpp", "cauldron allows non Sique"),
    "253-runes": ("alpha_v253_rules.cpp", "cauldron rejects other runes"),
    "253-selections": ("sique.cpp", "Sique selections require one through five"),
    "253-flavor": ("sique.cpp", "Sique flavor shift can only attach"),
    "253-soup-sugar": ("sique.cpp", "soup pays exactly one owned rune"),
    "253-ingredients": ("sique.cpp", "ingredients tracks the searched instances"),
    "253-divine-stew": ("sique.cpp", "divine stew substitutes only mana"),
    "253-hanged-man": ("sique.cpp", "hanged man pays rounded ordinary life"),
    "253-magician": ("sique.cpp", "magician lets the opponent choose"),
    "253-word": ("alpha_v253_rules.cpp", "hand word responses share legal generation"),
    "pool": ("alpha_v252_content.cpp", "current pool has unique executable identities"),
    "arcana": ("alpha_v252_rules.cpp", "every nonconditional Arcana source"),
    "arcana-cost": ("alpha_v252_rules.cpp", "Arcana costs and conditions reject"),
    "hooks": ("alpha_v252_rules.cpp", "Arcana analysis hooks count events"),
    "devil": ("alpha_v252_rules.cpp", "Devil changes an opponent selected target"),
    "lovers": ("alpha_v252_rules.cpp", "Lovers departure forces"),
    "passives": ("alpha_v252_rules.cpp", "Hierophant absorbs one event"),
    "earthquake": ("alpha_v252_rules.cpp", "Earthquake removes both players"),
    "seals": ("alpha_v252_rules.cpp", "formations and spells accept several paid seals"),
    "remove": ("alpha_v252_rules.cpp", "removal is offered and shares"),
    "flip": ("alpha_v252_rules.cpp", "unconfigured Storm Vortex flips"),
    "flip-denied": ("alpha_v252_rules.cpp", "generic ambush activation keeps wait speed priority cost"),
    "wind-strike": ("alpha_v252_rules.cpp", "Wind Strike responds only"),
    "roast": ("alpha_v252_content.cpp", "roast recovers any Sique card"),
    "blowfire": ("alpha_v252_content.cpp", "blowfire optional discard"),
    "backfire": ("alpha_v252_content.cpp", "backfire recycles opponents cards"),
    "stars": ("alpha_v252_content.cpp", "new star hosts shorten analysis"),
    "moon": ("alpha_v252_content.cpp", "moon blocks face up setup"),
    "rays": ("alpha_v252_content.cpp", "scorching rays choose a fresh player"),
    "ice": ("alpha_v252_content.cpp", "ice storm optionally delays every enemy"),
    "circuit-sleep": ("alpha_v252_content.cpp", "instant circuit advances exactly two turns"),
    "empress": ("alpha_v252_acceptance.cpp", "Empress heals and clears two loads"),
    "tower-protection": ("alpha_v252_acceptance.cpp", "Tower respects effect indestructibility"),
    "vinegar": ("alpha_v252_acceptance.cpp", "Vinegar grants attach and detach life"),
    "chili": ("alpha_v252_acceptance.cpp", "Chili deals one damage per actual opponent draw"),
    "pool-formations": ("alpha_v252_pool.cpp", "every formal formation executes its printed recipe"),
    "pool-seals": ("alpha_v252_pool.cpp", "every formal seal attaches through authority"),
    "pool-spells": ("alpha_v252_pool.cpp", "every ordinary formal spell resolves its source"),
    "pool-responses": ("alpha_v252_pool.cpp", "each conditional formal response joins"),
    "unravel": ("alpha_v252_rules.cpp", "Unravel completes setup analysis targetless preparation"),
    "unravel-boundaries": ("alpha_v252_rules.cpp", "Unravel rejects occupied enemy formations"),
    "unravel-invalidated": ("alpha_v252_rules.cpp", "Unravel loses an invalidated chain target"),
    "response-cost": ("alpha_v252_rules.cpp", "all five conditional responses reject unpaid fees"),
    "unique-strength": ("alpha_v252_rules.cpp", "unique Strength rejects same owner setup duplicates"),
    "sun": ("alpha_v252_rules.cpp", "Sun clears only enemy concealed spells"),
    "messy-wave": ("alpha_v252_rules.cpp", "Messy Wave adds four temporary load"),
    "snow-rain": ("alpha_v252_rules.cpp", "Snow Rain Storm surcharges setting only"),
    "woodpile": ("alpha_v252_rules.cpp", "Woodpile allows two own copies"),
}


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def test_reference(filename, fragment):
    path = ROOT / "tests" / filename
    for line, text in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if "TEST_CASE(" in text and fragment in text:
            return {"file": "tests/" + filename, "line": line, "name": re.search(r'TEST_CASE\("([^"]+)', text)[1]}
    raise ValueError("Test reference drift: " + filename + ": " + fragment)


def main():
    tests = {key: test_reference(*value) for key, value in TESTS.items()}
    catalog = read(ROOT / "assets/catalog.json")
    cards = [read(ROOT / "assets" / entry["file"]) for entry in catalog["definitions"]]
    evidence = {card["id"]: {axis: [] for axis in ("normalFlow", "normal", "invalidTarget", "invalidCost", "invalidTiming")} for card in cards}

    def record(ids, axis, refs):
        for card_id in ids.split():
            evidence[card_id][axis] += refs.split()

    arcana = "arcana-fool arcana-magician arcana-priestess arcana-empress arcana-emperor arcana-hierophant arcana-lovers arcana-chariot arcana-strength arcana-hermit arcana-justice arcana-hanged-man arcana-death arcana-temperance arcana-tower arcana-world earthquake"
    # The shared test exercises activation from Casting, not setting or parsing.
    for axis in ("normal", "invalidTarget", "invalidTiming"):
        record(arcana, axis, "arcana")
    record("arcana-fool arcana-magician arcana-priestess arcana-emperor arcana-hanged-man", "invalidCost", "arcana-cost")
    record("arcana-magician arcana-priestess arcana-emperor arcana-hierophant arcana-hanged-man arcana-world", "invalidCost", "arcana")
    record("arcana-devil", "normal", "devil hooks")
    record("arcana-chariot arcana-death", "normal", "hooks")
    record("arcana-lovers", "normal", "lovers")
    record("arcana-hierophant arcana-strength arcana-temperance", "normal", "passives")
    record("arcana-empress", "normal", "empress")
    record("arcana-empress", "invalidTiming", "empress")
    record("arcana-tower", "normal", "tower-protection")
    record("arcana-tower", "invalidTarget", "tower-protection")
    record("earthquake", "normal", "earthquake")
    record("conduit uplift", "normal", "seals")
    record("hexagram", "normal", "remove")
    record("hexagram", "invalidTiming", "remove")
    record("storm-vortex", "normal", "flip")
    for axis in ("invalidCost", "invalidTiming"):
        record("storm-vortex", axis, "flip-denied")
    record("wind-strike", "normal", "wind-strike")
    record("wind-strike", "invalidTiming", "wind-strike")
    for card_id, ref in (("sique-roast", "roast"), ("sique-blowfire", "blowfire"), ("sique-backfire", "backfire")):
        record(card_id, "normal", ref)
    record("sique-roast", "invalidTarget", "roast")
    record("sique-blowfire", "invalidCost", "blowfire")
    record("sique-blowfire", "invalidTiming", "blowfire")
    record("sique-backfire", "invalidTarget", "backfire")
    record("three-star two-star", "normal", "stars")
    record("moon", "normal", "moon")
    record("scorching-ray", "normal", "rays")
    record("scorching-ray", "invalidTarget", "rays")
    record("ice-storm", "normal", "ice")
    record("instant-circuit-overload instantly-sleep", "normal", "circuit-sleep")
    record("instantly-sleep", "invalidTiming", "circuit-sleep")
    record("sique-vinegar", "normal", "vinegar")
    record("sique-vinegar", "invalidTarget", "vinegar")
    record("sique-chili", "normal", "chili")
    record("unravel", "invalidTarget", "unravel-boundaries unravel-invalidated")
    record("sun", "normal", "sun")
    record("sun", "invalidTiming", "sun")
    record("messy-wave", "normal", "messy-wave")
    record("snow-rain-storm", "normal", "snow-rain")
    record("sique-woodpile", "normal", "woodpile")
    record("arcana-strength", "invalidTiming", "unique-strength")
    record("barbs counter-spell wind-strike sique-boiling-surge arcana-devil", "invalidCost", "response-cost")
    record("unravel", "normal", "unravel")

    record("sique-campfire", "normal", "253-campfire")
    record("elemental-ring", "normal", "253-ring")
    record("storm-crest", "normal", "253-crest")
    record("sique-cauldron", "normal", "253-cauldron")
    record("sique-cauldron storm-annihilation-crest", "invalidTarget", "253-runes")
    record("sique-refire sique-backfire", "normal", "253-selections")
    record("sique-flavor-shift", "normal", "253-flavor")
    record("sique-mushroom-soup sique-sugar", "normal", "253-soup-sugar")
    record("sique-mushroom-soup", "invalidCost", "253-soup-sugar")
    record("sique-ingredients", "normal", "253-ingredients")
    record("sique-divine-stew", "normal", "253-divine-stew")
    record("sique-divine-stew sique-ingredients", "invalidCost", "253-divine-stew")
    record("arcana-hanged-man", "normal", "253-hanged-man")
    record("arcana-magician", "normal", "253-magician")
    record("arcana-magician", "invalidTarget", "253-magician")
    record("disrupt", "normal", "253-word")
    for axis in ("invalidCost", "invalidTiming"):
        record("disrupt", axis, "253-word")
    conditional = {"barbs", "counter-spell", "wind-strike", "sique-boiling-surge", "arcana-devil"}
    for card in cards:
        card_id = card["id"]
        if card["type"] == "formation":
            record(card_id, "normalFlow", "pool-formations")
            record(card_id, "invalidTarget", "pool-formations")
            record(card_id, "invalidTiming", "pool-formations")
            if card["cost"] > 0 or card.get("recipe"):
                record(card_id, "invalidCost", "pool-formations")
        elif card["type"] == "seal":
            for axis in ("normalFlow", "invalidTarget", "invalidCost", "invalidTiming"):
                record(card_id, axis, "pool-seals")
        elif card_id in conditional:
            for axis in ("normalFlow", "normal", "invalidTarget", "invalidTiming"):
                record(card_id, axis, "pool-responses")
        else:
            for axis in ("normalFlow", "invalidTarget"):
                record(card_id, axis, "pool-spells")
            # Duplicate activation is checked only when the match did not end.
            if card_id != "arcana-death":
                record(card_id, "invalidTiming", "pool-spells")
            fee = card.get("castCost", 0) if card["type"] == "analytic" else card["cost"]
            # These two scenarios begin with Sugar attached, reducing fee to zero.
            if fee > 0 and card_id not in {"sique-pancakes", "sique-divine-stew"}:
                record(card_id, "invalidCost", "pool-spells")
            if (card.get("effects") and card["effects"][0]["kind"] in {"damage", "heal"}) or card_id in {
                "scorching-ray", "chromatic-orb", "false-life", "aid", "instant-circuit-overload",
                "sique-pancakes", "sique-divine-stew", "arcana-hierophant", "arcana-emperor", "arcana-priestess", "true-strike", "unravel", "arcana-tower"}:
                record(card_id, "normal", "pool-spells")

    # These are references to historical tests that mention the ID, not proof that
    # the assertion tests that card or that the latest card data was loaded.
    historical = {}
    for path in sorted((ROOT / "tests").glob("*.cpp")):
        source = path.read_text(encoding="utf-8")
        if "tests/fixtures/alpha-v251-production" not in source and "tests/fixtures/alpha-v1-production" not in source:
            continue
        starts = list(re.finditer(r'TEST_CASE\("([^"]+)', source))
        for index, match in enumerate(starts):
            end = starts[index + 1].start() if index + 1 < len(starts) else len(source)
            chunk = source[match.start():end]
            for card in cards:
                if '"' + card["id"] + '"' in chunk:
                    historical.setdefault(card["id"], []).append({"file": path.relative_to(ROOT).as_posix(), "line": source[:match.start()].count("\n") + 1})
    rows = []
    for card in sorted(cards, key=lambda value: value["number"]):
        axes = evidence[card["id"]]
        rows.append({"id": card["id"], "number": card["number"], "name": card["name"], "type": card["type"],
                     "currentTests": {axis: sorted(set(refs)) for axis, refs in axes.items()},
                     "missingAxes": [axis for axis, refs in axes.items() if not refs],
                     "historicalReadingLeads": historical.get(card["id"], [])[:2]})
    summary = {axis: sum(bool(row["currentTests"][axis]) for row in rows) for axis in ("normalFlow", "normal", "invalidTarget", "invalidCost", "invalidTiming")}
    payload = {"version": "2.5.3", "mechanicsRevision": 6, "status": "test-evidence-index-not-execution-result", "cards": len(rows),
               "summary": summary, "tests": tests, "rows": rows,
               "limits": ["Written test references do not establish execution success.", "Shared Arcana regression begins in Casting and does not test all setting costs or host restrictions.", "All 99 definitions have a generic forged-ownership rejection test; it is not a substitute for invalid target, cost or timing scenarios.", "Historical fixture references are reading leads only."]}
    (OUT / "alpha-v2.5.3-card-acceptance.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    lines = ["# Alpha v2.5.3 逐卡验收矩阵", "", "这是测试证据索引，记录已编写的断言，不代表测试已经运行通过，也不把 TODO 自动判为完成。运行结果须引用当前构建的 CTest 日志。旧版兼容测试加载冻结夹具，只作为阅读线索。", "", f"共{len(rows)}张正式卡牌。已有当前数据测试引用：正常源动作流程{summary['normalFlow']}张、具体效果断言{summary['normal']}张、非法目标{summary['invalidTarget']}张、费用/专属条件{summary['invalidCost']}张、非法时点/重复发动{summary['invalidTiming']}张。空白项仍需补充相应场景；所有99张共有的伪造拥有者拒绝测试不计入这些维度。", "", "源动作流程验证每个正式ID的设置/附着/发动、支付、强制选择可结束和状态不变量；具体效果列仅计入对该卡效果结果有直接断言的专项测试。阿卡那和普通法术共用回归从施法区开始，不能据此宣称已覆盖设置费用、宿主阶级、解析、反转或全部组合。", "", "| 编号/ID | 卡名 | 正常源动作 | 具体效果 | 非法目标 | 费用/条件 | 时点 | 历史线索 |", "|---|---|---|---|---|---|---|---|"]
    def refs(keys):
        return "、".join(f"[{key}](../../{tests[key]['file']}#L{tests[key]['line']})" for key in keys) or "待补"
    for row in rows:
        lead = "、".join(f"[{Path(e['file']).name}:{e['line']}](../../{e['file']}#L{e['line']})" for e in row["historicalReadingLeads"]) or "—"
        lines.append(f"| {row['number']} `{row['id']}` | {row['name']} | " + " | ".join(refs(row["currentTests"][axis]) for axis in ("normalFlow", "normal", "invalidTarget", "invalidCost", "invalidTiming")) + f" | {lead} |")
    lines += ["", "## 测试索引", "", "| 代号 | 当前测试名称 |", "|---|---|"]
    lines += [f"| {key} | [{entry['name']}](../../{entry['file']}#L{entry['line']}) |" for key, entry in tests.items()]
    lines += ["", "## 覆盖边界", "", "- 标记待补表示该维度尚无专项断言索引；继续增加当前卡池的正常结算、非法目标、费用不足/额外费用和错误时点场景时，应更新本表。", "- 高塔专项检查己方和对方不会被效果破坏的阵法；碎环术专项从手牌设置、解析至无目标准备，再验证发动与副卡组送灰结算。", "- 源动作流程、专项效果与所有效果组合是不同范围；当前索引不宣称穷举卡牌相互作用。冻结兼容夹具不替代当前数据测试。", "- 本轮实际命令、通过数量、失败数及日志路径记录在 [2.5.3 交付报告](2.5.3.md)。本表只记录断言位置，不根据引用推断执行通过。", "", "运行 `python tools/build_alpha_v253_acceptance_matrix.py` 刷新引用行号和机器可读 JSON。", ""]
    (OUT / "alpha-v2.5.3-card-acceptance.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"Acceptance evidence index: {len(rows)} cards; {summary}; test execution not inferred")


if __name__ == "__main__":
    main()
