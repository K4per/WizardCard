"""Deterministic pixel-native UI sources + separate PNG/SVG exports.

Original UI drawing only. Existing generated card illustrations are not modified.
Run with the bundled Python/Pillow runtime documented in README.md.
"""
from pathlib import Path
from functools import lru_cache
from hashlib import sha256
from html import escape
import json, math, os, re
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
ART = ROOT.parent.parent
ASSETS = ART.parent
UI = ART / 'ui'
SRC = ROOT / 'sources'
P = dict(bg='#0A1119', ink='#101923', panel='#122029', raised='#192D38', fill='#263E49', muted='#364E59',
         bronze0='#44352B', bronze1='#6D5035', bronze2='#9B7448', gold='#C49A5A', lightgold='#E0BE7C', cream='#F2DBA0',
         teal0='#174A4B', teal1='#247475', teal2='#3CA6A0', mint='#72D7C5', pale='#B6F0DD',
         gray='#68778E', silver='#A4B4CB', white='#DCE6EB', text='#E7E5D5', ruby='#703244', red='#B74C60', coral='#ED8A78',
         fire0='#B54E2A', orange='#ED8E3E', yellow='#FFD181', violet='#AD91D8', blue='#629ED2', green='#78BE98')
TYPES = {'action':('行动','orange'), 'analytic':('解析法术','blue'), 'word':('言灵','violet'), 'formation':('阵法','green'), 'seal':('符文','lightgold')}
REGISTRY = []

@lru_cache(None)
def font(size):
    return ImageFont.truetype(str(ASSETS/'fonts/NotoSansCJKsc-Regular.otf'), size)

def color(c): return P.get(c,c)

class Canvas:
    def __init__(self,w,h,fill=None):
        self.w,self.h=w,h
        self.im=Image.new('RGBA',(w,h),color(fill) if fill else (0,0,0,0))
        self.d=ImageDraw.Draw(self.im)
        self.layers={}; self.layer('base')
        if fill:self.rect(0,0,w,h,fill)
        self.refs={}
    def layer(self,name): self.current=self.layers.setdefault(name,[]);return self
    def rect(self,x,y,w,h,c):
        if w<=0 or h<=0:return
        c=color(c);self.d.rectangle((x,y,x+w-1,y+h-1),fill=c)
        self.current.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{c}"/>')
    def poly(self,pts,c):
        pts=[(int(x),int(y)) for x,y in pts];c=color(c);self.d.polygon(pts,fill=c)
        self.current.append(f'<polygon points="{" ".join(f"{x},{y}" for x,y in pts)}" fill="{c}"/>')
    def line(self,pts,c,width=1):
        pts=[(int(x),int(y)) for x,y in pts];c=color(c);self.d.line(pts,fill=c,width=width,joint='curve')
        self.current.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in pts)}" fill="none" stroke="{c}" stroke-width="{width}"/>')
    def ellipse(self,box,c,width=1,fill=False):
        box=tuple(map(int,box));c=color(c)
        self.d.ellipse(box,fill=c if fill else None,outline=None if fill else c,width=width)
        x,y,x2,y2=box
        self.current.append(f'<ellipse cx="{(x+x2)/2}" cy="{(y+y2)/2}" rx="{(x2-x)/2}" ry="{(y2-y)/2}" fill="{c if fill else "none"}" stroke="{c}" stroke-width="{width}"/>')
    def text(self,s,x,y,size=16,c='text'):
        self.d.text((x,y),s,font=font(size),fill=color(c),anchor='lt')
        self.current.append(f'<text x="{x}" y="{y}" dominant-baseline="text-before-edge" font-family="Noto Sans CJK SC,Microsoft YaHei,sans-serif" font-size="{size}" fill="{color(c)}" shape-rendering="auto">{escape(str(s))}</text>')
    def wrapped(self,s,x,y,width,size=14,c='text',lineheight=None,maxlines=None):
        lineheight=lineheight or int(size*1.5); lines=[];line=''
        for ch in str(s):
            if ch=='\n' or self.d.textlength(line+ch,font=font(size))>width:
                lines.append(line);line='' if ch=='\n' else ch
            else:line+=ch
        if line:lines.append(line)
        if maxlines and len(lines)>maxlines:lines=lines[:maxlines];lines[-1]=lines[-1][:-1]+'…'
        for n,line in enumerate(lines):self.text(line,x,y+n*lineheight,size,c)
        return y+len(lines)*lineheight
    def sprite(self,file,x,y,w=None,h=None,contain=False):
        file=Path(file);src=Image.open(file).convert('RGBA');w=w or src.width;h=h or src.height
        if contain:
            scale=min(w/src.width,h/src.height);nw=max(1,round(src.width*scale));nh=max(1,round(src.height*scale));x+=int((w-nw)/2);y+=int((h-nh)/2);w,h=nw,nh
        self.im.alpha_composite(src.resize((w,h),Image.Resampling.NEAREST),(int(x),int(y)))
        key=f'__REF_{len(self.refs)}__';self.refs[key]=file
        self.current.append(f'<image href="{key}" x="{x}" y="{y}" width="{w}" height="{h}" style="image-rendering:pixelated"/>')
    def save(self,png,svg=None):
        png=Path(png);png.parent.mkdir(parents=True,exist_ok=True);self.im.save(png)
        if svg:
            svg=Path(svg);svg.parent.mkdir(parents=True,exist_ok=True)
            s=f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" viewBox="0 0 {self.w} {self.h}" shape-rendering="crispEdges"><title>{escape(png.stem)}</title>'
            s+=''.join(f'<g id="{escape(k)}">{"".join(v)}</g>' for k,v in self.layers.items())+'</svg>'
            for key,file in self.refs.items():s=s.replace(key,os.path.relpath(file,svg.parent).replace('\\','/'))
            svg.write_text(s,encoding='utf-8')

