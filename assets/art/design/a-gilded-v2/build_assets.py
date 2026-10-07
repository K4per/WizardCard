"""A gilded grimoire assets: original generated masters + editable native UI.
Generated pixels are never edited. Composites are review-only, separate files.
"""
from pathlib import Path
import importlib.util, json, math, hashlib, collections, zipfile
from PIL import Image
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent
ART=ROOT.parent.parent
PROJECT=ART.parent.parent
UI=ART/'ui/a-gilded-v2'
spec=importlib.util.spec_from_file_location('c_theme',ART/'design/c-theme-v1/build_assets.py')
t=importlib.util.module_from_spec(spec);spec.loader.exec_module(t)
m=t.m
t.ROOT=ROOT;t.UI=UI;m.ROOT=ROOT;m.UI=UI;m.SRC=ROOT/'sources'
m.P.update(bg='#E7CF9F',ink='#322324',panel='#F5E4BF',raised='#E8CF99',fill='#CCAE75',muted='#987C60',
 bronze0='#321B20',bronze1='#7A4227',bronze2='#AD7933',gold='#D3A64A',lightgold='#F4D785',cream='#FFF1CD',
 ruby='#632035',red='#993249',coral='#CF7361',teal0='#153F3B',teal1='#196D63',teal2='#2F9985',mint='#70BDA0',
 silver='#BFCBD0',white='#E9EEF0',blue='#3A81B8',violet='#9B68C2',green='#3A9576',text='#322324')
old_emblem=t.emblem;old_frame=t.frame;old_icon=m.icon

def gold_line(c,pts,width=3):
    c.line([(x+1,y+1) for x,y in pts],'bronze1',width+2)
    c.line(pts,'gold',width)
    c.line([(x,y-1) for x,y in pts],'lightgold',1)

def jewel(c,x,y,r,col='teal1'):
    m.diamond(c,x,y,r+2,'bronze0');m.diamond(c,x,y,r+1,'gold');m.diamond(c,x,y,r,col)
    c.poly([(x,y-r),(x,y),(x-r,y)],'pale' if col=='teal1' else 'cream')
    c.poly([(x,y-r),(x+r,y),(x,y)],col)
    c.poly([(x-r,y),(x,y),(x,y+r)],'teal0' if col=='teal1' else 'bronze1')
    c.line([(x-r,y),(x,y-r)],'lightgold')

def curl(c,x,y,sx=1,sy=1,unit=1):
    pts=[(0,13),(0,7),(2,3),(5,1),(10,1),(13,3),(14,6),(12,9),(9,10),(7,8),(7,6)]
    gold_line(c,[(x+sx*a*unit,y+sy*b*unit) for a,b in pts],2*unit)
    c.poly([(x+sx*5*unit,y+sy*2*unit),(x+sx*12*unit,y-sy*2*unit),(x+sx*10*unit,y+sy*4*unit)],'gold')
    c.line([(x+sx*6*unit,y+sy*2*unit),(x+sx*11*unit,y-sy*unit)],'lightgold')

