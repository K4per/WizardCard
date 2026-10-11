"""Create NEW code-native SVG/PNG components; never edit any AI raster or card art.

Run with bundled Python + Pillow. Re-reads spec lock for every emitted asset.
"""
from pathlib import Path
import json, math, hashlib, re
from PIL import Image, ImageDraw

SRC=Path(__file__).resolve().parent
ROOT=SRC.parents[1]
OUT=ROOT/'export'/'c-pixel-v1'
MAN=ROOT/'manifests'/'c-pixel-v1'
PALETTE=re.findall(r'#[0-9a-fA-F]{6}',(SRC/'spec_lock.md').read_text(encoding='utf-8').split('## palette')[1].split('## style')[0])
P=dict(zip(['ink','navy','slate','teal','teal2','teal3','muted','pale','brown0','brown1','bronze0','bronze1','gold0','gold1','gold2','gold3','paper0','paper1','paper2','paper3','paper4','paper5','white','gray','green0','green1','green2','cyan0','cyan1','purple0','purple1','red'],PALETTE))
assets=[]
def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')

class Canvas:
    def __init__(self,w,h):
        self.w,self.h=w,h;self.im=Image.new('RGBA',(w,h));self.d=ImageDraw.Draw(self.im);self.nodes=[]
    def poly(self,pts,fill,stroke=None,width=1):
        pts=[(int(x),int(y)) for x,y in pts];self.d.polygon(pts,fill=fill)
        if stroke:self.d.line(pts+[pts[0]],fill=stroke,width=width)
        self.nodes.append(f'<polygon points="{" ".join(f"{x},{y}" for x,y in pts)}" fill="{fill or "none"}" stroke="{stroke or "none"}" stroke-width="{width}"/>')
    def rect(self,x,y,w,h,fill,stroke=None,width=1):
        x,y,w,h=map(int,(x,y,w,h));self.d.rectangle((x,y,x+w-1,y+h-1),fill=fill,outline=stroke,width=width)
        self.nodes.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill or "none"}" stroke="{stroke or "none"}" stroke-width="{width}"/>')
    def line(self,pts,color,width=1):
        pts=[(int(x),int(y)) for x,y in pts];self.d.line(pts,fill=color,width=width)
        self.nodes.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in pts)}" fill="none" stroke="{color}" stroke-width="{width}"/>')
    def ring(self,cx,cy,r,color,width=1,n=16):
        pts=[(round(cx+r*math.cos(i*2*math.pi/n)),round(cy+r*math.sin(i*2*math.pi/n))) for i in range(n)]
        self.line(pts+[pts[0]],color,width)
    def svg(self):return f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" viewBox="0 0 {self.w} {self.h}" shape-rendering="crispEdges">'+''.join(self.nodes)+'</svg>'

def stepped(c,x,y,w,h,fill,edge=P['bronze1'],thick=2):
    q=4;c.poly([(x+q,y),(x+w-q-1,y),(x+w-1,y+q),(x+w-1,y+h-q-1),(x+w-q-1,y+h-1),(x+q,y+h-1),(x,y+h-q-1),(x,y+q)],fill,edge,thick)
    c.line([(x+q,y+2),(x+w-q-2,y+2)],P['gold1'])
    c.line([(x+2,y+q),(x+2,y+h-q-2)],P['gold0'])
    for px,py,sx,sy in [(x+7,y+7,1,1),(x+w-8,y+7,-1,1),(x+7,y+h-8,1,-1),(x+w-8,y+h-8,-1,-1)]:
        c.line([(px,py+5*sy),(px,py),(px+5*sx,py)],P['bronze0'])
        c.rect(px+sx,py+sy,2,2,P['gold0'])