def diamond(c,x,y,r,fill='gold'):
    c.poly([(x,y-r),(x+r,y),(x,y+r),(x-r,y)],fill)

def outline(c,x,y,w,h,edge='bronze2',fill='panel',ornate=True):
    c.layer('body');c.poly([(x+4,y),(x+w-5,y),(x+w-1,y+4),(x+w-1,y+h-5),(x+w-5,y+h-1),(x+4,y+h-1),(x,y+h-5),(x,y+4)],'ink')
    c.poly([(x+4,y+1),(x+w-5,y+1),(x+w-2,y+4),(x+w-2,y+h-5),(x+w-5,y+h-2),(x+4,y+h-2),(x+1,y+h-5),(x+1,y+4)],edge)
    c.poly([(x+5,y+3),(x+w-6,y+3),(x+w-4,y+5),(x+w-4,y+h-6),(x+w-6,y+h-4),(x+5,y+h-4),(x+3,y+h-6),(x+3,y+5)],fill)
    c.layer('bevel');c.line([(x+6,y+4),(x+w-7,y+4)],'raised');c.line([(x+5,y+h-5),(x+w-6,y+h-5)],'ink')
    if ornate:
        c.layer('corner_inlays')
        for xx,yy,sx,sy in [(x+3,y+3,1,1),(x+w-4,y+3,-1,1),(x+3,y+h-4,1,-1),(x+w-4,y+h-4,-1,-1)]:
            c.line([(xx,yy+8*sy),(xx,yy),(xx+8*sx,yy)],'gold',2)
            c.rect(xx if sx>0 else xx-1,yy if sy>0 else yy-1,2,2,'lightgold')

def export(c,name,category,purpose,**meta):
    # Each independently drawn asset has the same on-disk contract read first.
    (ROOT/'spec_lock.md').read_text(encoding='utf-8')
    png=UI/category/f'{name}.png';svg=SRC/category/f'{name}.svg';c.save(png,svg)
    REGISTRY.append(dict(id=name,category=category,file=png.relative_to(ART).as_posix(),source=svg.relative_to(ART).as_posix(),size=[c.w,c.h],purpose=purpose,**meta))
    return png

