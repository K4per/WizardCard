"""Generate review-only Alpha v2 card proposals; never change shipped content."""
from pathlib import Path
from collections import Counter
import json

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs' / 'proposals' / 'alpha-v2'
RULE_SPEC = 'alpha-v2-final-2026-10-10'
FINAL_REPORT_SHA256 = 'f3c38d92d594c7556190a6369a755931cb3498bac8950daa0022e6d9ad37e1fe'
FORMATION = {
    'balance': (1, [{'kind': 'mana', 'amount': 2}]),
    'pentagram-of-life': (1, [{'kind': 'mana', 'amount': 2}]),
    'reservoir': (2, [{'kind': 'ready_spell', 'count': 1}]),
    'engeas-four-point-star': (3, [{'kind': 'mana', 'amount': 3}, {'kind': 'ready_spell', 'count': 1}]),
}
NOTES = {
    'balance': '承载上限由旧2阶改为等级1；免费初始设置，后续按配方。',
    'pentagram-of-life': '初始等级1；原基础塑能/咒法减费改为本阵法承载时生效，避免新规则无基础保护标志后语义漂移。',
    'reservoir': '保留对手销毁保护及无环位；2级阵法不能作为初始阵法。配方要求一张已解析完成的己方法术进入灰烬。',
    'engeas-four-point-star': '保留每己方回合第一张1阶解析加速；设置配方含已解析完成法术，取消原免费设置。',
    'unravel': '改为销毁对手空白阵法；额外弃置从手牌阵法改为副卡组阵法实体进入灰烬。可选择初始阵法，仍遵守卡面保护。',
    'instantly-sleep': '旧行动封锁改为禁止对方下一己方回合主动发动言灵；是否同时封锁咒符须审定，草案不封锁咒符响应。',
    'magic-missile': '保留对方施法阶段从已解析完成解析区响应的特例；需明确卡面授予快速进入施法区且支付施法费用，普通解析不继承特例。',
    'true-strike': '由0阶改为1阶；检索普通/罕见且带 former-action 标签的言灵，避免把检索范围扩大到全部言灵。',
    'spark': '由0阶改为1阶；按言灵流程立即解析、准备至施法区并发动，普通法术结算后留施法区至结束。',
    'counter-spell': '草案迁为咒符；保留4速及取消敌方法术链节，临时荷载取目标链节实际施法费用。',
    'barbs': '草案迁为咒符；保留准备宣告取消及可选额外准备的提案，额外准备依被准备卡的速度、类型、来源和阶段共同校验，不能无条件授予阶段外权限。',
    'shield': '草案迁为咒符；主动发动权限需卡面明确，临时生命在己方准备阶段第2步清除。',
    'aid': '临时生命取较高值，在己方准备阶段第2步清除；立即解析仍需支付并占环位。',
    'false-life': '临时生命在己方准备阶段第2步清除，保留独立荷载2。',
    'conduit': '阵法收入+2移至抽牌阶段，符文自身不另外触发重复收入。',
    'ward': '收入在抽牌阶段；准备阶段先清临时荷载和临时生命，再依次处理阵法、永续、专注维持；施法阶段结束前重新选择环位回位。',
    'messy-wave': '计时3在每个全局结束阶段递减，回位不预留旧环位；来源离场清除关联荷载。',
    'protective-flame': '专注言灵同样在施法阶段结束前回位，维持在己方准备处理；离场失去抗性。',
    'uplift': '阵法承载修正改为等级导出的上限+1，允许至7阶；收入最低0。',
}

