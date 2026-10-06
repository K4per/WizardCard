"""Generate the player-facing Alpha card catalogue from the shipped definitions.

Run from any directory: python tools/generate_card_catalog.py
No dependencies; generated files are committed alongside the game content.
"""
from pathlib import Path
from collections import Counter
import hashlib
import html
import json

ROOT = Path(__file__).resolve().parents[1]
TYPES = {'formation': '阵法卡', 'analytic': '解析法术', 'word': '言灵法术',
         'action': '行动卡', 'seal': '符文卡'}
RARITIES = {'common': '普通', 'uncommon': '罕见', 'rare': '稀有',
            'epic': '史诗', 'legendary': '传说'}
SCHOOLS = {'evocation': '塑能系', 'transmutation': '变化系', 'conjuration': '咒法系',
           'enchantment': '惑控系', 'illusion': '幻术系', 'abjuration': '防护系',
           'necromancy': '死灵系', 'divination': '预言系'}
WINDOWS = {'phase_start': '阶段开始', 'phase_end': '阶段结束', 'action': '行动宣告',
           'prepare': '准备宣告', 'cast': '释放宣告', 'trigger': '被动触发'}
SOURCES = {'words': '已设置的言灵', 'ambush': '可反转的埋伏卡', 'analysis': '已解析完成的解析法术'}
TARGETS = {'opponent': '对方玩家', 'self': '自己', 'empty_enemy_formation': '对方空白扩展阵法',
           'own_analyzing_spell': '自己的解析中法术'}