def icon(name):
    c=Canvas(32,32);c.layer('silhouette')
    if name in ('life','life_core'):
        pts=[(3,7),(8,3),(13,3),(16,7),(19,3),(24,3),(29,7),(29,16),(16,29),(3,16)]
        c.poly(pts,'ruby');c.poly([(5,8),(9,5),(12,5),(16,10),(20,5),(23,5),(27,9),(26,16),(16,26),(6,16)],'red');c.poly([(6,9),(10,6),(13,9),(9,15),(6,13)],'coral')
    elif name in ('mana','mana_income'):
        c.poly([(16,2),(27,12),(24,25),(16,30),(8,25),(5,12)],'teal1');c.poly([(16,4),(23,12),(16,27),(9,12)],'mint');c.poly([(16,4),(16,27),(23,12)],'teal2')
        if name=='mana_income':c.line([(3,3),(3,16),(8,12)],'lightgold',3)
    elif name=='load':
        c.rect(10,3,12,8,'bronze2');c.rect(13,5,6,4,'ink');c.poly([(9,10),(23,10),(29,28),(3,28)],'bronze1');c.poly([(11,12),(21,12),(25,25),(7,25)],'gold');c.line([(12,14),(10,23)],'cream',2)
    elif name=='capacity':
        c.line([(4,27),(4,15),(8,7),(12,4),(20,4),(24,7),(28,15),(28,27)],'teal2',4);c.line([(6,25),(6,14),(10,8),(14,6),(19,6),(23,9)],'mint',2);c.rect(1,27,9,3,'gold');c.rect(22,27,9,3,'gold')
    elif name in ('type_action','zone_action','cast_cost'):
        c.poly([(17,2),(5,18),(14,18),(10,30),(28,12),(18,12),(23,2)],'orange');c.poly([(17,6),(10,15),(18,15),(16,22),(23,15),(15,15)],'yellow')
        if name=='cast_cost':diamond(c,25,25,5,'blue')
    elif name in ('type_analytic','zone_analysis','analysis_cost','log'):
        c.poly([(3,5),(13,6),(16,9),(19,6),(29,5),(29,26),(19,26),(16,29),(13,26),(3,26)],'silver');c.poly([(5,8),(12,9),(14,11),(14,24),(6,23)],'raised');c.poly([(18,11),(21,9),(27,8),(26,23),(18,24)],'teal1');c.line([(16,10),(16,26)],'blue',2)
        if name=='analysis_cost':diamond(c,25,25,5,'blue')
    elif name in ('type_word','zone_words'):
        for j in range(3):c.line([(3+4*j,24-6*j),(10+4*j,18-6*j),(24,16-6*j),(29,10-3*j)],'violet',3)
    elif name in ('type_formation','zone_formation'):
        c.line([(16,3),(29,25),(3,25),(16,3)],'green',2);c.line([(3,8),(29,8),(16,30),(3,8)],'gold',2);diamond(c,16,16,3,'mint')
    elif name in ('type_seal','zone_casting','ring_slot'):
        c.ellipse((4,4,27,27),'gold',3);c.ellipse((8,8,23,23),'teal2',2)
        for x,y in [(16,3),(28,16),(16,28),(3,16)]:diamond(c,x,y,2,'lightgold')
        if name=='zone_casting':c.poly([(15,9),(11,19),(19,19),(16,25),(23,15),(17,15),(21,9)],'orange')
        if name=='ring_slot':c.rect(12,13,7,5,'panel')
    elif name=='rank':
        for y in (5,13,21):c.line([(5,y+5),(16,y),(27,y+5)],'lightgold',3)
    elif name=='body':
        for x in (3,17):c.rect(x,6,12,20,'bronze2');c.rect(x+2,8,8,16,'panel')
        c.rect(11,14,10,4,'green')
    elif name in ('duration','state_analyzing'):
        c.rect(7,3,18,3,'gold');c.rect(7,27,18,3,'gold');c.line([(9,7),(23,7),(22,11),(11,23),(9,26),(23,26),(20,20),(10,11),(9,7)],'silver',2);c.poly([(11,8),(21,8),(16,14)],'blue' if name=='state_analyzing' else 'gold');c.poly([(11,25),(21,25),(16,20)],'gold')
    elif name in ('state_analyzed','confirm','success'):
        c.poly([(3,17),(8,12),(14,18),(26,5),(30,10),(14,29)],'mint');c.line([(7,18),(14,24),(27,9)],'pale',2)
    elif name=='state_prepared':
        c.ellipse((3,3,28,28),'blue',2);c.poly([(12,8),(24,16),(12,24)],'lightgold')
    elif name=='state_active':
        c.line([(5,11),(11,5),(24,5),(28,11),(26,16)],'mint',3);c.poly([(22,13),(30,13),(29,21)],'mint');c.line([(27,23),(21,28),(9,28),(4,23),(5,18)],'mint',3);c.poly([(1,19),(10,19),(3,11)],'mint')
    elif name=='state_cancelled':
        c.ellipse((4,4,27,27),'silver',2);c.rect(13,1,7,7,'ink');c.line([(9,10),(22,23)],'coral',3);c.rect(14,13,3,5,'silver')
    elif name=='state_protected':
        c.poly([(3,5),(16,2),(29,5),(27,21),(16,30),(5,21)],'gold');c.poly([(7,8),(16,5),(25,8),(23,20),(16,25),(9,20)],'teal0');c.line([(16,8),(16,22)],'mint',3)
    elif name=='state_attached':
        c.ellipse((2,10,19,27),'gold',3);c.ellipse((12,2,29,19),'mint',3);c.line([(11,20),(22,9)],'lightgold',2)
    elif name=='zone_ash':
        c.poly([(5,28),(9,20),(14,18),(13,7),(19,16),(23,12),(26,23),(28,28)],'gray');c.poly([(11,26),(17,16),(19,24),(23,26)],'silver')
    elif name in ('zone_hand','zone_deck','save'):
        for x,y in [(4,9),(8,5),(12,2)]:c.rect(x,y,16,22,'gold');c.rect(x+2,y+2,12,18,'panel')
        if name=='save':c.rect(17,6,6,6,'mint')
    elif name in ('close','cancel','rejected'):
        c.line([(7,7),(25,25)],'coral' if name=='rejected' else 'silver',3);c.line([(25,7),(7,25)],'coral' if name=='rejected' else 'silver',3)
    elif name in ('left','right','up','down'):
        pts=[(20,5),(9,16),(20,27)]
        if name=='right':pts=[(32-x,y) for x,y in pts]
        elif name=='up':pts=[(y,x) for x,y in pts]
        elif name=='down':pts=[(y,32-x) for x,y in pts]
        c.line(pts,'lightgold',3)
    elif name=='surrender':
        c.rect(7,3,3,28,'silver');c.poly([(10,4),(27,4),(23,11),(27,18),(10,18)],'red')
    elif name=='no_action':
        c.ellipse((3,3,28,28),'gray',3);c.line([(7,25),(25,7)],'silver',3)
    elif name=='wrong_target':
        c.ellipse((4,4,27,27),'coral',2);c.line([(1,16),(31,16)],'coral',2);c.line([(16,1),(16,31)],'coral',2)
    elif name=='eye':
        c.poly([(1,16),(10,7),(22,7),(31,16),(22,25),(10,25)],'silver');c.poly([(5,16),(12,10),(20,10),(27,16),(20,22),(12,22)],'panel');diamond(c,16,16,5,'mint')
    return c