def main():
    source = json.loads((ROOT / 'assets/cards.json').read_text(encoding='utf-8-sig'))
    proposals = []
    for card in source['cards']:
        kind = 'word' if card['type'] == 'action' else card['type']
        if card['id'] in ('counter-spell', 'barbs', 'shield'):
            kind = 'talisman'
        row = {
            'id': card['id'], 'name': card['name'], 'oldType': card['type'],
            'proposedType': kind, 'reviewStatus': 'pending',
            'cost': card.get('cost', 0), 'castCost': card.get('castCost', 0),
            'rank': max(1, card.get('rank', 1)) if kind != 'formation' else None,
            'speed': max(2, card.get('speed', 1)) if kind in ('word', 'talisman') else card.get('speed', 1),
            'durationMode': 'concentration' if card.get('concentration') else 'timer' if card.get('duration') else 'instant',
            'notes': NOTES.get(card['id'], '保留原效果数值；应用新版区域、荷载计伤、发动时点及清理规则。'),
        }
        if row['speed'] != card.get('speed', 1):
            row['previousSpeed'] = card.get('speed', 1)
            row['notes'] += ' 最终规则默认言灵/咒符至少2速，本候选由1速调整为2速，仍待逐卡审定。'
        if kind == 'formation':
            row['level'], row['recipe'] = FORMATION[card['id']]
        if card['type'] == 'action':
            row['tags'] = [*card.get('tags', []), 'former-action']
        proposals.append(row)
    assert len(proposals) == 30 and len({x['id'] for x in proposals}) == 30
    catalog = {x['id']: x for x in proposals}
    old_presets = json.loads((ROOT / 'assets/presets.json').read_text(encoding='utf-8-sig'))
    old_presets.append({'id': 'default', 'name': '默认预设', 'baseFormation': 'balance',
                        'cards': json.loads((ROOT / 'assets/deck.json').read_text(encoding='utf-8-sig'))})
    presets = []
    for preset in old_presets:
        counts = Counter({x['id']: x['count'] for x in preset['cards'] if catalog[x['id']]['proposedType'] != 'formation'})
        # Explicit proposals only; no automatic upgrade of any user's saved deck.
        priority = list(counts) + ['arcane-recovery', 'recall', 'ring', 'conduit', 'clarity'] + list(catalog)
        while sum(counts.values()) < 40:
            for card_id in dict.fromkeys(priority):
                if catalog[card_id]['proposedType'] != 'formation' and counts[card_id] < 3:
                    counts[card_id] += 1
                    if sum(counts.values()) == 40:
                        break
        assert sum(counts.values()) == 40 and max(counts.values()) <= 3
        presets.append({'id': preset['id'], 'name': preset['name'], 'reviewStatus': 'pending',
                        'initialFormation': preset['baseFormation'],
                        'main': [{'id': k, 'count': v} for k, v in counts.items()],
                        'side': [{'id': 'balance', 'count': 3}, {'id': 'pentagram-of-life', 'count': 3},
                                 {'id': 'reservoir', 'count': 2}, {'id': 'engeas-four-point-star', 'count': 2}]})
    OUT.mkdir(parents=True, exist_ok=True)
    for name, payload in [('migration.json', {'status': 'review-only', 'sourceRules': source['rulesVersion'],
                                             'ruleSpec': RULE_SPEC, 'finalReportSha256': FINAL_REPORT_SHA256, 'cards': proposals}),
                          ('presets.json', {'status': 'review-only', 'format': 2, 'ruleSpec': RULE_SPEC, 'presets': presets})]:
        (OUT / name).write_text(json.dumps(payload, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    lines = ['# Alpha v2.0 卡牌迁移审定草案', '',
             '以下是具体提案，全部等待组内审定，不进入正式卡池。费用与能力未列出的部分沿用原数值。阵法配方中的 ready_spell 表示将己方解析区已完成解析的法术实体移入灰烬；支付失败或取消必须原子回滚。', '',
             f'依据最终规则规格 `{RULE_SPEC}`。本轮只同步规则确认导致的速度/到期/阶段说明，不代表30卡或9份预设已批准。言灵和咒符默认至少2速；7张原1速言灵候选调整为2速，原值记在 previousSpeed。临时生命改在己方准备第2步清除；持续法术保留回位例外。', '',
             '主要审定点：言灵/咒符边界、阵法等级和配方、基础减费迁移、魔法飞弹阶段外快速施法、银光锐语阶段外准备限制、睡眠封锁范围。预设只是构筑候选，尚未做新规则平衡验证。', '',
             '| 稳定ID / 卡名 | 类型迁移 | 等级/阶 | 速度 | 配方或费用 | 持续 | 能力处理 |',
             '|---|---|---|---|---|---|---|']
    for row in proposals:
        recipe = json.dumps(row['recipe'], ensure_ascii=False) if 'recipe' in row else f"{row['cost']} / 施法{row['castCost']}"
        speed = f"{row['previousSpeed']}→{row['speed']}（候选）" if 'previousSpeed' in row else str(row['speed'])
        lines.append(f"| {row['id']} / {row['name']} | {row['oldType']} → {row['proposedType']} | {row.get('level', row['rank'])} | {speed} | {recipe} | {row['durationMode']} | {row['notes']} |")
    lines += ['', '## 40+10 候选预设', '', '副卡组统一：均衡×3、生命×3、阿克乌姆×2、恩格亚斯×2；每份均含合法的1级初始选择。同名限制分别检查主副卡组。']
    for preset in presets:
        lines += ['', f"### {preset['name']}（{preset['id']}）", '',
                  f"初始：{catalog[preset['initialFormation']]['name']}；主卡组40张：" + '、'.join(f"{catalog[x['id']]['name']}×{x['count']}" for x in preset['main']) + '。']
    (OUT / 'review.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'{len(proposals)} card proposals; {len(presets)} validated 40+10 draft presets; shipped files untouched')

if __name__ == '__main__':
    main()
