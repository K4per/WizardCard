"""Recompute an analytical balance inventory from the shipped content.

Standard library only. This does not simulate matches or edit game assets.
Run: python tools/analyze_balance.py [--check]
Parameters: docs/balance-parameters.json
Outputs: docs/reports/card-balance.md and card-balance.json
"""
import argparse
import hashlib
import json
from math import ceil, comb
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPES = {"formation": "阵法", "analytic": "解析", "word": "言灵", "action": "行动", "seal": "符文"}
SEAL_LABELS = {"ringBonus": "环位", "incomeBonus": "收入", "capacityBonus": "容量", "rankBonus": "最高位阶", "castCostBonus": "施法费用", "damageBonus": "首次额外力场伤害", "refundSetting": "返还实际设置费"}

# Authored design hypotheses, deliberately separate from computed measurements.
# New definitions receive an explicit unreviewed label instead of an invented score.
REVIEWS = {
    "balance": ("基础参照", "10容量、2基础总收入；扩展实例免费，但占2阵体。"),
    "pentagram-of-life": ("基础参照", "7容量、3基础总收入；基础实例才减费，低容量是主要约束。"),
    "reservoir": ("高优先级观察", "免费、1阵体换5容量且免对手销毁；会削弱荷载负担的定价。"),
    "engeas-four-point-star": ("高优先级观察", "免费、1阵体、每轮+4收入并加速；不能选为基础。先测试设置费1或收入3。"),
    "conduit": ("高优先级观察", "1费换每轮+2收入，可附在受保护基础阵法；先测试费用2。"),
    "ring": ("组合观察", "返还实际设置费，不减少释放前峰值荷载；五芒星折扣会降低返还。"),
    "uplift": ("分模式观察", "阵法模式以每轮-1收入换容量/位阶；法术模式总增费2换2力场伤害。"),
    "recall": ("优先削弱", "1费抽2净赚1手牌，1临时荷载本回合到期；首轮候选费用2，费用3作上界对照。"),
    "arcane-recovery": ("高优先级观察", "0费产3，短期2荷载；与回想合用可净产2魔素且手牌净变化0。"),
    "true-strike": ("组合观察", "净手牌0；提高行动获取质量，必须与回想/复苏一起定价。"),
    "fireball": ("伤害上沿", "通常4费10伤；五芒星3费10伤。比闪电束少1费、多2伤，类型不同。"),
    "lightning-bolt": ("优先补偿", "通常5费8伤；火抗环境有优势。先测试伤害10或解析费用2，两者择一。"),
    "ray-of-frost": ("低费参照", "通常2费3伤，五芒星1费3伤；低启动费、低位阶、非火伤。"),
    "spark": ("即时参照", "1费2伤；无需阵法/解析等待，但消耗1张牌。"),
    "magic-missile": ("条件观察", "4费3伤加可选拆埋伏；空埋伏环境只剩低伤，快速释放有时机价值。"),
    "ward": ("优先补偿", "3费首次2伤；每次再付1魔素、累积1绑定荷载换1伤。先测试周期伤害2。"),
    "messy-wave": ("非线性风险", "8峰值绑定荷载；对手来源临时荷载5→8→11，持续3包括释放回合。"),
    "disrupt": ("非线性风险", "对手剩余容量小于4时直接过载；可同回合连续使用。"),
    "mend": ("永久负担观察", "至多治疗6，留下2独立荷载；当前卡池没有独立清除。"),
    "aid": ("防护参照", "2费立即解析获得5临时生命，仍须阵法、环位和释放时点。"),
    "false-life": ("永久负担观察", "3费8临时生命并留下2独立荷载；已有临时生命时只补差额。"),
    "shield": ("防护上沿", "2费6临时生命，可预置响应；不能从手牌响应，预置占荷载/后场位。"),
    "protective-flame": ("环境依赖", "首次2费，后续每次2费+2累积荷载；按每次火伤向下取整后减免。"),
    "lesser-restoration": ("场景依赖", "3费立即解析，清全部临时并治疗至多2；释放前先要容纳3绑定荷载。"),
    "clarity": ("场景依赖", "1费并另弃1手牌换清临时；已经过载败北后无法补救。"),
    "unravel": ("目标稀缺", "2费并另弃阵法手牌；只能空白扩展阵法，无法拆基础/受保护储容。"),
    "barbs": ("优先补偿", "5费取消准备，只耗掉对手本次施法费、延迟一轮；额外准备仍付费。先测试费用3或4。"),
    "counter-spell": ("反制参照", "4费无效并销毁；另承担目标实际施法费的临时荷载，不是总解析+施法费。"),
    "instantly-sleep": ("环境依赖", "4费且默认等一轮，只禁下一个对方回合的行动；不禁设置/解析/释放。"),
    "instant-circuit-overload": ("组合观察", "2费加实际解析费等量延迟临时荷载；四角星存在免费同类加速。"),
}


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def amount(card, kind, field="effects"):
    return sum(e.get("amount", 0) for e in card.get(field, []) if e["kind"] == kind)