NOTES = {
    'balance': ('均衡基础', '基础开局免费入场，总荷载上限8+2=10。占2阵体，适合多数二阶以下法术。'),
    'reservoir': ('扩容支援', '不能承载解析法术，主要增加荷载上限；对手的销毁效果不能销毁它，自己仍可按合法操作处理。'),
    'pentagram-of-life': ('塑能基础', '基础开局免费入场，总荷载上限5+2=7。减费仅适用于作为基础阵法的实例及其承载的塑能/咒法解析；作为扩展阵法无此折扣。'),
    'engeas-four-point-star': ('快速解析', '不能作为基础阵法。每个自己的回合自动加速第一张符合条件的解析法术；本来就立即解析的法术不消耗这次加速。'),
    'fireball': ('高伤终结', '销毁自己的空白扩展阵法是可选后续效果，可选择跳过；基础阵法不可选。失去阵法容量可能造成过载，应提前留出荷载空间。'),
    'mend': ('恢复生命', '治疗上限40。2点独立荷载不会因这张瞬时法术离场而消失，也不能由心如止水或次级复原清除。'),
    'unravel': ('阵法干扰', '需要结算时另弃置一张手中的阵法卡；仅有有效目标还不够。基础阵法及被对手销毁保护的阿克乌姆之弧不能被它销毁。'),
    'ward': ('持续伤害', '首次释放只造成2火焰伤害，后续自己的施法阶段触发1伤害；自己的准备阶段需先维持专注，不能借尚未到账的收入支付维持。'),
    'messy-wave': ('来源荷载', '附加荷载不按普通回合期限清除；来源销毁或临时清除效果可以处理。持续计数在自己的结束阶段递减，3是持续计数，不是额外三次触发。'),
    'instantly-sleep': ('行动封锁', '只封锁对方下一个自己的回合内使用行动卡，包括反转使用行动；不禁止解析、设置、符文或所有其他操作。'),
    'magic-missile': ('快速施法', '对方施法阶段有来源的合法响应时点才可快速释放，仍受2速限制。需已解析完成并支付施法费用；销毁埋伏为可选后续选择。'),
    'aid': ('立即防护', '立即解析仍需阵法环位、支付解析费用，并另行准备或直接释放；5点临时生命取较高值，不能叠加。'),
    'ray-of-frost': ('低费输出', '提供非火焰伤害；生命之五芒星作为基础阵法并承载本卡时，基础解析费用可减至0。'),
    'lightning-bolt': ('异色终结', '闪电伤害不受避火咒的火焰抗性减半；施法费用高于火球术，准备前应检查魔素与荷载。'),
    'lesser-restoration': ('恢复与减压', '只清除临时类荷载，包括来源关联临时荷载；不清除解析/施法绑定荷载与独立荷载。治疗不会补回临时生命。'),
    'false-life': ('高额防护', '临时生命取较高值且无自然到期，但重复使用会继续产生独立荷载；防护收益不能无限叠加。'),
    'barbs': ('准备反制', '只能在对手准备宣告窗口响应，取消准备不等于取消实际释放。后续额外准备仍需费用、就绪法术及荷载空间。'),
    'spark': ('轻量直伤', '可只设置，也可选择设置并释放；拖到后场默认只设置。只设置需后场空位，零阶设置并释放可不长期占位。'),
    'counter-spell': ('关键反制', '仅针对对手待释放的法术链节；不能取消普通行动或准备宣告。承担的是该链节实际施法费用的快照，四速后禁止继续响应。'),
    'true-strike': ('行动检索', '可只设置或设置并释放。检索范围为普通/罕见行动卡；无合格候选自动继续，检索后牌库洗切。'),
    'protective-flame': ('火焰抗性', '成功释放后才获得抗性。每位玩家只能维持一个专注；替换、放弃或销毁专注后失去抗性。抗性减半向下取整，多个相同抗性不叠加。'),
    'shield': ('响应防护', '不是专注法术。可先设置再响应，也可合法主动释放；临时生命6与已有临时生命取较高值，不相加。'),
    'recall': ('补充手牌', '先抽牌，再获得1临时荷载；该荷载在自己的本回合结束清除。行动卡没有每回合使用次数上限，但仍受费用与封锁约束。'),
    'disrupt': ('荷载施压', '荷载由对手承担，到对手下一次自己的结束阶段清除；和对手已有绑定荷载配合会形成过载压力。'),
    'clarity': ('临时减压', '效果结算时需要另弃置一张手牌；不会清除独立或绑定荷载。对来源荷载的清除不销毁来源，之后仍可再产生。'),
    'instant-circuit-overload': ('加速解析', '目标必须仍在解析中；形成的临时荷载按目标实际解析支付计算，受基础阵法减费影响，到自己下一个结束阶段清除。'),
    'arcane-recovery': ('资源转换', '魔素最多储存12，满魔素时仍会产生2临时荷载；按本回合结束清除，使用前评估收益。'),
    'ring': ('扩环与返还', '同一宿主最多附一个符文。返还的是解析/设置时实际支付的魔素，不含准备和专注维持费用；仅成功首次释放返还一次。'),
    'conduit': ('收入支援', '收入加成只在附着阵法时生效；附在法术上没有额外效果。总准备收入仍按场上阵法计算。'),
    'uplift': ('阵法与法术强化', '阵法路线增加容量/承载位阶，代价为收入减少；法术路线增加施法费用，首次成功释放另加2力场伤害。周期触发不会重复这次额外伤害。'),
}

RULES = [
    '初始生命与恢复上限均为40；临时生命之核独立计算，取较高值、不叠加、无自然到期，伤害先扣临时生命。',
    '主卡组30张、同名最多3张；基础阵法不占主卡组，在编辑器内选择。玩家自带基础荷载上限2，均衡开局10、五芒星开局7。',
    '费用表为未修正的魔素费用；解析/设置言灵及施法支付形成绑定荷载，卡牌离场按规则清理。费用修正、弃置、临时及独立荷载另按效果执行。',
    '未明确解析时间的解析法术默认为1个自己的回合：经过下一个自己的准备阶段完成；立即解析并不代表免费或自动释放。',
    '言灵的设置支付已包含基础释放费用，正常释放不重复支付；符文增加的费用等追加成本仍需支付。只有纯响应言灵必须等待其指定时点。',
    '施法阶段可选择不再释放，准备好的法术留在施法区，已付费用和绑定荷载保留，后续自己的施法阶段可继续释放。',
    '连锁只在规定窗口建立，非任意时刻；速度与来源、阶段、事件归属、目标须同时合法。手牌不能直接响应；四速后不能追加响应。',
    '专注共用每位玩家一个名额；自己的准备阶段先支付维持费用及等量新增绑定荷载，再获取收入。维持费用不足或容量不足可放弃，专注来源及关联状态清理。',
    '行动/言灵可埋伏在共用的3个后场位，解析法术埋伏在合法阵法环位；埋伏支付1魔素、无绑定荷载，当个全局回合不可反转。反转支付正常费用。',
    '构筑定位与使用要点是编辑说明，不修改卡面；精确时点以核心合法操作及连锁规范为准。',
]

