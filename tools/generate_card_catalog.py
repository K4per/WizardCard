"""Generate current player documentation from canonical cards and legal deck lists."""
from collections import Counter
from pathlib import Path
import hashlib
import html
import json
import re
from catalog_source import load_catalog

ROOT = Path(__file__).resolve().parents[1]
TYPES = {"formation": "阵法", "analytic": "解析法术", "word": "言灵法术", "talisman": "咒符法术", "seal": "符文"}
SCHOOLS = {"evocation": "塑能", "transmutation": "变化", "conjuration": "咒法", "enchantment": "惑控", "illusion": "幻术", "abjuration": "防护", "necromancy": "死灵", "divination": "预言"}
PREFIXES = {"formation": "01", "analytic": "02", "word": "03", "talisman": "04", "seal": "05"}

def load(path):
    return json.loads((ROOT / path).read_text(encoding="utf-8-sig"))

def attributes(card):
    pairs = [("编号", card["number"]), ("类型", TYPES[card["type"]])]
    if card["type"] == "formation":
        pairs += [("等级", card["level"]), ("阵体", card["body"]), ("荷载上限", card["capacity"]), ("魔素收入", card["income"]), ("环位", card["rings"]), ("承载位阶", card["maxRank"]), ("召唤条件", card.get("summonCondition", "—"))]
    elif card["type"] == "seal":
        pairs += [("附着费用", card["cost"])]
    else:
        pairs += [("学派", SCHOOLS.get(card.get("school"), "无")), ("位阶", card["rank"]), ("速度", card.get("speed", 1))]
        if card["type"] == "analytic":
            pairs += [("设置费用", card["cost"]), ("施法费用", card.get("castCost", 0)), ("解析回合", card.get("analysisTurns", 1))]
        else:
            if card["type"] == "talisman":
                pairs.append(("盖伏费用", 0))
            pairs.append(("施法费用", card["cost"]))
        pairs.append(("持续方式", "专注" if card.get("concentration") else "永续" if card.get("perpetual") else "计时 " + str(card["duration"]) if card.get("duration") else "通常"))
    return pairs

def validate_deck(rows, size, formation, by_id):
    assert sum(row["count"] for row in rows) == size
    counts = Counter()
    for row in rows:
        card = by_id[row["id"]]
        assert (card["type"] == "formation") == formation
        assert isinstance(row["count"], int) and row["count"] > 0
        counts[card["name"]] += row["count"]
    assert max(counts.values()) <= 3