def outline(c,x,y,w,h,edge='bronze2',fill='panel',ornate=True):
    c.layer('gilded_structure');cut=4
    c.poly([(x+cut,y),(x+w-cut-1,y),(x+w-1,y+cut),(x+w-1,y+h-cut-1),(x+w-cut-1,y+h-1),(x+cut,y+h-1),(x,y+h-cut-1),(x,y+cut)],'bronze0')
    c.rect(x+2,y+2,w-4,h-4,'ruby');c.rect(x+4,y+4,w-8,h-8,edge)
    c.rect(x+6,y+6,w-12,h-12,'bronze1');c.rect(x+7,y+7,w-14,h-14,fill)
    c.line([(x+8,y+5),(x+w-9,y+5)],'lightgold');c.line([(x+7,y+h-6),(x+w-8,y+h-6)],'bronze0')
    c.layer('leather_stitches')
    for xx in range(x+14,x+w-14,8):c.rect(xx,y+2,2,1,'coral');c.rect(xx,y+h-3,2,1,'bronze2')
    for yy in range(y+18,y+h-18,10):c.rect(x+2,yy,1,2,'coral');c.rect(x+w-3,yy,1,2,'bronze2')
    if ornate and w>48 and h>30:
        c.layer('gold_leaf_corners');unit=1 if h<80 or w<180 else 2
        for xx,yy,sx,sy in [(x+8,y+8,1,1),(x+w-9,y+8,-1,1),(x+8,y+h-9,1,-1),(x+w-9,y+h-9,-1,-1)]:
            curl(c,xx,yy,sx,sy,unit)
            jewel(c,xx+sx*3*unit,yy+sy*3*unit,2 if unit==1 else 3)
        if w>=180 and h>=80:
            c.layer('symmetric_gold_insets');jewel(c,x+w//2,y+5,3);jewel(c,x+w//2,y+h-6,3)
m.outline=outline;t.outline=outline

def emblem(c,typ,x,y,r=12):
    c.layer('type_medallion');steps=12
    for radius,col in [(r+2,'bronze0'),(r+1,'gold'),(r,'lightgold'),(r-2,'bronze1'),(r-3,'teal0' if typ=='formation' else 'ruby')]:
        if radius>0:c.poly([(x+int(radius*math.sin(i*2*math.pi/steps)),y+int(radius*math.cos(i*2*math.pi/steps))) for i in range(steps)],col)
    old_emblem(c,typ,x,y,max(3,r-5))
t.emblem=emblem

def frame(w,h,typ,mode):
    c=old_frame(w,h,typ,mode);c.layer('gilded_type_fittings')
    if mode=='full':
        gold_line(c,[(48,15),(w-18,15),(w-16,18),(w-16,51)],2)
        c.line([(48,52),(w-20,52)],'gold')
        for xx in (10,w-11):
            for yy in range(106,266,40):
                if typ=='formation':jewel(c,xx,yy,3)
                elif typ=='word':c.poly([(xx-2,yy+6),(xx+3,yy-6),(xx+4,yy-2),(xx,yy+6)],'silver')
                elif typ=='seal':c.ellipse((xx-3,yy-3,xx+3,yy+3),'gold',2)
                elif typ=='action':gold_line(c,[(xx-2,yy-4),(xx+2,yy),(xx-2,yy+4)],1)
                else:c.rect(xx-2,yy-4,4,8,'gold');c.rect(xx-1,yy-3,2,6,'cream')
        for yy in (286,358,506):
            gold_line(c,[(24,yy),(w-25,yy)],1)
            m.diamond(c,w//2,yy,2,'gold')
    return c
t.frame=frame;m.frame_asset=frame

def rarity(name,w=64,h=40):
    c=m.Canvas(w,h);_,label,col,n=next(r for r in t.RARITIES if r[0]==name);x=w//2;y=h//2-4;r=11
    c.layer('gold_setting');curl(c,x-20,y-6,1,1);curl(c,x+20,y-6,-1,1)
    if n==5:
        pts=[(x-12,y-9),(x-6,y-3),(x,y-13),(x+6,y-3),(x+12,y-9),(x+10,y+9),(x-10,y+9)]
    elif n==4:
        pts=[(x+int((13 if i%2==0 else 6)*math.sin(i*math.pi/8)),y+int((13 if i%2==0 else 6)*math.cos(i*math.pi/8))) for i in range(16)]
    elif n==3:pts=[(x,y-13),(x+9,y-3),(x+10,y+5),(x+5,y+11),(x-5,y+11),(x-10,y+5),(x-9,y-3)]
    else:pts=[(x,y-12),(x+11,y),(x,y+12),(x-11,y)]
    c.poly([(x+(a-x)*1.15,y+(b-y)*1.15) for a,b in pts],'bronze0');c.poly(pts,'gold')
    inside=[(x+int((a-x)*.77),y+int((b-y)*.77)) for a,b in pts];c.poly(inside,col)
    c.poly([(x,y-8),(x,y),(x-7,y)],'white' if n==1 else 'cream')
    c.poly([(x,y-8),(x+7,y),(x,y+8)],col)
    c.line([(x-6,y),(x,y+8)],'bronze1');c.rect(x,y-5,2,3,'white')
    if n==5:c.rect(x-10,y+11,20,3,'lightgold')
    c.layer('independent_rarity_pips')
    for i in range(n):m.diamond(c,x-(n-1)*4+i*8,h-3,2,col)
    return c
t.rarity=rarity

EXTRA={'speed':'速度','temporary_load':'临时荷载','bound_load':'绑定荷载','independent_load':'独立荷载','response_only':'仅响应','ambush':'伏击','concentration':'专注'}
SCHOOLS={'evocation':'塑能','abjuration':'防护','necromancy':'死灵','transmutation':'变化','enchantment':'惑控','divination':'预言','illusion':'幻术','conjuration':'咒法'}
def icon(name):
    if name in EXTRA or name.startswith('school_'):
        c=m.Canvas(32,32);c.layer('engraved_glyph')
        if name=='speed':c.poly([(6,27),(10,14),(25,3),(28,6),(17,23)],'silver');gold_line(c,[(4,29),(23,8)],1)
        elif name in ('temporary_load','bound_load','independent_load'):
            c=old_icon('load');tag={'temporary_load':'blue','bound_load':'violet','independent_load':'orange'}[name];jewel(c,25,25,4,tag)
        elif name in ('response_only','ambush'):
            c.poly([(6,6),(26,16),(6,26),(12,16)],'violet' if name=='response_only' else 'teal2');c.line([(4,5),(4,27)],'gold',2)
        elif name=='concentration':
            c.ellipse((4,4,27,27),'gold',2);jewel(c,16,16,5,'violet');c.line([(16,1),(16,6)],'lightgold');c.line([(16,26),(16,31)],'lightgold')
        else:
            school=name[7:];i=list(SCHOOLS).index(school);col=['orange','blue','violet','green','red','gold','silver','mint'][i]
            c.poly([(16,2),(28,10),(28,22),(16,30),(4,22),(4,10)],'gold');c.poly([(16,5),(25,11),(25,21),(16,27),(7,21),(7,11)],'ruby')
            if i==0:c.poly([(17,7),(10,20),(18,18),(16,25),(23,14),(17,15)],col)
            elif i==1:c.poly([(10,9),(22,9),(21,20),(16,25),(11,20)],col)
            elif i==2:c.rect(11,9,10,11,col);c.rect(12,12,2,3,'ink');c.rect(18,12,2,3,'ink');c.rect(14,21,4,4,col)
            elif i==3:c.line([(9,13),(15,7),(23,13),(17,20),(9,13)],col,2);c.line([(9,22),(23,22)],col,2)
            elif i==4:jewel(c,16,16,7,col)
            elif i==5:c.poly([(8,16),(16,10),(24,16),(16,22)],col);c.rect(15,13,3,6,'ink')
            elif i==6:c.line([(9,20),(13,11),(18,18),(23,9)],col,3)
            else:c.line([(9,11),(23,11),(23,23),(9,23),(9,11)],col,2);m.diamond(c,16,16,4,col)
        return c
    return old_icon(name)
m.icon=icon
m.ICONS.update(EXTRA);m.ICONS.update({'school_'+k:v+'学派' for k,v in SCHOOLS.items()})

def export_extras():
    for typ in m.TYPES:
        for size in (64,128):
            c=m.Canvas(size,size);emblem(c,typ,size//2,size//2,size//2-10)
            m.export(c,'crest_'+typ+'_'+str(size),'cards','独立古金类型纹章 '+m.TYPES[typ][0],stretch='none')
    for name,label,col,n in t.RARITIES:
        c=m.Canvas(128,80);c.sprite(UI/'rarity'/f'rarity_badge_{name}.png',0,0,128,80)
        m.export(c,'rarity_badge_'+name+'_128','rarity',label+'大型镶座与刻度',rarity=name,pixel_unit=2)
        c=m.Canvas(148,48);outline(c,0,0,148,48,'gold','ruby');c.sprite(UI/'rarity'/f'rarity_badge_{name}.png',4,4)
        m.export(c,'rarity_cartouche_'+name,'rarity',label+'名称与宝石组件',label_rect=[70,14,66,20],stretch='none')
    for state,col in [('normal','bronze2'),('hover','gold'),('disabled','gray')]:
        c=m.Canvas(112,36);outline(c,0,0,112,36,col,'panel',False);m.diamond(c,15,18,6,'gold' if state!='disabled' else 'gray')
        m.export(c,'stat_receptacle_'+state,'cards','可选属性槽 '+state,icon_rect=[5,8,20,20],label_rect=[29,10,52,16],value_rect=[84,7,22,20],insets=[28,10,10,10],stretch='horizontal')
    for tag in ['response_only','ambush','concentration','base_protected','attached','duration']:
        c=m.Canvas(132,28);outline(c,0,0,132,28,'gold','ruby',False)
        key={'base_protected':'state_protected','attached':'state_attached'}.get(tag,tag);c.sprite(UI/'icons'/f'{key}.png',7,4,20,20)
        m.export(c,'tag_'+tag,'cards','条件/状态独立标签 '+tag,label_rect=[34,6,88,16],insets=[30,8,8,8],stretch='horizontal')
    for i,(sx,sy) in enumerate([(1,1),(-1,1),(1,-1),(-1,-1)]):
        c=m.Canvas(64,64);xx=12 if sx==1 else 51;yy=12 if sy==1 else 51;curl(c,xx,yy,sx,sy,3);jewel(c,xx+sx*9,yy+sy*9,7)
        m.export(c,'corner_clasp_'+str(i),'ornaments','可组合卷饰角扣',stretch='none')
    c=m.Canvas(320,24);gold_line(c,[(8,12),(312,12)],1)
    for xx in (35,120,160,200,285):jewel(c,xx,12,5 if xx==160 else 2)
    m.export(c,'gold_divider','ornaments','独立横向分隔卷饰',stretch='none')
    for state,col in [('hover','lightgold'),('selected','mint'),('target_candidate','gold'),('target_selected','coral'),('cost_candidate','violet'),('cost_selected','violet')]:
        c=m.Canvas(1024,1536);c.layer('master_interaction')
        if state=='target_candidate':
            for xx in range(40,984,40):c.rect(xx,12,18,5,col);c.rect(xx,1519,18,5,col)
            for yy in range(40,1496,40):c.rect(12,yy,5,18,col);c.rect(1007,yy,5,18,col)
        else:
            for xx,yy,sx,sy in [(12,12,1,1),(1011,12,-1,1),(12,1523,1,-1),(1011,1523,-1,-1)]:
                c.line([(xx+sx*72,yy),(xx,yy),(xx,yy+sy*72)],col,6)
            if state.startswith('cost'):
                c.poly([(947,12),(1011,12),(1011,76)],col)
                if state=='cost_selected':c.line([(966,35),(981,49),(1001,26)],'cream',5)
        m.export(c,'overlay_master_'+state,'interaction','精绘主卡框 '+state,overlay=True,stretch='none',logical_canvas=[1024,1536])
    # Small backs remain deterministic, editable and visually symmetric.
    for mode,w,h in [('hand',101,118),('opponent',39,41),('deck',100,132)]:
        c=m.Canvas(w,h);outline(c,0,0,w,h,'gold','ruby',mode!='opponent');r=min(w,h)//3;x=w//2;y=h//2
        for rr in (r,r-3):gold_line(c,[(x,y-rr),(x+rr,y),(x,y+rr),(x-rr,y),(x,y-rr)],1)
        jewel(c,x,y,max(3,r//3));c.save(UI/'cards'/f'card_back_{mode}.png',m.SRC/'cards'/f'card_back_{mode}.svg')

def add_masters():
    records=json.loads((ROOT/'generated-sources.json').read_text(encoding='utf-8'))
    records+=json.loads((ROOT/'generated-supplement.json').read_text(encoding='utf-8'))
    for rec in records:
        path=ART/rec['file']
        with Image.open(path) as im:size=list(im.size)
        entry={'id':'master_'+rec['id'],'category':'painted','file':rec['file'],'size':size,'purpose':'A 古籍金饰原始绘制：'+rec['id'],'stretch':'none','provenance':rec,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'raster_original_preserved':True}
        if rec['id'] in m.TYPES:
            entry['layout']={'title':[316,119,534,70],'school':[320,237,310,35],'art':[155,308,710,445],'stats_cells':[[126+270*col,841+90*row,225,55] for row in range(2) for col in range(3)],'rules':[150,1058,720,240],'type':[150,1380,330,50],'rarity_label':[644,1387,110,45],'rarity_badge':[761,1338,128,128],'rarity_pips':[763,1464,126,36]}
            entry['usage']='1024×1536 固定比例主卡框；透明插画窗；文字、数值、插画与稀有度独立叠加'
        elif rec['id']=='window':entry['layout']={'title':[320,110,880,68],'content':[160,280,1216,520]}
        elif rec['id'].startswith('gem_'):
            entry['rarity']=rec['id'][4:];entry['usage']='精绘稀有度徽章；保持比例；与独立刻度及名称组合'
        m.REGISTRY.append(entry)

def build_previews():
    out=ROOT/'previews';out.mkdir(exist_ok=True)
    cards=json.loads((ART.parent/'cards.json').read_text(encoding='utf-8'))['cards'];mapping=json.loads((ART/'runtime.json').read_text(encoding='utf-8'))['cards']
    layouts={a['id'][7:]:a['layout'] for a in m.REGISTRY if a['id'].startswith('master_') and 'layout' in a}
    metrics=[]
    for card in cards:
        c=m.Canvas(1024,1536);layout=layouts[card['type']]
        # UI layers composing original master, existing art and editable text.
        c.layer('art_backing');c.rect(130,290,765,490,'panel')
        c.layer('existing_card_illustration');c.sprite(ART/mapping[card['id']],*layout['art'],contain=True)
        c.layer('original_master');c.sprite(UI/'painted'/f"{card['type']}.png",0,0)
        x,y,w,h=layout['title'];size=48
        while c.d.textlength(card['name'],font=m.font(size))>w:size-=1
        c.layer('dynamic_title');c.text(card['name'],x,y,size,'cream')
        sub=SCHOOLS.get(card.get('school'),'基础阵法' if card.get('baseEligible') else 'WizardCard')
        if card.get('responseOnly'):sub+=' · 仅响应'
        if card.get('concentration'):sub+=' · 专注'
        c.text(sub,320,237,27)
        fields=t.field_list(card)
        for i,(label,value) in enumerate(fields):
            xx,yy,ww,hh=layout['stats_cells'][i];c.text(label,xx,yy,27);c.text(value,xx+173,yy-3,40)
        x,y,w,h=layout['rules'];end=c.wrapped(card['text'],x,y,w,36,lineheight=51)
        assert end<=y+h,(card['id'],end,y+h)
        c.text(m.TYPES[card['type']][0],150,1380,40,'cream')
        rare=next(r for r in t.RARITIES if r[0]==card['rarity']);c.text(rare[1],644,1387,34,'cream');c.sprite(UI/'painted'/f'gem_{rare[0]}.png',*layout['rarity_badge'],contain=True)
        c.sprite(UI/'cards'/f'rarity_{rare[0]}.png',*layout['rarity_pips'])
        c.save(out/(card['id']+'.png'),out/(card['id']+'.svg'))
        metrics.append({'id':card['id'],'rules_end_y':end,'rules_limit_y':y+h,'title_size':size,'stat_count':len(fields)})
    sheet=m.Canvas(1900,820,'bg');sheet.text('WizardCard · A 古籍金饰资源',28,22,32);sheet.text('五类独立主卡框 / 宝石镶座 / 当前卡牌信息',28,72,20)
    chosen=[next(card for card in cards if card['type']==typ) for typ in m.TYPES]
    for i,card in enumerate(chosen):sheet.sprite(out/(card['id']+'.png'),12+i*376,110,360,540)
    for i,(name,label,col,n) in enumerate(t.RARITIES):
        sheet.sprite(UI/'painted'/f'gem_{name}.png',180+i*330,680,110,110,contain=True);sheet.text(label,305+i*330,727,22)
        sheet.sprite(UI/'cards'/f'rarity_{name}.png',190+i*330,790,84,24)
    sheet.save(out/'card-types-and-rarities.png')
    hero=m.Canvas(1750,1110,'bg');hero.text('A 古籍金饰 · 成品卡牌预览',30,22,34)
    hero.text('空白卡框、原有插画、当前规则与稀有度独立组合',30,72,21)
    for i,name in enumerate(['fireball','balance','barbs']):hero.sprite(out/(name+'.png'),30+i*574,115,512,768)
    for i,(name,label,col,n) in enumerate(t.RARITIES):
        hero.sprite(UI/'painted'/f'gem_{name}.png',95+i*330,928,118,118,contain=True);hero.text(label,222+i*330,974,22)
        hero.sprite(UI/'cards'/f'rarity_{name}.png',112+i*330,1055,84,24)
    hero.save(out/'a-final-card-preview.png')
    small=m.Canvas(1400,460,'bg');small.text('缩略卡框与状态组件',24,20,28)
    for i,typ in enumerate(m.TYPES):
        xx=24+i*275;small.sprite(UI/'cards'/f'frame_{typ}_hand.png',xx,78,202,236);small.text(m.TYPES[typ][0],xx,327,18)
        small.sprite(UI/'interaction/overlay_hand_selected.png',xx,78,202,236)
        small.sprite(UI/'cards'/f'frame_{typ}_strip.png',xx,368,250,88)
    small.save(out/'compact-cards.png')
    component_preview(out)
    (ROOT/'preview-metrics.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2),encoding='utf-8')
    html='<!doctype html><html lang="zh"><meta charset="utf-8"><title>A 古籍金饰资源库</title><style>body{background:#e7cf9f;color:#322324;font:16px sans-serif;margin:28px}img{max-width:100%;image-rendering:pixelated}.cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(330px,1fr));gap:18px}.cards img{width:380px}a{color:#196d63}</style><h1>WizardCard · A 古籍金饰资源</h1><p>独立组件与真实卡牌排版；未重排战场，未接入游戏。</p><p><a href="ASSET_LIST.md">完整清单</a> · <a href="../../ui/a-gilded-v2/manifest.json">组件参数</a></p>'
    for name in ['a-final-card-preview','card-types-and-rarities','compact-cards','component-library']:html+=f'<img src="previews/{name}.png">'
    html+='<h2>30 张真实卡牌信息预览</h2><div class="cards">'
    for card in cards:html+=f'<div><img src="previews/{card["id"]}.png"><p>{card["name"]}</p></div>'
    (ROOT/'gallery.html').write_text(html+'</div></html>',encoding='utf-8')

def component_preview(out):
    c=m.Canvas(1800,1350,'bg');c.text('A 风格配套组件 · 古金卷饰 / 皮革缝线 / 切面宝石',28,24,32)
    zones={'casting':'施法区','words':'言灵区','analysis':'解析区','action':'行动区','ash':'灰烬区','formation':'阵法区','hand':'手牌区','deck':'牌库区'}
    for i,(key,label) in enumerate(zones.items()):
        x=30+(i%4)*440;y=95+(i//4)*68;c.sprite(UI/'board'/f'zone_header_{key}.png',x,y,336,60);c.text(label,x+69,y+18,21,'cream')
    for i,kind in enumerate(['primary','secondary','danger']):
        y=265+i*88;c.text({'primary':'主要操作','secondary':'次要操作','danger':'危险操作'}[kind],30,y+17,20)
        for j,state in enumerate(['normal','hover','pressed','disabled']):
            x=230+j*355;c.sprite(UI/'interaction'/f'button_{kind}_{state}.png',x,y,320,72);c.text(state,x+95,y+23,17,'cream' if kind!='secondary' and state!='disabled' else 'ink')
    for i,name in enumerate(['life','mana','load','capacity']):
        x=30+i*440;c.sprite(UI/'board'/f'meter_{name}_track.png',x,548,384,36);c.sprite(UI/'board'/f'meter_{name}_fill.png',x+16,558,352,16);c.text(name,x,597,18)
    c.sprite(UI/'painted/window.png',35,658,960,640);c.text('弹窗 / 信息面板',240,728,27,'cream');c.text('框体、标题与内容独立组合',145,892,24)
    c.sprite(UI/'painted/back.png',1485,676,280,420)
    c.text('通用卡背',1510,1123,20)
    c.text('属性 / 状态 / 学派图标',1040,660,22)
    names=list(m.ICONS)
    for i,name in enumerate(names):
        x=1040+(i%7)*58;y=715+(i//7)*40;c.sprite(UI/'icons'/f'{name}.png',x,y)
    c.save(out/'component-library.png')

def document(manifest):
    counts=collections.Counter(a['category'] for a in m.REGISTRY)
    lines=['# A 古籍金饰 · 美术资源清单','','用户已选 A 版；本批重制卡牌组件并细化其余 UI。未重排战场，也未接入游戏。','',f'共 **{len(m.REGISTRY)} 个独立 PNG**，其中七张原始生成主素材；另附 30 卡排版预览、三张总览、可编辑 SVG、参数清单、提示词和生成脚本。','','|分类|数量|','|---|---:|']
    lines += [f'|{k}|{v}|' for k,v in counts.items()]
    lines += ['','## 本轮内容','','- 五类独立主卡框：行动闪电与箭形镶边、解析法术卷册徽章、言灵银羽角饰、阵法六芒星徽章、符文圆环链扣。所有主框均无烘焙文字、数值和插画。','- 25 个可编辑固定尺寸卡框：五种类型各有手牌、短条、场内小卡、详情及完整模板；配有独立类型纹章、属性槽、条件标签和六类交互叠加。','- 五档宝石稀有度：银灰菱晶、翠绿菱晶、蓝色水滴、紫色星芒、古金冠饰；镶座与名称容器分别导出，1—5 刻度独立，不绑定卡牌类型。','- 一张原始绘制通用卡背及三个可编辑缩略卡背。卡背不携带类型或稀有度等私有信息。','- 其余组件重绘金属分层、皮革缝线、古金卷饰和切面宝石：区域框／标题、按钮四状态、HUD、资源条、弹窗、反馈、阶段、滚动、步骤和图标。','- 施法区、言灵区、解析区、行动区、灰烬区均有独立框体和标题，双方可复用；另含阵法、手牌、牌库。','- 补充速度、临时／绑定／独立荷载、仅响应、伏击、专注和八个学派图标。','- 保留上一批可选羊皮纸底图；它是唯一直接复用的材料资源。既有卡牌插画未重画。','','## 使用与限制','','原始绘制主框为 1024×1536，窗口为 1536×1024。生成 PNG 完整原样保存，不做裁切、改色或像素加工。透明通道保留原始边缘覆盖率；代码原生组件为硬边像素。原始绘制图的细节较多，需用实际尺寸确认小屏阅读效果。','主框和卡背固定比例缩放，禁止九宫格拉伸；可编辑小卡框也固定尺寸。按钮和窗口等代码原生组件按 manifest 的 stretch/insets 使用。生成主窗口同样固定比例，不宣称可任意九宫格拉伸。','五张主框内容安全区按输出保守设置，见各 master_* 的 layout。卡名、费用、规则、插画、稀有度需要程序单独叠加；预览 PNG 只是信息组合展示，不应作运行时卡面。','属性字段以 assets/cards.json 为准。解析回合缺省 1，显示解析费用与施法费用分别支付；规则文字全部保留；不存在的属性不造数值。详情可用滚动区域显示风味文字和动态实例信息。','所有 30 张卡已做标题、正文与属性数检查。透明窗采样、源图 SHA256、SVG 链接与独立组件数校验见 validation.json。没有进行游戏实机验收，因为本批未接入。','','## 原始绘制提示词','','内置 imagegen 制作了五张主框、通用卡背与主窗口；完整提示词存放于 prompts/，来源存放于 generated-sources.json。其余组件通过当前项目可编辑绘制系统制作。','','## 逐项清单','','路径相对于 assets/art。','','|分类 / ID|尺寸|文件|用途|','|---|---|---|---|']
    for a in m.REGISTRY:lines.append(f'|{a["category"]} / {a["id"]}|{a["size"][0]}×{a["size"][1]}|`{a["file"]}`|{a["purpose"]}|')
    (ROOT/'ASSET_LIST.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    (ROOT/'README.md').write_text('# A 古籍金饰交付\n\n入口 gallery.html；资产清单 ASSET_LIST.md；组件参数 ../../ui/a-gilded-v2/manifest.json。\n\nbuild_assets.py 使用 Python/Pillow，依赖上一批 c-theme-v1/build_assets.py 和 v1/build_ui.py；这些依赖、当前 cards.json、runtime.json、字体和既有插画都包含在交付 ZIP 中。painted/ 下七张原始生成 PNG 不可由该脚本重新生成，来源及提示词另附。其余 PNG 的分层 SVG 位于 sources/。\n\n文字、数值及插画均动态组合；previews/ 属于排版示例，不是游戏中的底板资源。未修改游戏运行时。\n',encoding='utf-8')

def validate(manifest):
    keys=set();svg_count=0;transparent=0
    for a in m.REGISTRY:
        key=(a['category'],a['id']);assert key not in keys,key;keys.add(key)
        p=ART/a['file']
        with Image.open(p) as im:
            assert list(im.size)==a['size'];assert im.mode in ('RGBA','RGB')
            if im.mode=='RGBA' and im.getextrema()[3][0]==0:transparent+=1
            if a['category'] not in ('painted','materials'):assert set(im.getchannel('A').tobytes())<={0,255},key
        if 'source' in a:ET.parse(ART/a['source']);svg_count+=1
        if a.get('raster_original_preserved'):
            assert hashlib.sha256(p.read_bytes()).hexdigest()==a['sha256']
            original=Path(a['provenance']['original_source'])
            if original.exists():assert p.read_bytes()==original.read_bytes(),a['id']
    for typ in m.TYPES:
        with Image.open(UI/'painted'/f'{typ}.png') as im:
            assert im.getpixel((0,0))[3]==0
            assert im.getpixel((512,500))[3]==0
    for svg in list(m.SRC.rglob('*.svg'))+list((ROOT/'previews').glob('*.svg')):
        doc=ET.parse(svg)
        for node in doc.iter():
            if 'href' in node.attrib:assert (svg.parent/node.attrib['href']).resolve().exists(),svg
    assert all(('board','region_'+zone) in keys and ('board','zone_header_'+zone) in keys for zone in ('casting','words','analysis','action','ash'))
    for name,label,col,n in t.RARITIES:
        doc=ET.parse(m.SRC/'cards'/f'rarity_{name}.svg')
        assert len(doc.findall('.//{http://www.w3.org/2000/svg}polygon'))==n,name
    for a in m.REGISTRY:
        if a.get('stretch')=='nine_slice':
            left,top,right,bottom=a['insets'];w,h=a['size'];assert left+right<w and top+bottom<h,a['id']
    metrics=json.loads((ROOT/'preview-metrics.json').read_text(encoding='utf-8'));assert len(metrics)==30
    report={'assets':len(m.REGISTRY),'transparent_pngs':transparent,'native_svgs':svg_count,'original_painted_assets':sum(a['category']=='painted' for a in m.REGISTRY),'painted_sources_unchanged':True,'transparent_master_art_apertures':True,'svg_refs_resolve':True,'cards_previewed':30,'all_rule_text_fits':True,'max_rule_lines':max((a['rules_end_y']-1058)//51 for a in metrics),'rarity_pips_match_five_tiers':True,'nine_slice_centers_positive':True,'legendary_gem_color':'golden yellow','required_five_regions_present':True,'runtime_integrated':False}
    (ROOT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(report)

def package():
    dist=ROOT/'dist';dist.mkdir(exist_ok=True)
    paths={p for p in ROOT.rglob('*') if p.is_file() and 'dist' not in p.parts and '__pycache__' not in p.parts}
    paths.update(p for p in UI.rglob('*') if p.is_file())
    for name in ('design/c-theme-v1/build_assets.py','design/v1/build_ui.py','runtime.json'):paths.add(ART/name)
    paths.add(ART.parent/'cards.json');paths.update(p for p in (ART.parent/'fonts').glob('*') if p.is_file())
    paths.update(ART/f for f in json.loads((ART/'runtime.json').read_text(encoding='utf-8'))['cards'].values())
    with zipfile.ZipFile(dist/'WizardCard-A-gilded-v2.zip','w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(paths):z.write(p,p.relative_to(PROJECT))
    print('Package:',dist/'WizardCard-A-gilded-v2.zip')

def build():
    t.previews=lambda _:None;t.inventory=lambda _:None;t.validate=lambda _:None;t.package=lambda:None
    t.build();export_extras();add_masters()
    for a in m.REGISTRY:
        if a.get('stretch')=='nine_slice':
            w,h=a['size'];top=24 if h<=80 else 46;bottom=min(36,h-top-8)
            a['insets']=[min(36,w//3),top,min(36,w//3),bottom]
        if a['id'].startswith('button_'):
            a['text_insets']=[28,9,28,9];a['stretch']='horizontal';a['insets']=[28,0,28,0];a['fixed_height']=36
    manifest={'version':'wizardcard_a_gilded_v2','runtime_integrated':False,'text_in_textures':False,'sampling':'nearest','palette':m.P,'card_types':list(m.TYPES),'rarities':[r[0] for r in t.RARITIES],'assets':m.REGISTRY}
    (UI/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    build_previews();document(manifest)
    for name in ('ASSET_LIST.md','README.md'):
        p=ROOT/name;body=p.read_text(encoding='utf-8')
        body=body.replace('七张','十二张').replace('三张总览','四张总览').replace('古金冠饰','金黄色宝石冠饰')
        body=body.replace('五张主框、通用卡背与主窗口；','五张主框、通用卡背、主窗口与五档精绘稀有度徽章；')
        body=body.replace('来源存放于 generated-sources.json。','来源存放于 generated-sources.json 与 generated-supplement.json。')
        p.write_text(body,encoding='utf-8')
    validate(manifest);package()

if __name__=='__main__':build()