def discounted_income(income, cost, card_price, horizon, discount):
    """Next-own-preparation income, at full utilization and survival."""
    return sum(income * discount ** t for t in range(1, horizon + 1)) - cost - card_price


def access_probability(total, copies, seen):
    return 1 - (comb(total - copies, seen) / comb(total, seen) if seen <= total - copies else 0)


def draw_surplus(draws, cost, load, card_price, load_price, extra_discard=0):
    return (draws - 1 - extra_discard) * card_price - cost - load * load_price


def card_row(card):
    kind = card["type"]
    cost = card.get("cost", 0)
    analytic = kind == "analytic"
    cast = card.get("castCost", 0) if analytic else 0
    discount = min(1, cost) if analytic and card.get("school") in ("evocation", "conjuration") else 0
    wait = card.get("analysisTurns", 1) if analytic else 0
    damage = amount(card, "damage")
    temp_self = amount(card, "temporary") if card.get("target") == "self" else 0
    row = {
        "id": card["id"], "name": card["name"], "type": kind,
        "cost": cost, "castCost": cast,
        "totalMana": cost + cast,
        "lifeBaseMana": cost + cast - discount,
        "lifeBaseDiscount": discount,
        "analysisTurns": wait, "rank": card.get("rank", 0), "speed": card.get("speed", 1),
        "boundLoadBeforeRelease": cost + cast if kind in ("analytic", "word") else 0,
        "lifeBaseBoundLoadBeforeRelease": cost + cast - discount if kind in ("analytic", "word") else 0,
        "firstDamage": damage, "damageTypes": sorted({e.get("damageType", "force") for e in card.get("effects", []) if e["kind"] == "damage"}),
        "healCeiling": amount(card, "heal"), "temporaryLifeCeiling": amount(card, "temporary_life"),
        "selfTemporaryLoad": temp_self, "independentLoad": amount(card, "independent"),
        "enemyTemporaryLoad": amount(card, "temporary") if card.get("target") == "opponent" else 0,
        "enemySourceLoad": amount(card, "source_temporary"),
        "draw": amount(card, "draw"), "manaGainCeiling": amount(card, "mana"),
        "extraDiscard": card.get("extraDiscard", 0) + sum(e["kind"] in ("discard_hand", "discard_formation") for e in card.get("effects", [])),
        "effects": card.get("effects", []), "periodicCastEffects": card.get("onCast", []),
        "responses": card.get("responses", []), "concentration": card.get("concentration", False),
        "duration": card.get("duration", 0), "assessment": REVIEWS.get(card["id"], ("未人工评估", "新增卡需按模型复核。")),
    }
    if kind == "formation":
        row.update({k: card.get(k, 0) for k in ("body", "capacity", "income", "rings", "maxRank")})
        row["baseEligible"] = card.get("baseEligible", False)
    if kind == "seal":
        row["sealModes"] = {k: card.get(k, 0) for k in ("ringBonus", "incomeBonus", "capacityBonus", "rankBonus", "castCostBonus", "damageBonus", "refundSetting")}
    return row


