"""Update asset-only runtime mapping, provenance, inspection report and local gallery.

Run after build_ui.py and build_previews.py. Never modifies illustration pixels.
"""
from pathlib import Path
from hashlib import sha256
from collections import Counter
from html import escape
import json, xml.etree.ElementTree as ET
import numpy as np
from PIL import Image, ImageColor

ROOT=Path(__file__).resolve().parent
ART=ROOT.parent.parent
PROJECT=ART.parent.parent
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def digest(p):return sha256(p.read_bytes()).hexdigest()

def finalize():
    cards=read(PROJECT/'assets/cards.json')['cards']
    prompts=read(ROOT/'illustration_prompts.json')
    provenance=read(ART/'provenance.json')
    previous={r['id']:r for r in provenance}
    original_files=prompts['generated_files']
    for record in original_files:
        ident=record['id'];file=ART/'cards'/f'{ident}.png'
        previous[ident]={'id':ident,'file':f'cards/{ident}.png','source':'OpenAI built-in imagegen',
          'sourceVersion':'art_production_v1','originalGeneratedFilename':record['original'],
          'promptFile':'design/v1/illustration_prompts.json','sha256':digest(file),
          'licenseNote':'AI-generated project artwork; no external stock assets or additional third-party license claim.'}
    runtime=read(ART/'runtime.json')
    runtime['cards']={c['id']:f'cards/{c["id"]}.png' for c in cards}
    runtime['fallback']='Existing procedural card-type emblem when an illustration is missing or unreadable.'
    write(ART/'runtime.json',runtime)
    write(ART/'provenance.json',list(previous.values()))
    report={'date':'2026-10-04','scope':'Asset files and design sheets; UI skin is not integrated into the client.',
      'cards':[],'ui':[],'previews':[],'errors':[],'notes':[]}
    errors=report['errors']
    for c in cards:
        path=ART/runtime['cards'][c['id']]
        im=Image.open(path);im.load();a=np.asarray(im.convert('RGBA'))[:,:,3]
        h,w=a.shape;edges=np.concatenate([a[0,:],a[-1,:],a[:,0],a[:,-1]])
        item={'id':c['id'],'file':runtime['cards'][c['id']],'mode':im.mode,'size':list(im.size),
          'sha256':digest(path),'transparent_fraction':round(float((a==0).mean()),5),
          'partial_alpha_fraction':round(float(((a>0)&(a<255)).mean()),5),
          'alpha_bbox':im.convert('RGBA').getchannel('A').getbbox(),'transparent_outer_edges':bool((edges==0).all()),
          'outer_edge_alpha_max':int(edges.max()),'visible_subject_clear_of_edges':bool((edges<=1).all()),
          'provenance_matches':digest(path)==previous[c['id']]['sha256']}
        if im.mode!='RGBA' or not (a==0).any() or not (a>0).any():errors.append(c['id']+': invalid alpha')
        if not item['visible_subject_clear_of_edges']:errors.append(c['id']+': visible subject reaches outer edge')
        elif not item['transparent_outer_edges']:report['notes'].append(c['id']+': outer edge has alpha 1/255 residue; original AI output preserved, not strict binary-alpha pixel art.')
        if not item['provenance_matches']:errors.append(c['id']+': provenance mismatch')
        report['cards'].append(item)
    manifest=read(ART/'ui/manifest.json')
    palette={ImageColor.getrgb(v) for v in manifest['palette'].values()}
    seen=set()
    for asset in manifest['assets']:
        path=ART/asset['file'];src=ART/asset['source'];im=Image.open(path).convert('RGBA');a=np.asarray(im)
        alphas=set(np.unique(a[:,:,3]).tolist());rgbs={tuple(row) for row in np.unique(a[a[:,:,3]>0][:,:3],axis=0)}
        doc=ET.parse(src);missing=[]
        for el in doc.iter():
            href=el.get('href')
            if href and not (src.parent/href).is_file():missing.append(href)
        no_text=not any(el.tag.endswith('}text') for el in doc.iter())
        item={'id':asset['id'],'size_ok':list(im.size)==asset['size'],'binary_alpha':alphas<={0,255},
          'palette_ok':rgbs<=palette,'source_ok':not missing,'no_baked_text':no_text,'sha256':digest(path)}
        if asset['id'] in seen:errors.append(asset['id']+': duplicate id')
        seen.add(asset['id'])
        if not all(item[k] for k in ['size_ok','binary_alpha','palette_ok','source_ok','no_baked_text']):errors.append(asset['id']+': asset check failed')
        asset.update(sha256=item['sha256'],alpha='binary 0/255',origin='Original code-native project pixel UI',license='Project artwork; no external stock assets',anchor=asset.get('anchor',[0,0]))
        report['ui'].append(item)
    write(ART/'ui/manifest.json',manifest)
    flows=read(ROOT/'flows.json')
    for sheet in flows['sheets']:
        path=ROOT/sheet['png'];src=ROOT/sheet['source'];im=Image.open(path);im.load();doc=ET.parse(src)
        refs=[el.get('href') for el in doc.iter() if el.get('href')]
        item={'id':sheet['id'],'size_ok':im.size==(1600,1000),'linked_assets_ok':all((src.parent/r).is_file() for r in refs)}
        if not all(item[k] for k in ['size_ok','linked_assets_ok']):errors.append(sheet['id']+': preview check failed')
        report['previews'].append(item)
    report['summary']={'card_illustrations':len(cards),'new_illustrations':len(original_files),'ui_pngs':len(manifest['assets']),
      'ui_categories':dict(Counter(a['category'] for a in manifest['assets'])),'design_sheets':len(flows['sheets']),
      'flows':len(flows['flows']),'preserved_original_four':all(previous[k]['sourceVersion']=='pixel_fantasy_v2' and digest(ART/previous[k]['file'])==previous[k]['sha256'] for k in ['balance','fireball','barbs','ring']),
      'errors':len(errors)}
    write(ROOT/'validation.json',report)
    gallery(cards,manifest,flows,report['summary'])
    print(json.dumps(report['summary'],ensure_ascii=False,indent=2))
    if errors:raise RuntimeError('; '.join(errors))