def main():
    catalog = load_catalog()
    cards = sorted(catalog["cards"], key=lambda card: card["number"])
    by_id = {card["id"]: card for card in cards}
    assert len(by_id) == len(cards)
    assert len({card["number"] for card in cards}) == len(cards)
    art = {entry["id"]: entry["file"] for entry in load("godot/art/assets.json")["assets"]}
    side = load("assets/side-deck.json")
    presets = [{"id": "default", "name": "综合示范", "description": "练习解析、言灵、咒符、符文与阵法配方。", "baseFormation": side["initialFormation"], "cards": load("assets/deck.json"), "sideCards": side["cards"]}] + load("assets/presets.json")
    for preset in presets:
        validate_deck(preset["cards"], 40, False, by_id)
        validate_deck(preset["sideCards"], 10, True, by_id)
        initial = by_id[preset["baseFormation"]]
        assert initial["level"] == 1 and any(row["id"] == initial["id"] for row in preset["sideCards"])
    version = catalog["cardSetVersion"]
    intro = f"规则 {catalog['rulesVersion']} · 卡池 {version} · {len(cards)} 种正式卡牌 · {len(presets)} 套 40+10 预设。"
    md = ["# 巫师牌卡池图鉴", "", intro, "", "本页由正式定义生成。规则见 [Alpha v2.5](alpha-v2.5-rules.md)，卡面召唤条件由正式字段生成，效果以正式规则为准。", "", "编号前两位：01 阵法、02 解析、03 言灵、04 咒符、05 符文；后四位为该类序号。稳定内容 ID 用于存档和资源关联。", "", "| 编号 | 卡名 | 类型 |", "|---|---|---|"]
    sections = []
    for card in cards:
        assert re.fullmatch(r"[0-9]{6}", card["number"])
        assert card["number"].startswith(PREFIXES[card["type"]])
        md.append(f"| {card['number']} | [{card['name']}](#{card['id']}) | {TYPES[card['type']]} |")
    for card in cards:
        art_id = "illustration." + card.get("illustrationId", card["id"])
        image = "../godot/" + art[art_id].removeprefix("res://")
        assert (ROOT / "docs" / image).is_file(), image
        pairs = attributes(card)
        md += ["", f'<a id="{card["id"]}"></a>', "", f"## {card['number']} · {card['name']}", "", f"![{card['name']}]({image})", "", "| 属性 | 数值 |", "|---|---|"]
        md += [f"| {key} | {value} |" for key, value in pairs]
        md += ["", card["text"], "", card.get("flavor", ""), "", f"内容 ID：`{card['id']}`。"]
        escape = html.escape
        attrs = "".join(f"<dt>{escape(str(key))}</dt><dd>{escape(str(value))}</dd>" for key, value in pairs)
        sections.append(f'<article data-search="{escape(card["number"] + " " + card["name"] + " " + card["text"] + " " + TYPES[card["type"]])}"><img loading="lazy" src="{escape(image)}" alt=""><h2>{card["number"]} · {escape(card["name"])}</h2><p>{escape(card["text"])}</p><details><summary>属性与风味</summary><dl>{attrs}</dl><p>{escape(card.get("flavor", ""))}</p></details></article>')
    md += ["", "## 预设牌表", "", "初始阵法来自副卡组；主卡组与副卡组分别限制同名最多三张。"]
    for preset in presets:
        md += ["", f"### {preset['name']}", "", preset.get("description", ""), "", f"初始阵法：{by_id[preset['baseFormation']]['name']}。"]
        for key, label in (("cards", "主卡组 · 40 张"), ("sideCards", "副卡组 · 10 张")):
            md += ["", label, "", "| 卡牌 | 份数 |", "|---|---|"]
            md += [f"| {by_id[row['id']]['name']} | {row['count']} |" for row in preset[key]]
    digest = hashlib.sha256(json.dumps(catalog, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    md += ["", "## 生成依据", "", "运行 `python tools/generate_card_catalog.py` 更新。", "", f"规范化 JSON SHA256：`{digest}`。", ""]
    (ROOT / "docs/card-catalog.md").write_text("\n".join(md), encoding="utf-8")
    page = '<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>巫师牌卡池图鉴</title><style>body{margin:32px auto;max-width:1400px;padding:0 24px;background:#111923;color:#e8e1cf;font:16px/1.7 sans-serif}a{color:#9ed8cc}input{box-sizing:border-box;width:100%;padding:14px;font:inherit;margin-bottom:24px}main{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:20px}article{background:#1c2935;border:1px solid #53615b;padding:20px}article[hidden]{display:none}img{width:100%;height:220px;object-fit:contain}h2{font-size:21px}dl{display:grid;grid-template-columns:1fr 1fr}dd{margin:0}summary{cursor:pointer}footer{margin-top:30px}</style><h1>巫师牌卡池图鉴</h1><p>' + html.escape(intro) + '</p><p><a href="alpha-v2.5-rules.md">规则</a> · <a href="card-catalog.md">完整牌表与文字版</a></p><input id="search" type="search" aria-label="搜索卡名、编号、类型或效果" placeholder="搜索卡名、编号、类型或效果"><main>' + "".join(sections) + '</main><footer>数据来自正式卡池；通常法术效果处理后留在施法区，至结束阶段清理。</footer><script>document.getElementById("search").addEventListener("input",event=>{const q=event.target.value.trim().toLowerCase();document.querySelectorAll("article").forEach(card=>card.hidden=!card.dataset.search.toLowerCase().includes(q))});</script></html>'
    (ROOT / "docs/card-catalog.html").write_text(page, encoding="utf-8")
    print(f"Generated catalogue: {len(cards)} cards, {len(presets)} validated 40+10 decks, all images present.")

if __name__ == "__main__":
    main()