def build_model(catalog, presets, params):
    cards = {c["id"]: c for c in catalog["cards"]}
    rows = [card_row(c) for c in catalog["cards"]]
    draw_cards = [c for c in catalog["cards"] if amount(c, "draw")]
    draw_sensitivity = []
    for card in draw_cards:
        load = amount(card, "temporary") + card.get("burden", 0)
        for fee in params["drawCandidateCosts"]:
            prices = params["cardPriceRange"]
            loads = params["temporaryLoadPriceRange"]
            values = [draw_surplus(amount(card, "draw"), fee, load, h, l) for h in prices for l in loads]
            draw_sensitivity.append({"id": card["id"], "cost": fee, "centralSurplus": draw_surplus(amount(card, "draw"), fee, load, params["cardPriceMana"], params["temporaryLoadPriceMana"]), "range": [min(values), max(values)]})
    engines = []
    for c in catalog["cards"]:
        inc = c.get("income", 0) if c["type"] == "formation" else c.get("incomeBonus", 0) if c["type"] == "seal" else 0
        if inc > 0:
            engines.append({"id": c["id"], "income": inc, "cost": c.get("cost", 0), "body": c.get("body", 0), "fullUseManaPaybackEvents": ceil(c.get("cost", 0) / inc), "conditionalNetPresentMana": discounted_income(inc, c.get("cost", 0), params["cardPriceMana"], params["incomeHorizon"], params["futureDiscount"])})
    preset_rows = []
    for deck in presets:
        entries = deck["cards"]
        total = sum(x["count"] for x in entries)
        direct_draw = sum(x["count"] for x in entries if amount(cards[x["id"]], "draw"))
        tutors = sum(x["count"] for x in entries if any(e["kind"] == "search_action" for e in cards[x["id"]].get("effects", [])))
        base = cards[deck["baseFormation"]]
        preset_rows.append({"id": deck["id"], "name": deck["name"], "baseFormation": base["name"], "totalCards": total, "baseCapacity": catalog["baseLoadCapacity"] + base["capacity"], "baseIncome": 1 + base["income"], "directDrawCopies": direct_draw, "actionTutorCopies": tutors, "drawAccessOpening5": access_probability(total, direct_draw, 5), "drawOrTutorOpening5": access_probability(total, direct_draw + (tutors if direct_draw else 0), 5)})
    special = {}
    if "messy-wave" in cards:
        c = cards["messy-wave"]
        special["sourceLoadByOwnCast"] = [amount(c, "source_temporary") + n * amount(c, "source_temporary", "onCast") for n in range(c.get("duration", 0))]
    if "ward" in cards:
        c = cards["ward"]
        special["wardByMaintenances"] = [{"maintenanceCount": n, "mana": c["cost"] + c["castCost"] * (n + 1), "boundLoad": c["cost"] + c["castCost"] * (n + 1), "damage": amount(c, "damage") + n * amount(c, "damage", "onCast")} for n in range(4)]
    tracked = ["assets/cards.json", "assets/presets.json", "assets/deck.json", "docs/balance-parameters.json", "src/core/engine.cpp", "src/core/state.cpp", "src/content/content.cpp", "src/application/application.cpp"]
    return {"modelVersion": params["modelVersion"], "rulesVersion": catalog["rulesVersion"], "cardSetVersion": catalog["cardSetVersion"], "parameters": params, "inputSha256": {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in tracked}, "cards": rows, "drawSensitivity": draw_sensitivity, "incomeEngines": engines, "presets": preset_rows, "specialCases": special, "limitations": ["静态分析和人工设计假设，不是对局胜率预测。", "折现收入假定存活与完全利用，未扣阵体/环位机会成本。", "五芒星费用只在对应基础宿主上适用；费用表不表示目标、容量及时点均可满足。", "绑定峰值为无符文单卡释放前值；独立负担与反制/加速的动态负担另算。"]}


def fmt(n):
    return f"{n:.2f}"


