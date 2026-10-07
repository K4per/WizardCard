"""C-theme native pixel UI. Run from any directory using Python with Pillow.
Existing generated illustrations are referenced unchanged, never rewritten.
"""
from pathlib import Path
import importlib.util, json, math, shutil, hashlib, collections, zipfile
from PIL import Image

ROOT = Path(__file__).resolve().parent
ART = ROOT.parent.parent
PROJECT = ART.parent.parent
UI = ART / 'ui/c-theme-v1'
spec = importlib.util.spec_from_file_location('native_ui', ART/'design/v1/build_ui.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
m.ROOT=ROOT; m.UI=UI; m.SRC=ROOT/'sources'
m.P.update(bg='#E5CC97', ink='#392C29', panel='#F3E2BA', raised='#E2C891', fill='#D2B279', muted='#A38A68',
 bronze0='#4A2631',bronze1='#7D5637',bronze2='#A47A42',gold='#BC9553',lightgold='#E5C779',cream='#FAEFD2',
 teal0='#28554E',teal1='#367566',teal2='#55977C',mint='#78B99B',pale='#C0D9B1',
 gray='#928575',silver='#BCC0B0',white='#EAE4D4',text='#392C29',ruby='#662B3B',red='#943C4C',coral='#C56958',
 fire0='#9F4F35',orange='#B76839',yellow='#EBD18A',violet='#87638C',blue='#527F93',green='#52846A')
orig_icon=m.icon

def outline(c,x,y,w,h,edge='bronze2',fill='panel',ornate=True):
    cut=4
    c.layer('leather_rim')
    c.poly([(x+cut,y),(x+w-cut-1,y),(x+w-1,y+cut),(x+w-1,y+h-cut-1),(x+w-cut-1,y+h-1),(x+cut,y+h-1),(x,y+h-cut-1),(x,y+cut)],'bronze0')
    c.rect(x+3,y+3,w-6,h-6,edge);c.rect(x+5,y+5,w-10,h-10,fill)
    c.line([(x+6,y+4),(x+w-7,y+4)],'lightgold');c.line([(x+5,y+h-5),(x+w-6,y+h-5)],'bronze1')
    if ornate and w>35 and h>30:
        c.layer('gold_clasps')
        for xx,yy,sx,sy in [(x+7,y+7,1,1),(x+w-8,y+7,-1,1),(x+7,y+h-8,1,-1),(x+w-8,y+h-8,-1,-1)]:
            c.line([(xx+sx*11,yy),(xx,yy),(xx,yy+sy*11)],'gold',2)
            m.diamond(c,xx+sx*4,yy+sy*4,2,'teal1')
m.outline=outline

def emblem(c,typ,x,y,r=12):
    color=m.TYPES[typ][1]
    if typ=='action': c.poly([(x+2,y-r),(x-r,y+2),(x-2,y+2),(x-4,y+r),(x+r,y-3),(x+2,y-3)],color)
    elif typ=='analytic':
        c.poly([(x-r,y-r+2),(x-2,y-r+5),(x,y-r+3),(x+2,y-r+5),(x+r,y-r+2),(x+r,y+r-2),(x+2,y+r),(x,y+r-2),(x-2,y+r),(x-r,y+r-2)],color)
        c.line([(x,y-r+4),(x,y+r-3)],'cream',2)
    elif typ=='word':
        c.poly([(x+8,y-r),(x+r,y-7),(x-3,y+8),(x-8,y+8),(x-8,y+3)],color);c.line([(x-10,y+r),(x+6,y-7)],'gold',2)
    elif typ=='formation':
        pts=[(x+int(r*math.sin(i*math.pi/3)),y+int(r*math.cos(i*math.pi/3))) for i in range(7)]
        c.line(pts,color,2)
        c.line([(x,y-r),(x+r-2,y+r//2),(x-r+2,y+r//2),(x,y-r)],color,2)
        c.line([(x,y+r),(x+r-2,y-r//2),(x-r+2,y-r//2),(x,y+r)],color,2)
    else:
        for rr in (r,r-4): c.line([(x,y-rr),(x+rr,y),(x,y+rr),(x-rr,y),(x,y-rr)],color,2)
        m.diamond(c,x,y,3,'teal1')

def frame(w,h,typ,mode):
    c=m.Canvas(w,h);outline(c,0,0,w,h)
    c.layer('type_structure');accent=m.TYPES[typ][1]
    if typ=='action':
        for y in range(30,h-28,14): c.poly([(4,y),(9,y+4),(4,y+8)],accent)
    elif typ=='analytic':
        for y in (8,h-9): c.rect(12,y,w-24,2,'lightgold'); c.rect(15,y+2 if y==8 else y-2,w-30,1,'bronze1')
    elif typ=='word':
        for y in (12,h-13): c.line([(10,y),(16,y-3),(w-17,y-3),(w-11,y)],accent)
    elif typ=='formation':
        for x,y in [(12,12),(w-13,12),(12,h-13),(w-13,h-13)]:m.diamond(c,x,y,5,accent);m.diamond(c,x,y,2,'cream')
    else:
        c.line([(9,20),(9,h-21)],accent); c.line([(w-10,20),(w-10,h-21)],accent)
        for y in (14,h-15):m.diamond(c,w//2,y,4,accent)
    if mode=='full':
        c.layer('type_side_inlays')
        for side in (10,w-11):
            if typ=='analytic':
                for yy in range(104,272,24):c.line([(side-2,yy),(side+2,yy),(side+2,yy+8),(side-2,yy+8)],'gold')
            elif typ=='word':
                for yy in (118,225,401):c.line([(side,yy-12),(side+3,yy-8),(side-2,yy),(side+3,yy+8),(side,yy+12)],'violet')
            elif typ=='formation':
                for yy in (110,172,234):m.diamond(c,side,yy,4,'green');m.diamond(c,side,yy,1,'lightgold')
            elif typ=='seal':
                for yy in range(106,260,30):m.diamond(c,side,yy,3,'gold')
        c.layer('title');c.rect(14,14,w-28,40,'ruby');emblem(c,typ,32,34,11)
        c.rect(18,60,w-36,16,'raised')
        c.layer('art_window_border');c.rect(15,81,w-30,200,'bronze1');c.rect(17,83,w-34,196,'gold')
        # Transparent artwork aperture; clear raster and add SVG mask across previous layers.
        c.d.rectangle((19,85,w-20,276),fill=(0,0,0,0))
        for name,parts in c.layers.items():
            c.layers[name]=['<g mask="url(#artHole)">']+parts+['</g>']
        c.layer('aperture_definition');c.current.append(f'<defs><mask id="artHole"><rect width="{w}" height="{h}" fill="white"/><rect x="19" y="85" width="{w-38}" height="192" fill="black"/></mask></defs>')
        c.layer('stats')
        for row in range(2):
            for col in range(3):
                xx=18+col*108;yy=289+row*35
                c.rect(xx,yy,104,31,'raised');c.line([(xx,yy),(xx+103,yy)],'bronze2')
        c.layer('rules');c.rect(18,363,w-36,140,'cream');c.line([(18,359),(w-19,359)],'bronze2')
        c.layer('footer');c.rect(14,510,w-28,36,'ruby');c.line([(18,507),(w-19,507)],'gold')
    else:
        emblem(c,typ,16,13,6)
        if mode=='strip':c.line([(29,7),(29,h-8)],'bronze2')
        else:
            c.line([(9,23),(w-10,23)],'bronze2');c.rect(9,h-28,w-18,18,'raised')
            if mode=='detail':c.rect(12,43,w-24,109,'cream');c.rect(12,278,w-24,62,'raised')
    return c
m.frame_asset=frame

RARITIES=[('common','普通','silver',1),('uncommon','罕见','green',2),('rare','稀有','blue',3),('epic','史诗','violet',4),('legendary','传说','gold',5)]
def rarity(name,w=64,h=40):
    c=m.Canvas(w,h);_,_,col,n=next(r for r in RARITIES if r[0]==name);x=w//2;y=h//2-4;r=min(12,h//2-6)
    c.layer('rarity_silhouette')
    if n==1:c.rect(x-r+3,y-r+3,2*r-5,2*r-5,col);c.rect(x-r+5,y-r+5,2*r-9,2*r-9,'cream')
    elif n==2:m.diamond(c,x,y,r,col);m.diamond(c,x,y,r-4,'teal0')
    elif n==3:c.poly([(x-r,y-5),(x-5,y-r),(x+5,y-r),(x+r,y-5),(x+7,y+r),(x-7,y+r)],col);m.diamond(c,x,y,4,'pale')
    elif n==4:
        pts=[(x+int((r if i%2==0 else r//2)*math.sin(i*math.pi/8)),y+int((r if i%2==0 else r//2)*math.cos(i*math.pi/8))) for i in range(16)]
        c.poly(pts,col);m.diamond(c,x,y,4,'cream')
    else:
        c.poly([(x-r,y-6),(x-5,y-2),(x,y-r),(x+5,y-2),(x+r,y-6),(x+r-3,y+7),(x-r+3,y+7)],col);c.rect(x-r+3,y+9,2*r-5,2,'lightgold');m.diamond(c,x,y,3,'teal1')
    c.layer('rank_pips')
    for i in range(n):c.rect(x-n*4+i*8+2,h-5,4,3,col)
    return c

def icon(name):
    if name.startswith('type_'):
        c=m.Canvas(32,32);emblem(c,name[5:],16,16);return c
    return orig_icon(name)
m.icon=icon

def field_list(card):
    typ=card['type']; f=[]
    def add(key,label,default=None):
        if key in card or default is not None:f.append((label,str(card.get(key,default))))
    if typ=='formation':
        for k,l in [('body','体量'),('capacity','负载上限'),('income','魔力产出'),('rings','环位'),('maxRank','最高环阶'),('speed','速度')]:add(k,l)
    elif typ=='analytic':
        for k,l in [('cost','解析费用'),('castCost','施法费用'),('rank','环阶'),('analysisTurns','解析回合'),('speed','速度'),('duration','持续')]:add(k,l,1 if k=='analysisTurns' else None)
    elif typ=='word':
        for k,l in [('cost','施法费用'),('rank','环阶'),('speed','速度'),('duration','持续')]:add(k,l)
    elif typ=='seal':
        for k,l in [('cost','费用'),('body','体量'),('burden','负载'),('speed','速度'),('activationCost','启动费用')]:add(k,l)
    else:
        for k,l in [('cost','费用'),('speed','速度')]:add(k,l)
    return f

def build():
    m.build()
    for entry in m.REGISTRY:
        entry.pop('reserved',None)
        if entry['category']=='cards' and entry['id'].startswith('frame_'):
            entry['stretch']='none';entry.pop('insets',None);entry['usage']='固定尺寸卡牌组件；按需制作额外尺寸，不整体拉伸内部结构'
    for name,label,col,n in RARITIES:
        small=m.Canvas(42,12)
        for i in range(n):m.diamond(small,5+i*8,6,3,col)
        small.save(UI/'cards'/f'rarity_{name}.png',m.SRC/'cards'/f'rarity_{name}.svg')
        m.export(rarity(name),f'rarity_badge_{name}','rarity',f'{label}：独立轮廓＋颜色＋{n}枚刻度',rarity=name,stretch='none')
    for typ in m.TYPES:
        m.export(frame(360,560,typ,'full'),f'frame_{typ}_full','cards',m.TYPES[typ][0]+'完整模板',stretch='none',
          layout={'title':[48,20,266,28],'school':[18,60,324,16],'art':[19,85,322,192],'stats':[18,289,324,66],'rules':[26,372,308,124],'type':[24,520,176,18],'rarity_label':[224,520,48,18],'rarity':[279,508,64,40]},stats_schema={'action':['cost','speed'],'analytic':['cost','castCost','rank','analysisTurns','speed','duration'],'word':['cost','rank','speed','duration'],'formation':['body','capacity','income','rings','maxRank','speed'],'seal':['cost','body','burden','speed','activationCost']}[typ])
    for typ in m.TYPES:
        c=m.Canvas(180,34);outline(c,0,0,180,34,'gold','ruby');emblem(c,typ,20,17,10)
        m.export(c,'type_header_'+typ,'cards','类型标题条；文字动态叠加',insets=[38,7,12,7],stretch='horizontal')
    for zone,typ in [('casting','analytic'),('words','word'),('analysis','analytic'),('action','action'),('ash','seal'),('formation','formation'),('hand','action'),('deck','seal')]:
        c=m.Canvas(224,40);outline(c,0,0,224,40,'gold','ruby');emblem(c,typ,24,20,11)
        m.export(c,'zone_header_'+zone,'board','区域独立标题条：'+zone,text_rect=[46,10,162,20],insets=[42,8,12,8],stretch='horizontal')
    for name,col in [('life','red'),('mana','teal1'),('load','violet'),('capacity','gold')]:
        c=m.Canvas(192,18);outline(c,0,0,192,18,'bronze2','ink',False);m.export(c,'meter_'+name+'_track','board',name+'数值条底框',insets=[8,5,8,5],stretch='horizontal')
        c=m.Canvas(176,8);c.rect(0,0,176,8,col);c.rect(0,0,176,2,'lightgold');m.export(c,'meter_'+name+'_fill','board',name+'数值条填充；按比例裁剪',stretch='clip_horizontal')
    for state in ['hover','selected','target_candidate','target_selected','cost_candidate','cost_selected']:
        m.export(m.state_overlay(360,560,state),'overlay_full_'+state,'interaction','完整卡牌 '+state,overlay=True)
    for name in ['card_detail','stack','settings','tooltip']:
        c=m.Canvas(440,600 if name=='card_detail' else 260);outline(c,0,0,c.w,c.h);c.rect(12,12,c.w-24,34,'ruby')
        m.export(c,name,'interaction','通用 '+name+'窗体',insets=[12,52,12,12],stretch='nine_slice')
    source=Path(r'C:\Users\home_\.codex\generated_images\01a1031f-0510-78a2-8dac-7e119a663ac0\exec-68709b82-9e99-460e-aa10-4aa661001403.png')
    dest=UI/'materials/parchment-backing.png';dest.parent.mkdir(parents=True,exist_ok=True)
    if source.exists():shutil.copyfile(source,dest)
    if dest.exists():
        with Image.open(dest) as im:size=list(im.size)
        m.REGISTRY.append({'id':'parchment-backing','category':'materials','file':dest.relative_to(ART).as_posix(),'size':size,'purpose':'AI 生成的 C 风格羊皮纸底图；可选背景，不规定布局','stretch':'none','provenance':'imagegen / 原始输出完整复制，未修改','sha256':hashlib.sha256(dest.read_bytes()).hexdigest()})
    manifest={'version':'wizardcard_c_theme_v1','runtime_integrated':False,'sampling':'nearest','text_in_textures':False,'palette':m.P,'card_types':list(m.TYPES),'rarities':[r[0] for r in RARITIES],'assets':m.REGISTRY}
    (UI/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    previews(ROOT/'previews')
    inventory(manifest)
    validate(manifest)
    package()

def previews(out):
    out.mkdir(exist_ok=True)
    data=json.loads((ART.parent/'cards.json').read_text(encoding='utf-8'))['cards']; art=json.loads((ART/'runtime.json').read_text(encoding='utf-8'))['cards']
    schools={'evocation':'塑能','abjuration':'防护','necromancy':'死灵','transmutation':'变化','enchantment':'惑控','divination':'预言','illusion':'幻术','conjuration':'咒法'}
    for card in data:
        c=m.Canvas(360,560);c.sprite(UI/'cards'/f"frame_{card['type']}_full.png",0,0)
        c.layer('existing_illustration');c.sprite(ART/art[card['id']],19,85,322,192,contain=True)
        c.text(card['name'],50,24,20,'cream');sub=schools.get(card.get('school'), '基础阵法' if card.get('baseEligible') else 'WizardCard')
        if card.get('responseOnly'):sub+=' · 仅响应'
        if card.get('concentration'):sub+=' · 专注'
        c.text(sub,24,60,12,'text')
        fields=field_list(card)
        for i,(label,value) in enumerate(fields[:6]):
            xx=24+(i%3)*108;yy=296+(i//3)*35;c.text(label,xx,yy,11);c.text(value,xx+77,yy-2,17)
        c.wrapped(card['text'],26,373,308,size=17,lineheight=25,maxlines=5)
        c.text(m.TYPES[card['type']][0],24,520,16,'cream');c.text(next(r[1] for r in RARITIES if r[0]==card['rarity']),224,522,14,'cream');c.sprite(UI/'rarity'/f"rarity_badge_{card['rarity']}.png",279,508)
        c.save(out/(card['id']+'.png'),out/(card['id']+'.svg'))
    chosen=[next(card for card in data if card['type']==typ) for typ in m.TYPES]
    sheet=m.Canvas(1900,760,'bg');sheet.text('WizardCard · C 风格卡牌模板',30,22,32);sheet.text('行动 / 解析法术 / 言灵 / 阵法 / 符文 · 真实卡牌排版预览',30,68,19)
    for i,card in enumerate(chosen):sheet.sprite(out/(card['id']+'.png'),20+i*376,114)
    for i,(name,label,col,n) in enumerate(RARITIES):sheet.sprite(UI/'rarity'/f'rarity_badge_{name}.png',270+i*285,700);sheet.text(label,340+i*285,713,18)
    sheet.save(out/'card-types-and-rarities.png')
    c=m.Canvas(1440,940,'bg');c.text('C 风格组件库 · 羊皮纸 / 酒红 / 古金 / 青绿',28,24,28)
    c.text('区域标题可自由组合，以下仅为资源展示',28,65,17)
    for i,zone in enumerate(['casting','words','analysis','action','ash','formation','hand','deck']):
        x=28+(i%4)*350;y=105+(i//4)*82;c.sprite(UI/'board'/f'zone_header_{zone}.png',x,y);c.text({'casting':'施法区','words':'言灵区','analysis':'解析区','action':'行动区','ash':'灰烬区','formation':'阵法区','hand':'手牌区','deck':'牌库区'}[zone],x+46,y+11,16,'cream')
    for i,kind in enumerate(['primary','secondary','danger']):
        c.text({'primary':'青绿主按钮','secondary':'羊皮纸次按钮','danger':'酒红危险按钮'}[kind],28,i*72+303,16)
        for j,state in enumerate(['normal','hover','pressed','disabled']):
            x=300+j*255;y=i*72+300;c.sprite(UI/'interaction'/f'button_{kind}_{state}.png',x,y);c.text(state,x,y+40,12)
    for i,name in enumerate(['life','mana','load','capacity']):
        x=28+i*350;c.sprite(UI/'board'/f'meter_{name}_track.png',x,542);c.sprite(UI/'board'/f'meter_{name}_fill.png',x+8,547);c.text(name,x,570,16)
    c.sprite(UI/'interaction/decision.png',28,610);c.text('弹窗 / 决策框体',46,625,18)
    names=list(m.ICONS)
    for i,name in enumerate(names):
        x=620+(i%12)*64;y=630+(i//12)*62;c.sprite(UI/'icons'/f'{name}.png',x,y)
    c.save(out/'component-library.png')
    html='<!doctype html><html lang="zh"><meta charset="utf-8"><title>WizardCard C 资源库</title><style>body{background:#e5cc97;color:#392c29;font:16px sans-serif;margin:30px}img{max-width:100%;image-rendering:pixelated}.cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:20px}.cards img{width:360px}a{color:#28554e}</style><h1>WizardCard C 风格资产</h1><p>资源展示，不调整战场布局。透明组件中的文字与数值由程序动态绘制。</p><p><a href="ASSET_LIST.md">完整清单</a> · <a href="../../ui/c-theme-v1/manifest.json">机器可读清单</a></p><img src="previews/card-types-and-rarities.png"><img src="previews/component-library.png"><h2>当前 30 张卡牌排版</h2><div class="cards">'
    for card in data:html+=f'<div><img src="previews/{card["id"]}.png"><p>{card["name"]} · {card["id"]}</p></div>'
    (ROOT/'gallery.html').write_text(html+'</div></html>',encoding='utf-8')

def inventory(manifest):
    counts=collections.Counter(a['category'] for a in manifest['assets'])
    lines=['# WizardCard C 风格美术资源清单','','已完成独立素材绘制；未接入游戏，也未重排战场。','',f'共 **{len(manifest["assets"])} 个 PNG 资产**；另附 30 张真实卡牌排版预览、2 张资源总览、分层 SVG 源文件、生成脚本和 JSON 清单。','','|分类|PNG 数量|','|---|---:|']
    lines += [f'|{k}|{v}|' for k,v in counts.items()]
    lines += ['','## 卡牌模板与信息设计','','五类均有手牌、条目、小立绘、详情和完整模板，共 25 套。完整模板 360×560；透明插画窗 322×192。','行动：折角齿形侧边与闪电纹章；解析法术：卷册双层横边与书页纹章；言灵：流线卷饰与羽笔纹章；阵法：角部晶格与六芒星纹章；符文：双线镶边与菱环纹章。','','卡名独立酒红标题栏，学派／基础属性副标题，插画窗，下方两行共六个属性格，规则文字区，底部类型与独立稀有度槽。','属性以当前 cards.json 为准：行动费用与速度；解析费用／施法费用／环阶／解析回合／速度／持续；言灵费用／环阶／速度／持续；阵法体量／负载上限／魔力产出／环位／最高环阶／速度；符文费用／体量／负载／速度／启动费用（字段存在才展示）。','完整卡规则区支持当前卡池全部文本；风味文字属于可选扩展详情，不烘焙进卡框。缩略模板用于简略信息；完整正文应在完整模板或滚动详情中展示。','','## 稀有度','','普通：银灰方章；罕见：青绿菱晶；稀有：蓝色六边宝石；史诗：紫色星芒；传说：古金王冠。每档附 1—5 枚刻度，并保留程序绘制名称的位置。颜色、轮廓、刻度共同区分，稀有度与类型无绑定。每种类型可组合任意稀有度。','','## 交付与使用','','PNG 均为独立组件，不包含卡名／规则／数值文字；大卡框插画窗透明。预览文件是文字和现有插画的示范组合，不能用作动态卡牌底板。','像素组件使用最近邻缩放；卡框保持固定比例，其他组件按 manifest 的九宫格／横向拉伸／裁剪参数使用。尺寸只是素材默认规格，不规定最终战场布局。','施法区、言灵区、解析区、行动区、灰烬区均提供独立标题与区域框体，双方可复用。另含阵法、手牌、牌库、HUD、弹窗、交互状态、资源条、图标与卡背。','材料底图为本次 imagegen 原始生成输出；其余 UI 为可编辑代码原生像素图形。既有 30 张插画仅在预览中引用，未重绘或改动。','','## 逐项清单','','下表路径相对于 assets/art。','','|分类 / ID|尺寸|文件|用途|','|---|---|---|---|']
    for a in manifest['assets']:lines.append(f'|{a["category"]} / {a["id"]}|{a["size"][0]}×{a["size"][1]}|`{a["file"]}`|{a["purpose"]}|')
    (ROOT/'ASSET_LIST.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    (ROOT/'README.md').write_text('# C 风格资源\n\n入口：gallery.html；完整清单：ASSET_LIST.md；运行时资源：../../ui/c-theme-v1/manifest.json。\n\n运行 build_assets.py 需要 Python / Pillow，并依赖 ../v1/build_ui.py、assets/fonts/NotoSansCJKsc-Regular.otf、assets/cards.json、assets/art/runtime.json 与既有卡牌插画。所有这些依赖均包含在 ZIP 中。素材目录不会修改旧 UI 或运行时配置。\n\n分层 SVG 中图形可编辑，透明插画窗以 mask 定义；实际游戏使用独立 PNG 与动态文字。预览 SVG 引用原始插画，文字可编辑。\n',encoding='utf-8')

def validate(manifest):
    import xml.etree.ElementTree as ET
    ids=set();transparent=0
    for a in manifest['assets']:
        assert (a['category'],a['id']) not in ids;ids.add((a['category'],a['id']))
        p=ART/a['file']
        with Image.open(p) as im:
            assert list(im.size)==a['size'];assert im.mode in ['RGB','RGBA']
            if im.mode=='RGBA' and im.getextrema()[3][0]==0:transparent+=1
            if a['category']!='materials':
                assert im.mode=='RGBA'
                assert set(im.getchannel('A').getdata())<={0,255},a['id']
        if 'source' in a:ET.parse(ART/a['source'])
    data=json.loads((ART.parent/'cards.json').read_text(encoding='utf-8'))['cards'];max_lines=0
    for card in data:
        c=m.Canvas(360,560);end_y=c.wrapped(card['text'],26,373,308,17,lineheight=25);n=(end_y-373)//25
        assert n<=5,(card['id'],n);max_lines=max(max_lines,n)
        assert len(field_list(card))<=6
        assert c.d.textlength(card['name'],font=m.font(20))<=266,card['id']
    for svg in list(m.SRC.rglob('*.svg'))+list((ROOT/'previews').glob('*.svg')):
        doc=ET.parse(svg)
        for node in doc.iter():
            href=node.attrib.get('href')
            if href:assert (svg.parent/href).resolve().exists(),(svg,href)
    for typ in m.TYPES:
        with Image.open(UI/'cards'/f'frame_{typ}_full.png') as im:assert im.getpixel((180,180))[3]==0
    report={'assets':len(manifest['assets']),'transparent_pngs':transparent,'svg_parsed':len(list(m.SRC.rglob('*.svg'))),'cards_previewed':len(data),'max_rule_lines':max_lines,'all_current_rule_text_fits':True,'titles_fit':True,'svg_image_links_resolve':True,'native_components_binary_alpha':True,'full_frame_art_windows_transparent':True,'runtime_integrated':False}
    (ROOT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(report)

def package():
    dist=ROOT/'dist';dist.mkdir(exist_ok=True);paths=set()
    paths.update(p for p in ROOT.rglob('*') if p.is_file() and 'dist' not in p.parts and '__pycache__' not in p.parts)
    paths.update(p for p in UI.rglob('*') if p.is_file())
    paths.add(ART/'design/v1/build_ui.py');paths.add(ART.parent/'cards.json');paths.add(ART/'runtime.json')
    paths.update(p for p in (ART.parent/'fonts').glob('*') if p.is_file())
    mapping=json.loads((ART/'runtime.json').read_text(encoding='utf-8'))['cards'];paths.update(ART/p for p in mapping.values())
    with zipfile.ZipFile(dist/'WizardCard-C-theme-v1.zip','w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(paths):z.write(p,p.relative_to(PROJECT))
    print('Package:',dist/'WizardCard-C-theme-v1.zip')

if __name__=='__main__':build()