def gallery(cards,manifest,flows,stats):
    tiles=''.join(f'<article class="art"><a href="../../cards/{c["id"]}.png" target="_blank"><img loading="lazy" src="../../cards/{c["id"]}.png" alt="{c["name"]}"></a><h3>{c["name"]}</h3><span>{c["id"]} · 透明 PNG</span></article>' for c in cards)
    groups=''
    for group,title in [('cards','卡框与卡背'),('board','场地组件'),('interaction','操作组件'),('icons','图标')]:
        items=[a for a in manifest['assets'] if a['category']==group]
        groups+=f'<details><summary>{title} · {len(items)} PNG</summary><div class="assetgrid">'
        for a in items:
            groups+=f'<article><a href="../../{a["file"]}" target="_blank"><img loading="lazy" src="../../{a["file"]}" alt="{a["id"]}"></a><p>{escape(a["purpose"])}<br><code>{a["id"]}</code><br>{a["size"][0]} × {a["size"][1]} · <a href="../../{a["source"]}" target="_blank">SVG 源稿</a></p></article>'
        groups+='</div></details>'
    template='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>巫师牌 · 美术资源 v1</title>
<style>
:root{color-scheme:dark;--bg:#0a1119;--panel:#122029;--text:#e7e5d5;--muted:#a4b4cb;--gold:#c49a5a;--mint:#72d7c5}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.65 system-ui,"Microsoft YaHei",sans-serif}main{max-width:1640px;margin:auto;padding:32px}h1{font-weight:500;font-size:36px;margin:10px 0}h2{font-size:25px;font-weight:500;margin-top:54px}p{color:var(--muted)}a{color:var(--mint)}nav{display:flex;gap:24px;flex-wrap:wrap}nav a{text-decoration:none}header{border-bottom:1px solid #6d5035;padding-bottom:26px}.eyebrow{color:var(--gold);letter-spacing:.14em;font-size:13px}.stats{display:flex;gap:30px;flex-wrap:wrap;margin:26px 0}.stats b{font-size:32px;font-weight:500;color:var(--gold);margin-right:7px}.toolbar{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:15px}button,select{border:1px solid #6d5035;background:var(--panel);color:var(--text);padding:9px 13px;font:inherit;border-radius:2px}button{cursor:pointer}button:disabled{opacity:.4;cursor:default}button:hover:not(:disabled){border-color:var(--mint)}.viewer{border:1px solid #364e59;background:#101923;overflow:auto;max-height:85vh}.viewer img{display:block;width:100%;image-rendering:pixelated}.viewer.native img{width:1600px;max-width:none}.caption{min-height:32px}.artgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:18px}.art{background:var(--panel);border:1px solid #263e49;padding:14px}.art img{display:block;width:100%;aspect-ratio:4/3;object-fit:contain;image-rendering:pixelated;background-color:#1b2630;background-image:linear-gradient(45deg,#26343f 25%,transparent 25%),linear-gradient(-45deg,#26343f 25%,transparent 25%),linear-gradient(45deg,transparent 75%,#26343f 75%),linear-gradient(-45deg,transparent 75%,#26343f 75%);background-size:20px 20px;background-position:0 0,0 10px,10px -10px,-10px 0}.artgrid.light img{background:#e7e5d5}.art h3{font-size:17px;font-weight:500;margin:10px 0 0}.art span{color:var(--muted);font-size:13px}details{border-top:1px solid #364e59;padding:15px 0}summary{cursor:pointer;color:var(--gold)}.assetgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:16px;margin-top:22px}.assetgrid article{padding:12px;background:var(--panel);overflow:hidden}.assetgrid img{display:block;max-width:100%;height:125px;object-fit:contain;image-rendering:pixelated;margin:auto}.assetgrid p{font-size:13px;overflow-wrap:anywhere}footer{border-top:1px solid #6d5035;margin-top:50px;padding:25px 0;font-size:14px}code{font-size:12px}select{max-width:100%}@media(max-width:600px){main{padding:16px}h1{font-size:27px}.stats{gap:16px}.stats b{font-size:25px}}
</style><main><header><div class="eyebrow">WIZARDCARD / PIXEL FANTASY / 2026.10.04</div><h1>巫师牌 · 美术资源 v1</h1><p>深色石板、古铜边饰与青绿符纹。卡牌主体全部透明；文字与数值独立绘制。</p><div class="stats"><span><b>13</b>幅插画</span><span><b>190</b>个 UI PNG</span><span><b>__SHEETS__</b>张设计稿</span><span><b>13</b>组流程</span></div><nav><a href="#design">设计与流程</a><a href="#illustrations">卡牌插画</a><a href="#components">独立组件</a><a href="README.md">交付说明</a><a href="integration.md">接入规范</a><a href="validation.json">文件检查</a></nav><p>插画已加入运行映射；新 UI 组件为已导出的设计资源，尚待客户端接入和实机验收。</p></header>
<section id="design"><h2>设计稿与操作流程</h2><div class="toolbar"><select id="flow" aria-label="选择流程"></select><select id="sheet" aria-label="选择状态"></select><button id="prev">← 上一张</button><button id="next">下一张 →</button><button id="zoom">原始 1600px</button><a id="png" target="_blank">PNG</a><a id="svg" target="_blank">SVG 源稿</a></div><div class="viewer" id="viewer"><img id="preview" alt="设计预览"></div><p class="caption" id="caption"></p><p>流程用于说明视觉和交互状态，属于手工设计稿，不是游戏存档或可执行规则重放。方向键可切换状态。</p></section>
<section id="illustrations"><h2>全卡池透明插画</h2><div class="toolbar"><button id="bg">切换浅色底</button><span>点击图片查看原始 PNG。棋盘格仅用于透明度预览。</span></div><div class="artgrid" id="arts">__TILES__</div></section>
<section id="components"><h2>独立 UI 资源</h2><p>190 张 PNG 均不含烘焙文字，采用固定色板和二值透明度；图标包含 32px 与 64px 两种导出。插画保留 AI 原始像素风结果，未宣称通过严格限色精修。</p><p><a href="../../ui/manifest.json">完整资源清单与尺寸</a> · <a href="tokens.json">颜色与状态规范</a></p>__GROUPS__</section><footer>字体沿用 Noto Sans CJK SC。原有四幅插画逐字节保留。可编辑源文件与生成脚本同包交付。</footer></main>
<script>
const data=__DATA__;const byId=Object.fromEntries(data.sheets.map(x=>[x.id,x]));const flow=document.querySelector('#flow'),sheet=document.querySelector('#sheet');
const standalone=data.sheets.filter(x=>!x.id.startsWith('flow_')).map(x=>x.id);const collections=[{id:'all',title:'总览与组件设计',states:standalone},...data.flows];
for(const f of collections)flow.add(new Option(f.title,f.id));
function fill(){const f=collections.find(x=>x.id===flow.value);sheet.replaceChildren();f.states.forEach(id=>sheet.add(new Option(byId[id].title,id)));show()}
function show(){const s=byId[sheet.value];document.querySelector('#preview').src=s.png;document.querySelector('#preview').alt=s.title;document.querySelector('#png').href=s.png;document.querySelector('#svg').href=s.source;document.querySelector('#caption').textContent=s.title+(s.annotation?' — '+s.annotation:'');document.querySelector('#prev').disabled=sheet.selectedIndex===0;document.querySelector('#next').disabled=sheet.selectedIndex===sheet.options.length-1}
function step(n){sheet.selectedIndex=Math.max(0,Math.min(sheet.options.length-1,sheet.selectedIndex+n));show()}
flow.onchange=fill;sheet.onchange=show;document.querySelector('#prev').onclick=()=>step(-1);document.querySelector('#next').onclick=()=>step(1);document.querySelector('#zoom').onclick=e=>{const active=document.querySelector('#viewer').classList.toggle('native');e.target.textContent=active?'适应窗口':'原始 1600px'};document.querySelector('#bg').onclick=e=>{const light=document.querySelector('#arts').classList.toggle('light');e.target.textContent=light?'切换透明棋盘格':'切换浅色底'};document.addEventListener('keydown',e=>{if(['SELECT','INPUT'].includes(e.target.tagName))return;if(e.key==='ArrowRight')step(1);if(e.key==='ArrowLeft')step(-1)});fill();sheet.value='board_complex';show();
</script></html>'''
    result=template.replace('__SHEETS__',str(stats['design_sheets'])).replace('__TILES__',tiles).replace('__GROUPS__',groups).replace('__DATA__',json.dumps(flows,ensure_ascii=False).replace('</','<\\/'))
    (ROOT/'index.html').write_text(result,encoding='utf-8')

if __name__=='__main__':finalize()