def load(path):
    return json.loads((ROOT / path).read_text(encoding='utf-8-sig'))

def properties(c):
    t = c['type']
    p = [('类型', TYPES[t]), ('稀有度', RARITIES[c['rarity']])]
    if t in ('analytic', 'word'):
        p += [('学派', SCHOOLS[c['school']]), ('位阶', str(c['rank'])), ('咒文速度', f"{c.get('speed',1)}速")]
    p += [('费用', cost(c))]
    if t == 'analytic':
        p += [('解析时间', '立即解析' if c.get('analysisTurns',1) == 0 else f"{c.get('analysisTurns',1)}个自己的回合")]
    if t == 'formation':
        p += [('阵体',str(c['body'])),('荷载上限',str(c['capacity'])),('魔素收入',str(c['income'])),
              ('环位数',str(c['rings'])),('承载位阶上限',str(c['maxRank'])),
              ('基础阵法资格','有' if c.get('baseEligible') else '无')]
    if c.get('target'):
        p += [('效果目标',TARGETS[c['target']])]
    if c.get('concentration'):
        p += [('持续方式','专注')]
    elif c.get('duration'):
        p += [('持续计数',str(c['duration']))]
    if c.get('responseOnly'):
        p += [('释放限制','仅响应')]
    return p

def cost(c):
    t=c['type']; n=c.get('cost',0)
    if t == 'analytic': return f"解析 {n} / 施法 {c.get('castCost',0)}"
    return f"{'附着' if t=='seal' else '使用' if t=='action' else '设置'} {n}"

def response(c):
    out=[]
    for r in c.get('responses',[]):
        chunks=[r.get('name',r['id']), '来源：'+'、'.join(SOURCES[s] for s in r['sources']),
                '窗口：'+'、'.join(WINDOWS[w] for w in r['windows'])]
        if r.get('phases'):
            chunks.append('阶段：'+'、'.join({'cast':'施法'}[p] for p in r['phases']))
        if r.get('eventOwner')=='opponent': chunks.append('事件归属：对方')
        if r.get('requiresSource'): chunks.append('窗口须有来源卡')
        out.append('；'.join(chunks)+'。')
    return out