def render(model):
    lines = ["# 当前卡池数学建模测算", "", "由 `python tools/analyze_balance.py` 生成；方法、推导和调参顺序见[数学模型](../balance-model.md)。", "", f"规则 `{model['rulesVersion']}`，卡池 `{model['cardSetVersion']}`，共{len(model['cards'])}种卡。所有‘观察/补偿/削弱’为人工假设；数值列为静态计算。没有运行胜率模拟。", "", "## 抽二的费用敏感性", "", "安全、牌库≥2、抽到的牌可用且无最终弃牌损耗：净手牌+1。净收益单位为模型中的魔素等价；范围来自参数先验，不是统计置信区间。", "", "| 卡 | 候选费用 | 中央净收益 | 先验范围 |", "|---|---:|---:|---:|"]
    names = {c["id"]: c["name"] for c in model["cards"]}
    for r in model["drawSensitivity"]:
        lines.append(f"| {names[r['id']]} | {r['cost']} | {fmt(r['centralSurplus'])} | {fmt(r['range'][0])}～{fmt(r['range'][1])} |")
    lines += ["", f"## {len(model['cards'])}张卡的资源与评估", "", "费用‘普通/五芒星’为首次总支出；五芒星只指在基础实例上解析。阵法费用为扩展设置，基础实例开局免费。解析等待以自己的回合计；四角星低阶加速另算。言灵等待0不代表可从手牌响应，仍须合法设置/释放时点。绑定峰值为释放前，瞬时离场会清理，不能把它与永久负担相加定价。", ""]
    for kind, label in TYPES.items():
        lines += [f"### {label}", "", "| 卡牌 | 首次费用 普通/五芒星 | 等待 | 绑定峰值 普通/五芒星 | 数值收益/约束 | 评估与候选 |", "|---|---:|---:|---:|---|---|"]
        for c in model["cards"]:
            if c["type"] != kind:
                continue
            parts = []
            if kind == "formation":
                parts.append(f"阵体{c['body']}；容量{c['capacity']}；收入{c['income']}；环位{c['rings']}；最高阶{c['maxRank']}")
            if kind == "seal":
                parts.append(" / ".join(SEAL_LABELS[k] if k == "refundSetting" else f"{SEAL_LABELS[k]}{v:+d}" for k, v in c["sealModes"].items() if v))
            for key, text in [("firstDamage", "首次伤害"), ("healCeiling", "治疗上限"), ("temporaryLifeCeiling", "临时生命上限"), ("draw", "抽牌"), ("manaGainCeiling", "产魔素上限"), ("selfTemporaryLoad", "自身本回合临时"), ("enemyTemporaryLoad", "对手临时"), ("enemySourceLoad", "对手来源临时"), ("independentLoad", "独立荷载"), ("extraDiscard", "另弃牌")]:
                if c[key]:
                    parts.append(f"{text}{c[key]}")
            if c["concentration"]:
                parts.append("专注维持累积荷载")
            if c["duration"]:
                parts.append(f"持续{c['duration']}（含首次）")
            lines.append(f"| {c['name']} | {c['totalMana']}/{c['lifeBaseMana']} | {c['analysisTurns']} | {c['boundLoadBeforeRelease']}/{c['lifeBaseBoundLoadBeforeRelease']} | {'；'.join(parts) or '条件效果，见右列'} | **{c['assessment'][0]}**：{c['assessment'][1]} |")
    lines += ["", "## 产费结构", "", f"以未来{model['parameters']['incomeHorizon']}次自己的准备阶段、每期折现{model['parameters']['futureDiscount']}计算；减去设置费及1张牌的先验价格。假定满额利用、宿主存活，未扣阵体机会成本；不是无条件强度评分。基础免费实例的开局价值不在此表中。", "", "| 扩展阵法/阵法符文 | 设置费 | 每轮新增收入 | 阵体 | 回本需后续收入次数 | 条件净现值 |", "|---|---:|---:|---:|---:|---:|"]
    for e in model["incomeEngines"]:
        lines.append(f"| {names[e['id']]} | {e['cost']} | {e['income']} | {e['body']} | {e['fullUseManaPaybackEvents']} | {fmt(e['conditionalNetPresentMana'])} |")
    lines += ["", "## 持续与专注", "", "紊乱波动：对手来源临时荷载序列 `" + " → ".join(map(str, model["specialCases"].get("sourceLoadByOwnCast", []))) + "`；对手不清除、来源不提前离场时成立。", "", "| 炽燃结界维持次数 | 累计费用 | 绑定荷载 | 累计未减免伤害 |", "|---:|---:|---:|---:|"]
    for r in model["specialCases"].get("wardByMaintenances", []):
        lines.append(f"| {r['maintenanceCount']} | {r['mana']} | {r['boundLoad']} | {r['damage']} |")
    lines += ["", "## 现有预设的资源与抽牌入口", "", "起手5张，精确超几何计算；‘抽牌或检索’只表示看到至少一个入口，还要支付费用并满足时机、牌库候选及荷载。忽略先后手第一次自然抽牌；不是最终抽到核心牌的概率。", "", "| 预设 | 基础阵法 | 开局容量 | 每轮基础收入 | 抽牌卡份数 | 行动检索份数 | 起手抽牌入口 | 起手抽牌/检索入口 |", "|---|---|---:|---:|---:|---:|---:|---:|"]
    for d in model["presets"]:
        lines.append(f"| {d['name']} | {d['baseFormation']} | {d['baseCapacity']} | {d['baseIncome']} | {d['directDrawCopies']} | {d['actionTutorCopies']} | {d['drawAccessOpening5']:.1%} | {d['drawOrTutorOpening5']:.1%} |")
    lines += ["", "## 数据来源与使用边界", "", "完整结构化指标、输入SHA-256及参数见 [card-balance.json](card-balance.json)。源卡牌数值改变后重新运行脚本。规则实现改变时先复核模型假设，再更新报告；哈希只用于定位来源，不自动证明规则兼容。", ""]
    lines.extend(f"- {x}" for x in model["limitations"])
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="Check committed reports match recomputation without writing.")
    args = parser.parse_args()
    catalog = read_json(ROOT / "assets/cards.json")
    presets = read_json(ROOT / "assets/presets.json")
    # Match loadPresets in src/application/application.cpp, including the default deck.
    presets = [{"id": "default", "name": "示范卡组", "baseFormation": "balance", "cards": read_json(ROOT / "assets/deck.json")}] + presets
    params = read_json(ROOT / "docs/balance-parameters.json")
    model = build_model(catalog, presets, params)
    outputs = {"docs/reports/card-balance.json": json.dumps(model, ensure_ascii=False, indent=2) + "\n", "docs/reports/card-balance.md": render(model)}
    for name, content in outputs.items():
        path = ROOT / name
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                raise SystemExit(f"Outdated balance report: {name}; rerun python tools/analyze_balance.py")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
    print(f"{'Verified' if args.check else 'Generated'} balance inventory: {len(model['cards'])} cards, {len(model['presets'])} presets.")


if __name__ == "__main__":
    main()