def emit(c,ident,cat,req,**extra):
    lock=(SRC/'spec_lock.md').read_text(encoding='utf-8')
    assert 'C clean' in lock
    safe=ident.replace('.','_');png=OUT/cat/(safe+'.png');svg=SRC/'svg'/cat/(safe+'.svg')
    png.parent.mkdir(parents=True,exist_ok=True);svg.parent.mkdir(parents=True,exist_ok=True)
    c.im.save(png);svg.write_text(c.svg(),encoding='utf-8',newline='\n')
    assets.append(dict(id=ident,file=png.relative_to(ROOT).as_posix(),source=svg.relative_to(ROOT).as_posix(),size=[c.w,c.h],requirements=req,version=1,status='待接入',sampling='nearest',origin='new native SVG + primitive raster generator',license='Project-authored original; no external stock',sha256=hashlib.sha256(png.read_bytes()).hexdigest(),**extra))

def panel(ident,req,fill=None,edge=None,cat='ui',size=(128,96)):
    c=Canvas(*size);stepped(c,2,2,size[0]-4,size[1]-4,fill or P['navy'],edge or P['bronze1']);emit(c,ident,cat,req,nineSlice=[12,12,12,12],safeRect=[16,16,size[0]-32,size[1]-32])

TYPE={'analysis':('解析法术','cyan0'),'talisman':('咒符法术','teal2'),'incantation':('言灵法术','purple0'),'formation':('阵法','green0'),'rune':('符文','bronze0')}
def emblem(c,kind,cx=32,cy=32,r=20,color=None):
    color=color or P['gold1'];dark=P['ink']
    if kind in ['formation','rank']:
        a=[(cx,cy-r),(cx+r,cy+r//2),(cx-r,cy+r//2),(cx,cy-r)];b=[(cx,cy+r),(cx+r,cy-r//2),(cx-r,cy-r//2),(cx,cy+r)]
        c.line(a,dark,5);c.line(b,dark,5);c.line(a,color,2);c.line(b,color,2)
    elif kind in ['rune','capacity','material','target']:
        c.poly([(cx,cy-r),(cx+r,cy),(cx,cy+r),(cx-r,cy)],P['slate'],color,2)
        c.line([(cx,cy-r+7),(cx,cy+r-7)],P['gold2'],2)
        c.line([(cx-r+7,cy),(cx+r-7,cy)],P['gold2'],2)
    elif kind in ['talisman','facedown','flip','missing','side-deck']:
        c.poly([(cx-r+5,cy-r),(cx+r-5,cy-r),(cx+r-2,cy+r),(cx-r+2,cy+r)],P['paper3'],color,2)
        q=max(1,r//3);v=max(1,r//2)
        c.line([(cx-q,cy-v),(cx+q,cy-v),(cx-q,cy+v),(cx+q,cy+v)],P['bronze0'],max(1,r//6))
    elif kind in ['incantation','priority','response','speed']:
        for k in range(3):c.line([(cx-r+k*8,cy-r+k*4),(cx-r+8+k*8,cy),(cx-r+k*8,cy+r-k*4)],color,3)
    elif kind in ['life','heal','temp-life']:
        if kind=='temp-life':
            c.poly([(cx-r,cy-r+4),(cx+r,cy-r+4),(cx+r-3,cy+7),(cx,cy+r),(cx-r+3,cy+7)],P['navy'],color,3)
            c.line([(cx,cy-10),(cx,cy+10)],P['cyan1'],3)
        else:c.poly([(cx,cy-r+7),(cx-r//2,cy-r),(cx-r,cy-r+5),(cx-r,cy),(cx,cy+r),(cx+r,cy),(cx+r,cy-r+5),(cx+r//2,cy-r)],color,P['ink'],2)
    elif kind in ['mana','gem','income','cost']:
        c.poly([(cx,cy-r),(cx+r-5,cy-r//2),(cx+r-5,cy+r//2),(cx,cy+r),(cx-r+5,cy+r//2),(cx-r+5,cy-r//2)],color,P['ink'],2)
        c.poly([(cx,cy-r+3),(cx,cy+r-3),(cx-r+8,cy+r//2),(cx-r+8,cy-r//2)],P['cyan1'])
    elif kind in ['load','danger','exhaustion','overload','error','wait','expire','no-response','invalid']:
        c.poly([(cx,cy-r),(cx+r,cy+r),(cx-r,cy+r)],P['navy'],color,3)
        c.rect(cx-2,cy-8,4,14,color);c.rect(cx-2,cy+11,4,4,color)
        if kind in ['no-response','invalid']:c.line([(cx-r,cy+r),(cx+r,cy-r)],P['red'],4)
    elif kind in ['ash','destroy','leave','discard']:
        c.poly([(cx-r,cy+r),(cx-r+6,cy+4),(cx-5,cy+8),(cx+2,cy-r),(cx+9,cy+6),(cx+r-4,cy-3),(cx+r,cy+r)],color,P['ink'],2)
    elif kind in ['check','complete','selected','cost-selected']:
        c.line([(cx-r,cy),(cx-4,cy+r-5),(cx+r,cy-r)],P['ink'],7);c.line([(cx-r,cy),(cx-4,cy+r-5),(cx+r,cy-r)],color,4)
    elif kind in ['cancel','counter']:
        c.line([(cx-r,cy-r),(cx+r,cy+r)],color,4);c.line([(cx+r,cy-r),(cx-r,cy+r)],color,4)
    elif kind in ['body','shield']:
        c.poly([(cx-r,cy-r),(cx+r,cy-r),(cx+r,cy+4),(cx,cy+r),(cx-r,cy+4)],P['teal'],color,3)
    elif kind in ['attach','host']:
        c.ring(cx-8,cy,12,color,3);c.ring(cx+8,cy,12,color,3)
    elif kind in ['analysis','timer','ongoing']:
        c.poly([(cx-r,cy-r),(cx+r,cy-r),(cx+5,cy),(cx+r,cy+r),(cx-r,cy+r),(cx-5,cy)],P['teal'],color,2)
        c.line([(cx-8,cy-r+5),(cx+8,cy-r+5),(cx,cy-2)],P['gold2'],2)
    else:
        c.ring(cx,cy,r,color,3);c.poly([(cx,cy-r+5),(cx+5,cy),(cx,cy+r-5),(cx-5,cy)],color)

def icon(ident,kind,req,color=None):
    c=Canvas(64,64);emblem(c,kind,color=color);emit(c,'c2d.icon.'+ident,'icons',req,anchor=[32,32])

def card(kind,w,h):
    c=Canvas(w,h);accent=P[TYPE[kind][1]];stepped(c,2,2,w-4,h-4,P['navy'])
    m=max(8,w//24);hdr=int(h*.11);ar=[m,max(int(h*.15),m+hdr+2),w-2*m,int(h*.41)]
    # Outline the real transparent art aperture without filling it.
    c.rect(ar[0]-2,ar[1]-2,ar[2]+4,ar[3]+4,P['bronze0'])
    c.rect(*ar,(0,0,0,0))
    c.nodes[-1]=f'<rect x="{ar[0]}" y="{ar[1]}" width="{ar[2]}" height="{ar[3]}" fill="black"/>'
    # SVG source uses a mask so the art aperture stays transparent too.
    stepped(c,m,m,w-2*m-int(h*.1),hdr,P['paper4'])
    rad=max(9,int(h*.055));cx=w-rad-m;cy=m+rad
    c.poly([(round(cx+rad*math.cos(i*math.pi/8)),round(cy+rad*math.sin(i*math.pi/8))) for i in range(16)],P['paper3'],P['gold0'],2)
    strip=int(h*.59);c.rect(m,strip,w-2*m,int(h*.065),P['paper3'],P['bronze0'])
    emblem(c,kind,m+10,strip+int(h*.03),max(5,int(h*.025)),accent)
    c.rect(m,int(h*.68),w-2*m,int(h*.235),P['paper4'],P['bronze0'])
    if kind=='formation':c.line([(m+5,int(h*.8)),(w-m-5,int(h*.8))],P['paper2'],2)
    cx=m+rad;cy=h-rad-4
    c.poly([(round(cx+rad*math.cos(i*math.pi/8)),round(cy+rad*math.sin(i*math.pi/8))) for i in range(16)],P['paper3'],P['gold0'],2)
    c.rect(int(w*.62),int(h*.94),int(w*.29),max(5,int(h*.035)),P['paper3'])
    c.line([(m+2,int(h*.135)),(w-m-2,int(h*.135))],accent,3)
    # Add source mask applied to the full drawing: transparency identical to PNG.
    svg=c.svg();mask=f'<defs><mask id="art"><rect width="{w}" height="{h}" fill="white"/><rect x="{ar[0]}" y="{ar[1]}" width="{ar[2]}" height="{ar[3]}" fill="black"/></mask></defs>'
    c.nodes=[mask+'<g mask="url(#art)">'+''.join(c.nodes)+'</g>']
    return c,ar

def gems():
    hues={'common':('gray','white',6),'uncommon':('green0','green2',16),'rare':('cyan0','cyan1',6),'epic':('purple0','purple1',4),'legendary':('bronze1','gold3',8)}
    for name,(dark,light,n) in hues.items():
        c=Canvas(64,64);r=23
        pts=[(round(32+r*math.cos(i*2*math.pi/n)),round(32+(17 if name=='uncommon' else r)*math.sin(i*2*math.pi/n))) for i in range(n)]
        c.poly(pts,P[dark],P['ink'],3)
        for i in range(n):c.poly([(32,32),pts[i],pts[(i+1)%n]],P[light if i<n//2 else dark],P[dark])
        c.line([(21,20),(32,15),(42,20)],P['white'] if name=='common' else P[light],2)
        emit(c,'c2d.rarity.'+name,'cards',['CARD-04'],anchor=[32,32],rarity=name,shape={'common':'simple hex crystal','uncommon':'oval','rare':'hexagon','epic':'diamond','legendary':'octagonal radial facets'}[name])

def build_ui():
    for name in ['surface','dialog','tooltip','input','dropdown','list','tabs','slider-track','scroll-track','loading','empty','notification','portrait','resource','phase','detail','chain','log','action','confirm','recipe','browser','privacy','reconnect']:
        panel('c2d.panel.'+name,['STYLE-03','PAGE-01..12','BOARD-04..09','BOARD-12..13'],fill=P['ink'] if name in ['detail','privacy','log'] else P['navy'])
    states=['normal','hover','pressed','focus','disabled']
    for role in ['primary','secondary','danger']:
        for state in states:
            c=Canvas(192,48);fill=P[{'primary':'teal','secondary':'navy','danger':'brown1'}[role]]
            edge=P['gray'] if state=='disabled' else P['red'] if role=='danger' else P['gold2'] if state in ['hover','focus'] else P['bronze1']
            stepped(c,2,2,188,44,P['slate'] if state=='disabled' else fill,edge)
            if state=='pressed':c.line([(9,8),(182,8)],P['ink'],3)
            if state=='focus':c.rect(7,7,178,34,None,P['cyan1'],2)
            emit(c,f'c2d.button.{role}.{state}','ui',['STYLE-03'],nineSlice=[14,12,14,12],safeRect=[20,10,152,28],state=state)
    for control in ['input','tab','dropdown','switch','checkbox']:
        for state in states:
            c=Canvas(128,48);stepped(c,2,2,124,44,P['teal'] if state in ['hover','pressed'] else P['navy'],P['gray'] if state=='disabled' else P['cyan1'] if state=='focus' else P['bronze0'])
            if control=='checkbox':c.rect(13,12,24,24,P['ink'],P['gold0']);emblem(c,'check',25,24,7,P['green2']) if state=='pressed' else None
            if control=='switch':c.rect(14,14,60,20,P['ink']);c.rect(52 if state=='pressed' else 18,17,17,14,P['green1'] if state=='pressed' else P['gray'])
            if control=='dropdown':c.poly([(104,20),(116,20),(110,28)],P['gray'] if state=='disabled' else P['gold1'])
            emit(c,f'c2d.control.{control}.{state}','ui',['STYLE-03'],nineSlice=[12,12,12,12],state=state)
    for name,kind,color in [('pagination-prev','incantation','gold1'),('pagination-next','incantation','gold1'),('scroll-thumb','rune','teal3'),('slider-thumb','rune','gold2'),('spinner','analysis','cyan1'),('empty','missing','gray'),('focus','target','cyan1')]:icon('control.'+name,kind,['STYLE-03'],P[color])
    for name in ['checked','unchecked','on','off']:
        c=Canvas(48,32);c.rect(4,4,40,24,P['ink'],P['bronze0'],2)
        if name=='checked':emblem(c,'check',24,16,9,P['green2'])
        if name in ['on','off']:c.rect(25 if name=='on' else 7,7,16,18,P['green1'] if name=='on' else P['gray'])
        emit(c,'c2d.control.'+name,'ui',['STYLE-03'])

def build_cards():
    for kind in TYPE:
        for view,(w,h) in {'hand':(160,224),'field':(128,176),'builder':(224,304),'detail':(384,512)}.items():
            c,ar=card(kind,w,h)
            emit(c,f'c2d.card.{view}.{kind}','cards',['CARD-01','CARD-02'],cardType=kind,view=view,artRect=ar,nameRect=[10,8,w-50,int(h*.1)],effectRect=[12,int(h*.69),w-24,int(h*.2)],rarityAnchor=[int(w*.85),int(h*.62)],valueAnchors=[[w-24,24],[24,h-18]])
    gems()
    for kind in TYPE:icon('type.'+kind,kind,['CARD-05'],P[TYPE[kind][1]])
    for i in range(1,5):
        c=Canvas(64,64);emblem(c,'speed',color=P['red'] if i==4 else P['cyan1'])
        for j in range(i):c.rect(12+j*11,52,7,5,P['red'] if i==4 else P['gold1'])
        emit(c,'c2d.icon.speed.'+str(i),'icons',['CARD-05','BOARD-08'],dynamicNumber=i,noResponse=i==4)
    for category,kind,color in [('level','formation','green2'),('rank','rank','cyan1')]:
        for i in range(1,8):
            c=Canvas(64,64);emblem(c,kind,r=16,color=P[color])
            for j in range(i):c.rect(4+j*8,55,5,4,P[color])
            emit(c,f'c2d.icon.{category}.{i}','icons',['CARD-05'],dynamicNumber=i)
    attrs={'mana':'mana','analysis-cost':'analysis','cast-cost':'cost','body':'body','capacity':'capacity','ring-slot':'gem','income':'income','recipe-mana':'mana','recipe-ash':'ash','recipe-material':'material'}
    for name,kind in attrs.items():icon('attribute.'+name,kind,['CARD-05'])
    status={'analysis-start':'analysis','analysis-ongoing':'ongoing','analysis-complete':'complete','ready':'gem','facedown':'facedown','flip':'flip','talisman-wait':'wait','focused':'target','sustained':'ring','cancelled':'cancel','attached':'attach','missing':'missing','invalid-id':'invalid'}
    for name,kind in status.items():icon('state.'+name,kind,['CARD-06','CARD-07'],P['red'] if kind in ['cancel','invalid'] else P['cyan1'])
    panel('c2d.card.missing-art',['CARD-07'],cat='cards',size=(128,96));icon('missing-art','missing',['CARD-07'])
    # Anonymous, identical compact back across every private card type.
    c=Canvas(160,224);stepped(c,2,2,156,220,P['navy']);c.rect(14,15,132,194,P['teal'],P['bronze0'],2);emblem(c,'rune',80,112,42,P['gold1']);c.ring(80,112,60,P['bronze0'],2)
    emit(c,'c2d.card.back.compact','cards',['CARD-03'],privateIdentity='shared all types')

def build_board():
    c=Canvas(1920,1080);c.rect(0,0,1920,1080,P['navy'])
    for y in range(0,1080,24):
        for x in range(0,1920,24):
            if ((x//24)*7+(y//24)*11)%13==0:c.rect(x+4,y+5,9,3,P['slate'])
    emit(c,'c2d.board.background','board',['BOARD-01'],layer=0,opaque=True)
    c=Canvas(1920,1080);stepped(c,290,150,1340,780,P['ink'],P['bronze0']);emit(c,'c2d.board.main-surface','board',['BOARD-01'],layer=1,safeRect=[310,170,1300,740])
    c=Canvas(1920,1080);stepped(c,8,8,1904,1064,None,P['bronze0'])
    for x,y in [(36,36),(1884,36),(36,1044),(1884,1044)]:emblem(c,'rune',x,y,15,P['gold0'])
    emit(c,'c2d.board.edge-overlay','board',['BOARD-01'],layer=2)
    for name in ['casting','analysis-row','formation-anchor','hand','main-deck','side-deck','ash','host-group','slot','empty-deck']:
        panel('c2d.zone.'+name,['BOARD-02','BOARD-03','BOARD-06'],fill=P['green0'] if name=='analysis-row' else P['navy'],cat='board')
    for name,kind,color in [('main-deck','facedown','gold1'),('side-deck','formation','green2'),('ash','ash','gray'),('empty-deck','exhaustion','red'),('host','host','cyan1')]:icon('zone.'+name,kind,['BOARD-02','BOARD-03','BOARD-06'],P[color])
    c=Canvas(128,32);c.line([(4,16),(120,16)],P['cyan0'],2);c.poly([(120,10),(127,16),(120,22)],P['cyan1']);emit(c,'c2d.board.host-link','board',['BOARD-03'],nineSlice=[8,8,8,8])
    for name,color,kind in [('life','red','life'),('temp-life','cyan1','temp-life'),('mana','cyan1','mana'),('load','gold1','load'),('cap','gray','capacity')]:icon('hud.'+name,kind,['BOARD-04'],P[color])
    for state,color in [('normal','green1'),('overload','gold2'),('danger','red')]:
        c=Canvas(128,24);stepped(c,2,2,124,20,P['ink'],P[color]);emit(c,'c2d.hud.load.'+state,'board',['BOARD-04'],nineSlice=[8,6,8,6],state=state)
        icon('hud.load-'+state,'check' if state=='normal' else 'danger',['BOARD-04'],P[color])
    for phase,kind in [('draw','facedown'),('prepare','analysis'),('main','formation'),('cast','gem'),('end','ash')]:icon('phase.'+phase,kind,['BOARD-05'])
    for state,kind in [('priority','priority'),('pass','incantation'),('pending','analysis'),('resolving','complete'),('cancelled','cancel'),('no-response','no-response')]:icon('chain.'+state,kind,['BOARD-08'])
    for state,color,mark in [('hover','gold1','ring'),('selected','cyan1','target'),('legal','green2','check'),('target','purple1','target'),('cost-candidate','gold2','material'),('cost-selected','green2','cost-selected')]:
        c=Canvas(128,176);stepped(c,1,1,126,174,None,P[color],3);emblem(c,mark,108,18,9,P[color]);emit(c,'c2d.overlay.'+state,'board',['BOARD-10'],nineSlice=[20,20,20,20],overlay=True)
    for state,kind,color in [('preview','target','muted'),('legal-drop','check','green2'),('invalid-drop','invalid','red'),('return','cancel','gray')]:
        c=Canvas(128,96);stepped(c,2,2,124,92,None,P[color],2);emblem(c,kind,64,48,20,P[color]);emit(c,'c2d.drag.'+state,'board',['BOARD-11'],nineSlice=[12,12,12,12])
    for state,kind,color in [('quota-available','check','green2'),('quota-used','cancel','gray'),('condition-met','check','green2'),('condition-unmet','invalid','red'),('zero-cost','facedown','muted'),('flip-cost','flip','gold1'),('wait-turn','wait','gold2'),('cannot-cast','no-response','gray')]:icon('decision.'+state,kind,['BOARD-12','BOARD-13'],P[color])

FX=[('analysis-start','analysis','cyan1','FX-02'),('analysis-loop','ongoing','cyan1','FX-02'),('analysis-complete','complete','green2','FX-02'),('prepare','ring','gold1','FX-03'),('cast','gem','cyan1','FX-03'),('resolve','complete','gold2','FX-03'),('counter','counter','purple1','FX-03'),('damage','life','red','FX-04'),('heal','heal','green2','FX-04'),('mana','mana','cyan1','FX-04'),('load','load','gold2','FX-04'),('overload-damage','overload','red','FX-04'),('exhaustion-damage','exhaustion','purple1','FX-04'),('temp-life-gain','temp-life','cyan1','FX-04'),('temp-life-expire','expire','muted','FX-04'),('attach','attach','purple1','FX-05'),('formation-destroy','destroy','red','FX-05'),('leave','leave','gray','FX-05'),('facedown','facedown','muted','FX-07'),('flip','flip','gold1','FX-07'),('material-to-ash','ash','gold0','FX-07'),('formation-set','formation','green2','FX-07'),('formation-replace','attach','green2','FX-07'),('end-cleanup','ash','gray','FX-07')]
def build_fx():
    for name,kind,color,req in FX:
        c=Canvas(64,64);emblem(c,kind,r=19,color=P[color]);emit(c,'c2d.fx.'+name+'.mask','effects',[req],anchor=[32,32],blend='alpha')
        for reduced,n,fps in [(False,8,16),(True,4,24)]:
            c=Canvas(64*n,64)
            for i in range(n):
                r=int(8+15*math.sin((i+1)*math.pi/(n+1)));cx=i*64+32
                if i!=n-1 or name=='analysis-loop':
                    emblem(c,kind,cx,32,r,P[color]);c.ring(cx,32,r+5,P[color],1)
                    if not reduced:
                        for k in range(4):
                            ang=k*math.pi/2+i*.3;c.rect(cx+round((r+8)*math.cos(ang)),32+round((r+8)*math.sin(ang)),2,2,P[color])
            emit(c,'c2d.fx.'+name+('.reduced' if reduced else '.normal'),'effects',[req],frameSize=[64,64],frames=n,fps=fps,loop=name=='analysis-loop' and not reduced,loopPoint=0 if name=='analysis-loop' and not reduced else None,blend='alpha',reducedMotion=reduced,anchor=[32,32])
    for name,kind,color in [('victory','formation','gold2'),('defeat','cancel','gray'),('draw','ring','cyan1'),('opening','gem','gold1')]:
        c=Canvas(256,128);emblem(c,kind,128,64,46,P[color]);c.line([(12,64),(65,64)],P['bronze0'],2);c.line([(190,64),(243,64)],P['bronze0'],2)
        emit(c,'c2d.result.'+name,'effects',['FX-06'],anchor=[128,64])

def main():
    build_ui();build_cards();build_board();build_fx()
    write(MAN/'native-assets.json',{'palette':P,'assets':assets})
    print(json.dumps({'assets':len(assets),'png':len(assets),'svg':len(assets)},ensure_ascii=False))
if __name__=='__main__':main()