def main():
    catalog=load('assets/cards.json'); cards=catalog['cards']; mapping=load('assets/art/runtime.json')['cards']
    presets=[{'id':'default','name':'示范卡组','baseFormation':'balance','cards':load('assets/deck.json'),
              'description':'原始综合示范：练习解析、言灵、符文、阵法和荷载管理。'}]+load('assets/presets.json')
    by_id={c['id']:c for c in cards}
    assert len(by_id)==30 and by_id.keys()==NOTES.keys()
    assert len(presets)==9
    for c in cards: assert (ROOT/'assets/art'/mapping[c['id']]).is_file()
    for p in presets:
        assert sum(x['count'] for x in p['cards'])==30
        assert all(x['id'] in by_id and 1<=x['count']<=3 for x in p['cards'])
        assert len({x['id'] for x in p['cards']})==len(p['cards'])
        assert by_id[p['baseFormation']].get('baseEligible')
    counts=Counter(c['type'] for c in cards)
    ordered=[c for t in TYPES for c in cards if c['type']==t]
    digest=hashlib.sha256((ROOT/'assets/cards.json').read_bytes()).hexdigest()
    intro=f"版本：Alpha v1.0 · 程序1.0.0-alpha · 规则{catalog['rulesVersion']} · 卡池{catalog['cardSetVersion']}。共{len(cards)}种卡，九套合法预设。"
    md=['# 巫师牌 Alpha v1.0 卡池图鉴','',intro,'',
        '配图和卡面文字直接取自正式资源；数据来自 [cards.json](../assets/cards.json)。可用[图文浏览版](card-catalog.html)搜索和组合筛选，或按下文索引阅读。','',
        '## 阅读规则','']
    md += ['- '+s for s in RULES]
    md += ['', '## 全卡索引','', '| 卡牌 | 类型 | 稀有度 | 学派 / 位阶 / 速度 | 费用 | 构筑定位 |', '|---|---|---|---|---|---|']
    for c in ordered:
        spell=f"{SCHOOLS[c['school']]} / {c['rank']}阶 / {c.get('speed',1)}速" if c['type'] in ('analytic','word') else '—'
        md.append(f"| [{c['name']}](#{c['id']}) | {TYPES[c['type']]} | {RARITIES[c['rarity']]} | {spell} | {cost(c)} | {NOTES[c['id']][0]} |")
    data=[]
    for t in TYPES:
        md += ['',f'## {TYPES[t]} · {counts[t]}种','']
        for c in ordered:
            if c['type']!=t: continue
            image='../assets/art/'+mapping[c['id']]
            md += [f'<a id="{c["id"]}"></a>', '',f'### {c["name"]}', '',f'![{c["name"]}]({image})','',
                   '| 属性 | 数值 |','|---|---|']
            md += [f'| {k} | {v} |' for k,v in properties(c)]
            md += ['', '**卡面效果**', '', c['text'], '', '**构筑定位与使用要点**','',NOTES[c['id']][0]+'。'+NOTES[c['id']][1]]
            for r in response(c): md += ['', '**响应条件**：'+r+'速度及目标限制仍须同时满足。']
            if c.get('flavor'): md += ['', '**风味文字**：'+c['flavor']]
            md += ['',f'内容 ID：`{c["id"]}`。','']
            data.append(dict(c,displayType=TYPES[t],displayRarity=RARITIES[c['rarity']],
                             displaySchool=SCHOOLS.get(c.get('school'),'—'),image=image,properties=properties(c),
                             role=NOTES[c['id']][0],note=NOTES[c['id']][1],responsesText=response(c)))
    md += ['## 五种代表性构筑方向','',
           '| 方向 | 预设 | 核心路线 |', '|---|---|---|',
           '| 快速解析 | 速解与支援 | 恩格亚斯低阶速解 → 飞弹快速响应 → 支援/资源行动 |',
           '| 来源荷载 | 持续荷载与防护 | 扩容增收 → 紊乱波动 → 反制守住来源 |',
           '| 塑能爆发 | 五芒星塑能 | 基础解析减费 → 寒霜铺开 → 火球/闪电收尾 |',
           '| 防护续航 | 防护与续航 | 临时生命/抗性 → 荷载管理 → 结界持续伤害 |',
           '| 言灵节奏 | 言灵与节奏 | 先设置 → 选择响应 → 检索与资源转换 |', '',
           '以下牌表可直接在游戏内复制。它们用于展示操作路线，自动跑局不代表胜率或推荐强度排序。','', '## 九套预设牌表','']
    for p in presets:
        md += [f'### {p["name"]}', '',f'基础阵法：{by_id[p["baseFormation"]]["name"]}（不计入主卡组30张）。', '',p['description'],'', '| 卡牌 | 份数 |','|---|---|']
        md += [f'| [{by_id[x["id"]]["name"]}](#{x["id"]}) | {x["count"]} |' for x in p['cards']]
        md += ['', '合计：30张。','']
    md += ['## 更新与依据','',
           '本图鉴由 `python tools/generate_card_catalog.py` 生成。效果、数值与插画随正式卡池更新；构筑要点需人工复核。', '',
           '[五阶段与连锁](alpha-v1.0-rules.md) · [速度/埋伏/防护规则](alpha-v1-cards.md) · [发布补充卡](alpha-release-cards.md) · [术语规范](terminology.md)', '',
           f'卡池源文件 SHA256：`{digest}`。','']
    (ROOT/'docs/card-catalog.md').write_text('\n'.join(md),encoding='utf-8')
    payload=json.dumps(data,ensure_ascii=False).replace('<','\\u003c')
    page=HTML.replace('__INTRO__',html.escape(intro)).replace('__DATA__',payload).replace('__COUNT__',str(len(cards)))
    (ROOT/'docs/card-catalog.html').write_text(page,encoding='utf-8')
    print('Generated catalogue: 30 cards, 9 validated deck lists, all images present.')