ICONS = {
    **{f'type_{k}':v[0] for k,v in TYPES.items()},
    'rank':'位阶','analysis_cost':'解析费用','cast_cost':'施法费用','body':'阵体','ring_slot':'环位','mana_income':'魔素收入','duration':'持续计数',
    'state_analyzing':'解析中','state_analyzed':'解析完成','state_prepared':'待施放','state_active':'持续生效','state_cancelled':'准备被取消','state_protected':'基础阵法保护','state_attached':'符文附着',
    'life':'生命','mana':'魔素','load':'当前荷载','capacity':'荷载容量',
    'zone_formation':'阵法','zone_analysis':'解析','zone_casting':'施法','zone_words':'言灵','zone_action':'行动','zone_ash':'灰烬','zone_hand':'手牌','zone_deck':'牌库',
    'close':'关闭','confirm':'确认','cancel':'取消','left':'向前滚动','right':'向后滚动','up':'向上','down':'向下','save':'保存','log':'记录','surrender':'投降','success':'成功','no_action':'无操作','wrong_target':'选错目标','rejected':'操作拒绝','eye':'查看',
}

def frame_asset(w,h,typ,mode):
    c=Canvas(w,h);accent=TYPES[typ][1];outline(c,0,0,w,h)
    c.layer('type_band');c.rect(4,6,3,min(15,h-12),accent)
    if mode!='strip':
        c.line([(9,22),(w-10,22)],'bronze1');c.rect(5,h-28,w-10,23,'raised');c.line([(8,h-29),(w-9,h-29)],'bronze1')
        if mode=='detail':
            c.rect(8,43,w-16,108,'ink');c.line([(12,155),(w-13,155)],'bronze1');c.rect(8,278,w-16,64,'raised')
    else:c.line([(29,6),(29,h-7)],'bronze1')
    return c