HTML='''<!doctype html>
<html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>巫师牌 · Alpha v1.0 卡池图鉴</title>
<style>
:root{color-scheme:dark;--bg:#0a121a;--panel:#111f29;--line:#34434b;--ink:#e3e9e8;--mint:#60d9c9;--gold:#d9b469}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.7 "Microsoft YaHei",sans-serif}
header,main{max-width:1380px;margin:auto;padding:32px}header{padding-bottom:16px}h1{color:var(--gold);font-size:36px;margin:12px 0}h2,h3{line-height:1.4}p{margin:10px 0}a{color:var(--mint)}.eyebrow{letter-spacing:3px;color:var(--mint);font-size:12px}.muted{color:#9fafb9;font-size:14px}
.filters{display:flex;flex-wrap:wrap;gap:12px;margin:20px 0}input,select,button{font:inherit;color:var(--ink);background:var(--panel);border:1px solid var(--line);border-radius:5px;padding:9px 12px}input{flex:1;min-width:220px}button{cursor:pointer}button:hover{border-color:var(--gold)}button:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid var(--mint);outline-offset:3px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:18px}.card{text-align:left;width:100%;padding:18px;background:var(--panel);border-top:2px solid var(--gold)}.card img{width:100%;height:170px;object-fit:contain;image-rendering:pixelated}.card h2{font-size:20px;margin:12px 0 6px}.card .cost{color:var(--mint)}.tag{display:inline-block;font-size:12px;color:var(--gold);margin-right:12px}.role{border-top:1px solid var(--line);padding-top:12px;margin-top:14px;font-size:14px}.card .brief{font-size:14px;display:-webkit-box;-webkit-line-clamp:3;-webkit-box-orient:vertical;overflow:hidden;min-height:71px}
dialog{width:min(900px,95vw);max-height:90vh;border:1px solid var(--gold);background:var(--panel);color:var(--ink);padding:28px}dialog::backdrop{background:#000b}.detail{display:grid;grid-template-columns:240px 1fr;gap:28px}.detail img{width:100%;height:250px;object-fit:contain;image-rendering:pixelated}.close{float:right}dl{display:grid;grid-template-columns:110px 1fr;gap:8px;margin:16px 0}dt{color:var(--gold)}dd{margin:0}#detailName{font-size:28px;color:var(--gold);margin-top:0}.section{border-top:1px solid var(--line);padding-top:14px;margin-top:18px}.empty{padding:36px;color:var(--gold)}footer{margin-top:30px;color:#9fafb9;font-size:14px}
@media(max-width:650px){header,main{padding:20px}.detail{grid-template-columns:1fr}.detail img{height:170px}h1{font-size:28px}}
</style>
<header><div class="eyebrow">WIZARDCARD / FIELD GUIDE</div><h1>巫师牌 · 卡池图鉴</h1><p class="muted">__INTRO__</p>
<p>点选卡牌查看完整效果、属性与使用要点。<a href="card-catalog.md">完整文字图鉴与九套牌表</a> · <a href="alpha-v1.0-rules.md">规则说明</a></p></header>
<main><div class="filters"><input id="query" aria-label="搜索卡名、效果或定位" placeholder="搜索卡名、效果、构筑定位…">
<select id="type" aria-label="卡牌类型"><option value="">全部类型</option></select><select id="rarity" aria-label="稀有度"><option value="">全部稀有度</option></select><select id="school" aria-label="学派"><option value="">全部学派</option></select><select id="base" aria-label="基础阵法资格"><option value="">全部资格</option><option value="yes">可作基础阵法</option></select><button id="reset">重置</button></div>
<p id="count" class="muted" aria-live="polite"></p><div id="grid" class="grid"></div>
<footer>费用为基础魔素费用；修正、荷载与额外成本按规则计算。言灵设置后正常释放不重复基础支付。构筑定位为编辑说明；数据来自随仓库发布的 cards.json。</footer></main>
<dialog id="detail"><button class="close" id="close" aria-label="关闭详情">关闭 ×</button><div id="detailBody"></div></dialog>
<script>
const cards=__DATA__;
const $=id=>document.getElementById(id);const escape=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
for(const [id,field] of [['type','displayType'],['rarity','displayRarity'],['school','displaySchool']]){for(const v of [...new Set(cards.map(c=>c[field]))].filter(x=>x!=='—')){const o=document.createElement('option');o.value=v;o.textContent=v;$(id).append(o)}}
function openCard(c){$('detailBody').innerHTML=`<h2 id="detailName">${escape(c.name)}</h2><div class="detail"><div><img src="${escape(c.image)}" alt="${escape(c.name)}"><p class="muted">${escape(c.id)}</p></div><div><dl>${c.properties.map(([k,v])=>`<dt>${escape(k)}</dt><dd>${escape(v)}</dd>`).join('')}</dl><div class="section"><h3>卡面效果</h3><p>${escape(c.text)}</p></div><div class="section"><h3>${escape(c.role)} · 使用要点</h3><p>${escape(c.note)}</p></div>${c.responsesText.map(r=>`<p class="section"><strong>响应条件</strong><br>${escape(r)} 速度与目标仍须合法。</p>`).join('')}${c.flavor?`<p class="section muted">${escape(c.flavor)}</p>`:''}</div></div>`;$('detail').setAttribute('aria-labelledby','detailName');$('detail').showModal()}
function render(){const q=$('query').value.trim().toLocaleLowerCase();const result=cards.filter(c=>(!q||[c.name,c.id,c.text,c.role,c.note,c.displaySchool].join(' ').toLocaleLowerCase().includes(q))&&(!$('type').value||c.displayType===$('type').value)&&(!$('rarity').value||c.displayRarity===$('rarity').value)&&(!$('school').value||c.displaySchool===$('school').value)&&(!$('base').value||c.baseEligible));$('count').textContent=`显示 ${result.length} / __COUNT__ 种卡牌`;$('grid').replaceChildren();if(!result.length){$('grid').innerHTML='<p class="empty">没有符合条件的卡牌，试试减少筛选条件。</p>';return}for(const c of result){const b=document.createElement('button');b.className='card';b.type='button';b.setAttribute('aria-label','查看'+c.name);b.innerHTML=`<img src="${escape(c.image)}" alt="" loading="lazy"><h2>${escape(c.name)}</h2><span class="tag">${escape(c.displayType)}</span><span class="tag">${escape(c.displayRarity)}</span><p class="muted">${c.displaySchool==='—'?'无学派 / 位阶':`${escape(c.displaySchool)} · ${c.rank}阶 · ${c.speed||1}速`}</p><p class="cost">${escape(c.properties.find(p=>p[0]==='费用')[1])}</p><p class="brief">${escape(c.text)}</p><div class="role">${escape(c.role)}</div>`;b.onclick=()=>openCard(c);$('grid').append(b)}}
for(const id of ['query','type','rarity','school','base'])$(id).addEventListener(id==='query'?'input':'change',render);$('reset').onclick=()=>{for(const id of ['query','type','rarity','school','base'])$(id).value='';render()};$('close').onclick=()=>$('detail').close();$('detail').addEventListener('click',e=>{if(e.target===$('detail')){const r=$('detail').getBoundingClientRect();if(e.clientX<r.left||e.clientX>r.right||e.clientY<r.top||e.clientY>r.bottom)$('detail').close()}});render();
</script></html>'''

if __name__ == '__main__':
    main()