def state_overlay(w,h,state):
    c=Canvas(w,h);cols={'hover':'lightgold','selected':'mint','target_candidate':'lightgold','target_selected':'coral','cost_candidate':'violet','cost_selected':'violet'};co=cols[state]
    if state=='target_candidate':
        for x in range(2,w-2,7):c.rect(x,1,3,2,co);c.rect(x,h-3,3,2,co)
        for y in range(2,h-2,7):c.rect(1,y,2,3,co);c.rect(w-3,y,2,3,co)
    else:
        for x,y,sx,sy in [(1,1,1,1),(w-2,1,-1,1),(1,h-2,1,-1),(w-2,h-2,-1,-1)]:
            c.line([(x+sx*10,y),(x,y),(x,y+sy*10)],co,2 if state=='hover' else 3)
        if state=='selected':
            c.line([(5,7),(5,4),(8,4)],co,1);c.line([(w-8,h-5),(w-5,h-5),(w-5,h-8)],co,1)
        elif state=='target_selected':diamond(c,w//2,4,4,co)
        elif state.startswith('cost'):c.poly([(w-14,1),(w-1,1),(w-1,14)],co)
        if state=='cost_selected':c.line([(w-12,5),(w-8,9),(w-3,3)],'ink',2)
    return c

def build():
    REGISTRY.clear()
    for name,label in ICONS.items():
        c=icon(name);export(c,name,'icons',label,anchor=[16,16],pixel_unit=1)
        # Nearest-neighbor export of our code-native icon, not a generated image edit.
        big=Canvas(64,64);big.sprite(UI/'icons'/f'{name}.png',0,0,64,64);export(big,name+'_64','icons',label+' 2×',anchor=[32,32],pixel_unit=2)
    for typ in TYPES:
        for mode,(w,h) in {'hand':(101,118),'strip':(125,44),'portrait':(90,112),'detail':(215,354)}.items():
            export(frame_asset(w,h,typ,mode),f'frame_{typ}_{mode}','cards',f'{TYPES[typ][0]} {mode}',insets=[8,8,8,8],stretch='nine_slice',content_insets=[8,24,8,29])
    for name,col,number in [('common','white',1),('uncommon','green',2),('rare','mint',3),('epic','violet',4),('legendary','gold',5)]:
        c=Canvas(42,12)
        for i in range(number):diamond(c,5+i*8,6,3,col)
        export(c,'rarity_'+name,'cards','稀有度独立标记；仍需动态文字',reserved=name in ('epic','legendary'))
    for w,h,suffix in [(101,118,'hand'),(39,41,'opponent'),(100,132,'deck')]:
        c=Canvas(w,h);outline(c,0,0,w,h);c.layer('symmetric_sigil');cx,cy=w//2,h//2;r=min(w,h)//3
        for rr in (r,r-4):c.line([(cx,cy-rr),(cx+rr,cy),(cx,cy+rr),(cx-rr,cy),(cx,cy-rr)],'bronze2')
        diamond(c,cx,cy,max(3,r//4),'teal2');diamond(c,cx,cy,max(1,r//8),'mint')
        for yy in (-r//2,r//2):diamond(c,cx,cy+yy,2,'gold')
        export(c,'card_back_'+suffix,'cards','对称通用卡背；不携带私有信息')
    c=Canvas(64,64);c.ellipse((9,9,54,54),'muted',2);diamond(c,32,32,12,'raised');c.line([(32,21),(32,34)],'silver',3);c.rect(31,40,3,3,'silver');export(c,'missing_art','cards','缺图统一回退纹章')
    for kind,edge,fill in [('primary','mint','teal0'),('secondary','gold','panel'),('danger','coral','ruby')]:
        for state in ['normal','hover','pressed','disabled']:
            c=Canvas(160,36);outline(c,0,0,160,36,edge if state!='disabled' else 'gray',fill if state!='disabled' else 'raised')
            c.layer('state')
            if state=='hover':c.line([(12,5),(147,5)],'pale' if kind=='primary' else 'lightgold',2)
            if state=='pressed':c.rect(10,5,140,3,'ink');c.line([(12,30),(147,30)],edge)
            if state=='disabled':c.rect(9,8,2,20,'gray')
            export(c,f'button_{kind}_{state}','interaction',f'{kind} / {state}',insets=[12,8,12,8],stretch='nine_slice',text_insets=[14,7,14,7],pressed_text_offset=[0,1])
    for mode,w,h in [('hand',101,118),('strip',125,44),('portrait',90,112)]:
        for state in ['hover','selected','target_candidate','target_selected','cost_candidate','cost_selected']:
            export(state_overlay(w,h,state),'overlay_'+mode+'_'+state,'interaction',state,pixel_unit=1,overlay=True)
    for name,(w,h) in {'formation':(106,52),'analysis':(427,52),'casting':(1080,65),'words':(531,106),'action':(531,49),'ash':(531,101),'hand':(1080,134),'deck':(100,132)}.items():
        c=Canvas(w,h);outline(c,0,0,w,h,'muted','panel',False);c.layer('label_tab');c.rect(6,4,min(w-12,80),17,'raised');c.line([(10,h-5),(w-11,h-5)],'raised')
        export(c,'region_'+name,'board','区域框体：'+name,insets=[8,22,8,8],stretch='nine_slice')
    for name,w,h in [('slot_empty',106,52),('slot_occupied',106,52),('slot_span',106,107)]:
        c=Canvas(w,h);outline(c,0,0,w,h,'muted' if name=='slot_empty' else 'green','panel',False)
        if name=='slot_empty':diamond(c,w//2,h//2,7,'raised')
        if name=='slot_span':c.layer('host_span');c.line([(w-6,10),(w-3,10),(w-3,h-11),(w-6,h-11)],'green',2)
        export(c,name,'board','阵法槽状态',insets=[8,8,8,8],stretch='nine_slice')
    c=Canvas(12,52);c.line([(1,26),(9,26)],'green',2);c.poly([(6,22),(11,26),(6,30)],'mint');export(c,'host_connector','board','宿主到解析行关联')
    for name,w,h in [('hud',215,190),('phase_bar',700,42),('task_hint',800,36),('action_bar',672,44),('summary',215,88),('decision',540,310),('log_window',620,420),('result_window',520,340),('handoff',1080,134)]:
        c=Canvas(w,h);outline(c,0,0,w,h,'gold' if name in ('hud','result_window','decision') else 'bronze2');c.layer('divider');c.line([(12,36),(w-13,36)],'bronze1')
        export(c,name,'interaction' if name not in ('hud','phase_bar') else 'board',name,insets=[12,40,12,12],stretch='nine_slice')
    for state,col in [('inactive','muted'),('current','mint'),('complete','gold')]:
        c=Canvas(22,22);diamond(c,11,11,10,col);diamond(c,11,11,6,'panel');
        if state=='current':diamond(c,11,11,3,'mint')
        export(c,'phase_'+state,'board','阶段指示 '+state)
    for name in ['scroll_track','scroll_thumb']:
        c=Canvas(80,6);c.rect(0,1,80,4,'raised' if name=='scroll_track' else 'bronze2');c.rect(2,2,76,1,'muted' if name=='scroll_track' else 'lightgold');export(c,name,'board','滚动位置提示',insets=[2,1,2,1],stretch='horizontal')
    for result,col in [('victory','gold'),('defeat','coral'),('draw','silver')]:
        c=Canvas(96,64);c.layer('result_symbol')
        if result=='victory':c.poly([(14,15),(28,27),(48,5),(68,27),(82,15),(74,49),(22,49)],col);c.rect(24,53,48,5,'bronze2');diamond(c,48,30,7,'teal1')
        elif result=='defeat':c.poly([(15,7),(45,11),(38,27),(48,39),(36,58),(18,42)],col);c.poly([(52,11),(82,7),(79,42),(59,58),(54,35),(45,26)],'ruby')
        else:c.line([(16,38),(32,24),(48,38),(64,24),(80,38)],col,5);c.line([(16,24),(32,38),(48,24),(64,38),(80,24)],'gold',3)
        export(c,'result_'+result,'interaction',result+'结果标识')
    for name,col in [('success','mint'),('no_action','silver'),('wrong_target','lightgold'),('rejected','coral')]:
        c=Canvas(360,48);outline(c,0,0,360,48,col,'panel',False);c.sprite(UI/'icons'/f'{name}.png',9,8,32,32);export(c,'feedback_'+name,'interaction',name+'反馈',insets=[44,8,12,8],stretch='nine_slice')
    for active in range(4):
        c=Canvas(480,32);c.layer('step_connections');c.line([(16,16),(460,16)],'muted',2)
        for i in range(4):
            xx=16+i*148;c.rect(xx-13,3,26,26,'bg');diamond(c,xx,16,9,'mint' if i==active else 'gold' if i<active else 'muted');diamond(c,xx,16,5,'panel')
        export(c,f'stepper_{active}','interaction','查看/目标/额外成本/确认步骤底板；文字动态绘制',step_positions=[[16+i*148,16] for i in range(4)])
    c=Canvas(1600,1000,'bg');c.layer('stone_inset')
    c.rect(12,12,1576,976,'ink');c.rect(18,18,1564,964,'bg')
    c.layer('ritual_low_contrast')
    for r in (332,310,270):
        pts=[(800+int(r*math.cos(i*math.pi/32)),480+int(r*math.sin(i*math.pi/32))) for i in range(65)];c.line(pts,'panel',2)
    for sign in (1,-1):
        pts=[(800+int(260*math.sin(i*2*math.pi/3)),480+sign*int(260*math.cos(i*2*math.pi/3))) for i in range(4)];c.line(pts,'panel',2)
    c.layer('border_ornament')
    for x,y,sx,sy in [(22,22,1,1),(1577,22,-1,1),(22,977,1,-1),(1577,977,-1,-1)]:
        c.line([(x+72*sx,y),(x,y),(x,y+72*sy)],'bronze1',2);diamond(c,x+12*sx,y+12*sy,5,'bronze2')
    export(c,'match_background','board','低对比无文字场地底图',stretch='none',logical_canvas=[1600,1000])
    c=Canvas(1600,1000);c.layer('center_divider');c.line([(260,480),(670,480)],'bronze1',2);c.line([(930,480),(1340,480)],'bronze1',2);diamond(c,800,480,12,'bronze2');diamond(c,800,480,7,'teal1');export(c,'board_decoration','board','独立中央装饰层；不定义槽位')
    manifest={'version':'ui_art_v1','logical_canvas':[1600,1000],'sampling':'nearest','palette':P,'assets':REGISTRY,'runtime_integrated':False,'text_in_textures':False}
    (UI/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    (ROOT/'tokens.json').write_text(json.dumps({'palette':P,'type_colors':TYPES,'pixel_unit':1,'font':'Noto Sans CJK SC','runtime_text':True,'state_order':['normal','hover','selected','target_candidate','target_selected','cost_candidate','cost_selected'],'source_marker_persists':True},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Exported {len(REGISTRY)} separate PNG assets and layered SVG sources.')

if __name__=='__main__':build()
