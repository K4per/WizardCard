"""Deterministic, code-native material/UI/VFX sources. Never edits existing artwork."""
from pathlib import Path
import json, math, hashlib, shutil
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[3]
ART = PROJECT / 'assets/art'
OUT = ART / 'nonmodel/a-gilded-v15'
ENGINE = PROJECT / 'godot/art/nonmodel/a-gilded-v15'
P = {'gold':'#BA914C', 'light':'#F4D785', 'jade':'#70BDA0', 'ruby':'#CF7361',
     'silver':'#BFCBD0', 'violet':'#BDA2DD', 'blue':'#72AACF', 'ink':'#201B20'}
entries = []
FONT = ImageFont.truetype(str(PROJECT/'assets/fonts/NotoSansCJKsc-Regular.otf'), 22)

def lock():
    # Read immediately before each independently produced asset.
    assert '传说宝石固定金黄色' in (HERE/'spec_lock.md').read_text(encoding='utf-8')

def save(im, name, req, **meta):
    path = OUT/name; path.parent.mkdir(parents=True, exist_ok=True); im.save(path)
    target = ENGINE/name; target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(path, target)
    entry = {'id':'nm_' + Path(name).stem, 'file':str(path.relative_to(ART)).replace('\\','/'),
             'engine':'res://'+str(target.relative_to(PROJECT/'godot')).replace('\\','/'),
             'requirement':req, 'size':list(im.size), 'mode':im.mode,
             'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
             'source':'project-native procedural/vector', 'license':'project license; no external imagery', **meta}
    entries.append(entry)

def textfile(name, content):
    for root in (OUT, ENGINE):
        path=root/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_text(content,encoding='utf-8',newline='\n')

def rgb(hexcolor):
    return np.array([int(hexcolor[i:i+2],16) for i in (1,3,5)],dtype=np.float32)

def pbr(name, base, rough, metal, kind, n=2048):
    lock()
    y,x=np.mgrid[0:n,0:n].astype(np.float32)/n
    # Periodic harmonics: all maps share a height source and wrap across UV edges.
    grain=np.sin(2*np.pi*(x*16+0.24*np.sin(y*2*np.pi*3)))
    rng=np.random.default_rng(1510)
    broad=np.zeros_like(x)
    for k in range(18):
        fx,fy=rng.integers(1,19,size=2); phase=float(rng.uniform(0,2*np.pi))
        broad+=np.sin(2*np.pi*(x*fx+y*fy)+phase)/math.sqrt(18)
    broad=np.tanh(broad)
    fine=np.sin(2*np.pi*x*117)*np.sin(2*np.pi*y*131)
    pores=np.sin(2*np.pi*x*263+np.sin(2*np.pi*y*139))
    if kind=='wood':
        h=.5+.14*grain+.08*np.sin(2*np.pi*(x*64+.35*np.sin(y*2*np.pi*3)))+.03*fine
        tone=.035*grain+.09*broad+.02*fine
    elif kind=='leather':
        h=.5+.045*pores+.06*fine+.025*broad
        tone=.055*broad+.025*fine
    elif kind=='gold':
        scratches=np.maximum(0,np.cos(2*np.pi*(x*431+y*3))-.94)
        h=.5+.008*scratches+.008*fine
        tone=.08*broad-.12*scratches
    else:
        h=.5+.1*broad+.045*fine+.02*pores
        tone=.1*broad+.045*fine
    # Deliberately restrained, stepped base-color values; detail lives in normals.
    tone=np.round(tone*32)/32
    color=np.clip(rgb(base)[None,None,:]*(1+tone[:,:,None]),0,255).astype('uint8')
    dx=(np.roll(h,-1,1)-np.roll(h,1,1))*1.4
    dy=(np.roll(h,-1,0)-np.roll(h,1,0))*1.4
    normals=np.stack([-dx,dy,np.ones_like(h)],axis=-1)
    normals/=np.linalg.norm(normals,axis=-1,keepdims=True)
    maps={'basecolor':Image.fromarray(color), 'normal':Image.fromarray(((normals*.5+.5)*255).astype('uint8')),
          'roughness':Image.fromarray(np.clip((rough+.06*fine+.035*broad)*255,0,255).astype('uint8')),
          'metallic':Image.new('L',(n,n),round(metal*255))}
    for channel,im in maps.items():
        save(im,f'textures/{name}_{channel}.png','MAT-03' if name=='paper_edge' else 'MAT-01',
             color_space='sRGB' if channel=='basecolor' else 'linear',channel='RGB' if channel in ('basecolor','normal') else 'R',
             normal_convention='OpenGL +Y' if channel=='normal' else None,repeat=True)
    s=f'''[gd_resource type="StandardMaterial3D" load_steps=5 format=3]
[ext_resource type="Texture2D" path="res://art/nonmodel/a-gilded-v15/textures/{name}_basecolor.png" id="1"]
[ext_resource type="Texture2D" path="res://art/nonmodel/a-gilded-v15/textures/{name}_normal.png" id="2"]
[ext_resource type="Texture2D" path="res://art/nonmodel/a-gilded-v15/textures/{name}_roughness.png" id="3"]
[ext_resource type="Texture2D" path="res://art/nonmodel/a-gilded-v15/textures/{name}_metallic.png" id="4"]
[resource]
albedo_texture = ExtResource("1")
albedo_color = Color({0.4 if name == 'leather' else 1.0}, {0.4 if name == 'leather' else 1.0}, {0.4 if name == 'leather' else 1.0}, 1)
metallic = {metal}
metallic_texture = ExtResource("4")
metallic_texture_channel = 0
roughness = 1.0
roughness_texture = ExtResource("3")
roughness_texture_channel = 0
normal_enabled = true
normal_scale = 0.35
normal_texture = ExtResource("2")
texture_filter = 3
uv1_scale = Vector3(3, 2, 1)
'''
    textfile(f'materials/{name}.tres',s)

class Vector:
    def __init__(self,w,h):
        self.im=Image.new('RGBA',(w,h)); self.d=ImageDraw.Draw(self.im); self.svg=[]; self.size=(w,h)
    def line(self,pts,c,width=2):
        color=P.get(c,c); self.d.line(pts,fill=color,width=width)
        self.svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in pts)}" fill="none" stroke="{color}" stroke-width="{width}"/>')
    def box(self,box,c,width=2,fill=None):
        color=P.get(c,c); self.d.rectangle(box,outline=color,width=width,fill=P.get(fill,fill))
        x,y,r,b=box; self.svg.append(f'<rect x="{x}" y="{y}" width="{r-x}" height="{b-y}" fill="{P.get(fill,fill) or "none"}" stroke="{color}" stroke-width="{width}"/>')
    def circle(self,box,c,width=2):
        color=P.get(c,c); self.d.ellipse(box,outline=color,width=width)
        x,y,r,b=box; self.svg.append(f'<ellipse cx="{(x+r)/2}" cy="{(y+b)/2}" rx="{(r-x)/2}" ry="{(b-y)/2}" fill="none" stroke="{color}" stroke-width="{width}"/>')
    def export(self,name,req,**meta):
        save(self.im,name+'.png',req,**meta)
        w,h=self.size; path=HERE/'sources'/ (name+'.svg');path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">' + ''.join(self.svg)+'</svg>',encoding='utf-8')

def motif(v,name,c='gold',cx=128,cy=128,r=72):
    def point(a,rad=r):return (round(cx+math.cos(a)*rad),round(cy+math.sin(a)*rad))
    if name in ('analysis','analysis_start','analysis_loop','analysis_complete','casting','prepare','release','ambient_runes'):
        if name=='analysis_start':
            for k in range(4):v.line([point(k*math.pi/2+j*math.pi/40) for j in range(12)],c,3)
        else:v.circle((cx-r,cy-r,cx+r,cy+r),c,3)
        v.circle((cx-r+9,cy-r+9,cx+r-9,cy+r-9),c,2)
        for k in range(8):v.line([point(k*math.pi/4,r+8),point(k*math.pi/4,r+17)],c,3)
        if name in ('casting','release','analysis_complete'):
            for offset in (0,math.pi):v.line([point(offset+i*2*math.pi/3,r-18) for i in range(4)],c,3)
        elif name=='prepare':
            v.line([point(i*math.pi/2,r-22) for i in range(5)],c,3)
    elif name in ('words','attach','host_link'):
        v.line([(cx-r,cy),(cx-r//3,cy-r//2),(cx+r//3,cy+r//2),(cx+r,cy)],c,4)
        for off in (-r,r):v.circle((cx+off-12,cy-12,cx+off+12,cy+12),c,3)
        v.line([(cx-20,cy),(cx,cy-20),(cx+20,cy),(cx,cy+20),(cx-20,cy)],c,3)
    elif name in ('action','damage'):
        v.line([(cx-r,cy+20),(cx-5,cy+10),(cx-12,cy+r),(cx+r,cy-20),(cx+8,cy-12),(cx+16,cy-r),(cx-r,cy+20)],c,4)
    elif name in ('ash','leave','destroy'):
        for k in range(6):
            a=k*math.pi/3;v.line([point(a,r*.4),point(a,r*.85)],c,4)
            p=point(a,r);v.box((p[0]-3,p[1]-3,p[0]+3,p[1]+3),c,fill=c)
        if name=='destroy':v.line([(cx-17,cy+20),(cx+4,cy),(cx-5,cy-21)],c,3)
    elif name in ('slot','load'):
        for k in range(3):
            off=(k-1)*25;v.line([(cx-50,cy+off+8),(cx,cy+off-8),(cx+50,cy+off+8)],c,4)
    elif name in ('heal','mana'):
        if name=='heal':
            v.line([(cx,cy-r),(cx,cy+r)],c,9);v.line([(cx-r,cy),(cx+r,cy)],c,9)
            v.circle((cx-r-10,cy-r-10,cx+r+10,cy+r+10),c,2)
        else:
            v.line([(cx,cy-r),(cx+r//2,cy),(cx,cy+r),(cx-r//2,cy),(cx,cy-r)],c,4)
            v.line([(cx,cy+28),(cx,cy-28)],c,4)
    elif name=='counter':
        v.line([(cx-r,cy-r),(cx+r,cy+r)],c,6);v.line([(cx+r,cy-r),(cx-r,cy+r)],c,6)
        v.circle((cx-r-8,cy-r-8,cx+r+8,cy+r+8),c,3)
    elif name=='ambient_dust':
        for k in range(13):
            a=k*2.4;p=point(a,20+k*5);v.box((p[0],p[1],p[0]+2,p[1]+2),c,fill=c)

def zones():
    for name in ('analysis','casting','words','action','ash','slot','host_link'):
        lock();v=Vector(512,256)
        v.box((9,9,502,246),'gold',2);v.box((15,15,496,240),'gold',1)
        for x in (30,482):
            for y in (30,226):v.line([(x-10,y),(x,y-10),(x+10,y),(x,y+10),(x-10,y)],'gold',2)
        motif(v,name,cx=256,cy=128,r=60)
        v.export('zones/zone_'+name,'MAT-02',opacity=0.26,filter='linear_mipmap',text=False)

EFFECTS=[('ambient_dust','light',True,6.0),('ambient_runes','jade',True,4.0),
 ('analysis_start','gold',False,.45),('analysis_loop','jade',True,2.0),('analysis_complete','light',False,.5),
 ('prepare','gold',False,.45),('release','jade',False,.5),('counter','silver',False,.45),
 ('damage','ruby',False,.45),('heal','jade',False,.5),('mana','blue',False,.45),('load','gold',False,.45),
 ('attach','jade',False,.5),('destroy','violet',False,.5),('leave','silver',False,.4)]

def effects():
    for name,c,loop,duration in EFFECTS:
        lock();v=Vector(256,256);motif(v,name,c)
        group='VFX-01' if name.startswith('ambient') else 'VFX-02' if name.startswith('analysis') else 'VFX-03' if name in ('prepare','release','counter') else 'VFX-04' if name in ('damage','heal','mana','load') else 'VFX-05'
        v.export('vfx/fx_'+name,group,filter='nearest',loop=loop,duration=duration)
        for count,mode in ((8,'normal'),(4,'reduced')):
            sheet=Image.new('RGBA',(256*count,256))
            for i in range(count):
                t=i/max(1,count-1); scale=1 if mode=='reduced' else .72+.28*t
                small=v.im.resize((round(256*scale),round(256*scale)),Image.Resampling.NEAREST)
                alpha=np.array(small.getchannel('A'),dtype=np.float32)
                fade=1 if loop else math.sin(math.pi*(.1+t*.8))
                small.putalpha(Image.fromarray((alpha*fade).astype('uint8')))
                sheet.paste(small,(i*256+(256-small.width)//2,(256-small.height)//2))
            save(sheet,f'vfx/sheets/{name}_{mode}.png',group,frames=count,frame_size=[256,256],filter='nearest',preview_only=True)
        textfile(f'effects/{name}.tscn',f'''[gd_scene load_steps=3 format=3]
[ext_resource type="Script" path="res://scripts/art/wizard_art_effect_3d.gd" id="1"]
[ext_resource type="Texture2D" path="res://art/nonmodel/a-gilded-v15/vfx/fx_{name}.png" id="2"]
[node name="{name}" type="Node3D"]
script = ExtResource("1")
effect_texture = ExtResource("2")
duration = {duration}
looping = {'true' if loop else 'false'}
motion_style = {2 if name in ('damage','destroy','leave') else 1 if name in ('heal','mana') else 0}
''')

def ui():
    for name,c,index in [('hover','light',0),('selected','gold',1),('candidate','jade',2),('target','jade',3),('cost','violet',4),('host','blue',5),('seal','silver',6)]:
        lock();v=Vector(128,192);v.box((6,6,121,185),c,2 if index==0 else 3)
        if index==2:
            for y in range(18,180,22):v.line([(2,y),(2,y+10)],c,3)
        elif index==3:
            for x,y in ((12,12),(116,12),(12,180),(116,180)):
                v.line([(x-5,y),(x+5,y)],c,3);v.line([(x,y-5),(x,y+5)],c,3)
        elif index==4:
            for x in (16,28,40):v.line([(x,14),(x+6,20),(x,26),(x-6,20),(x,14)],c,2)
        else:motif(v,'words' if index>=5 else 'mana',c,cx=64,cy=175,r=9)
        v.export('ui/interaction_'+name,'UI-02',text=False,shape_state=name)
    for name,c,shape in [('enter','gold','mana'),('priority','jade','action'),('resolve','light','load'),('cancel','silver','counter')]:
        lock();v=Vector(64,64);motif(v,shape,c,cx=32,cy=32,r=20)
        v.export('ui/chain_'+name,'UI-03',text=False)
    for name in ('hud_phase','hud_resources','hud_detail','hud_chain','hud_action'):
        lock();v=Vector(256,128);v.box((1,1,254,126),'gold',2,fill='ink');v.box((5,5,250,122),'#634837',1)
        small=name in ('hud_phase','hud_resources')
        if not small:
            for x,y in ((14,14),(242,14),(14,114),(242,114)):
                v.line([(x-5,y),(x,y-5),(x+5,y),(x,y+5),(x-5,y)],'gold',2)
        v.export('ui/'+name,'UI-01',text=False,stretch='nine_slice',insets=[6,6,6,6] if small else [20,20,20,20],filter='nearest')
    for name,c in [('win','light'),('lose','silver'),('draw','jade')]:
        lock();v=Vector(256,128)
        v.line([(8,64),(64,64)],c,2);v.line([(192,64),(248,64)],c,2)
        if name=='win':
            motif(v,'release',c,cx=128,cy=64,r=36)
            for off in (-54,54):v.line([(128+off,84),(120+off,64),(128+off,42)],c,3)
        elif name=='lose':motif(v,'destroy',c,cx=128,cy=64,r=38)
        else:
            for off in (-22,22):motif(v,'mana',c,cx=128+off,cy=64,r=22)
        v.export('ui/result_'+name,'ANI-02',text=False,filter='nearest')

def animations():
    specs=[('draw',.28,.12,'position',[(0,1.1,0),(0,0,0)]),('place',.28,.12,'position',[(0,.3,0),(0,0,0)]),
      ('flip',.4,.12,'rotation',[(0,0,math.pi),(0,0,0)]),('raise',.12,.08,'position',[(0,0,0),(0,.18,0)]),
      ('return',.18,.08,'position',[(0,.18,0),(0,0,0)]),('intro',.55,.12,'scale',[(.97,.97,.97),(1,1,1)]),
      ('win',.5,.12,'scale',[(.96,.96,.96),(1,1,1)]),('lose',.4,.12,'scale',[(1.02,1.02,1.02),(1,1,1)]),
      ('draw_result',.4,.12,'scale',[(1,1,1),(1,1,1)]),('camera_response',.16,.0,'position',[(0,.015,0),(0,0,0)])]
    subs=[];maps=[];table=[]
    for name,duration,low,prop,values in specs:
        table.append({'name':name,'seconds':duration,'reduced_seconds':low,'curve':'cubic out (runtime Tween); linear library sample',
                      'anchor':'Visual child, never hit-test root','interrupt':'kill tween, restore committed destination',
                      'layer':'3D public card / foreground result; below HUD','camera_default':'disabled' if name=='camera_response' else 'fixed'})
        for suffix,d in (('',duration),('_reduced',max(low,.01))):
            key=name+suffix; a,b=values
            if low==0 and suffix:a=b
            subs.append(f'''[sub_resource type="Animation" id="{key}"]
resource_name = "{key}"
length = {d}
tracks/0/type = "value"
tracks/0/path = NodePath("Visual:{prop}")
tracks/0/interp = 1
tracks/0/keys = {{"times": PackedFloat32Array(0, {d}), "transitions": PackedFloat32Array(1, 1), "update": 0, "values": [Vector3{tuple(a)}, Vector3{tuple(b)}]}}
''')
            maps.append(f'&"{key}": SubResource("{key}")')
    textfile('animations/card_and_match.tres',f'[gd_resource type="AnimationLibrary" load_steps={len(subs)+1} format=3]\n'+''.join(subs)+'[resource]\n_data = {\n'+',\n'.join(maps)+'\n}\n')
    textfile('animations/spec.json',json.dumps(table,ensure_ascii=False,indent=2))

def diagrams():
    # Coordinates read from the authoritative engine scene, not a new layout proposal.
    import re
    raw=(PROJECT/'godot/scenes/match/battlefield.tscn').read_text(encoding='utf-8')
    rows=[]
    for match in re.finditer(r'\[node name="([^"]+)" parent="ViewportContainer/Viewport/World/Zones"[^\n]*\]\n([^\[]+)',raw):
        name,body=match.groups(); pos=re.search(r'position = Vector3\(([^)]+)\)',body);dim=re.search(r'dimensions = Vector2\(([^)]+)\)',body);title=re.search(r'title_text = "([^"]+)"',body)
        if pos and dim:
            x,_,z=map(float,pos.group(1).split(','));w,h=map(float,dim.group(1).split(','));rows.append((name,x,z,w,h,title.group(1) if title else '解析区'))
    im=Image.new('RGB',(1600,1200),'#181E20');d=ImageDraw.Draw(im)
    d.text((40,25),'VIS-02 · 当前场地顶视 / 尺寸不变 · Godot Y 向上',font=FONT,fill=P['light'])
    def xy(x,z):return (800+x*58,580+z*58)
    d.rectangle((*xy(-9.65,-7.15),*xy(9.65,7.15)),outline=P['gold'],width=5)
    d.rectangle((*xy(-9.35,-6.85),*xy(9.35,6.85)),fill='#582938',outline=P['gold'],width=1)
    for name,x,z,w,h,title in rows:
        a=xy(x-w/2,z-h/2);b=xy(x+w/2,z+h/2);d.rectangle((*a,*b),outline=P['jade'],width=2)
        d.text((a[0]+8,a[1]+4),f'{title} {w:g}×{h:g}',font=FONT,fill=P['light'])
    for x,z in ((-8,5),(8,-5)):
        a=xy(x-.55,z-.8);b=xy(x+.55,z+.8);d.rectangle((*a,*b),outline=P['silver'],width=2);d.text((a[0]-6,b[1]+5),'牌库',font=FONT,fill=P['silver'])
    for x in (70,1370):d.text((x,280),'外围环境\n后续建模\n不得遮选区',font=FONT,fill=P['silver'])
    d.text((40,1050),'尺寸：桌体19×14 / 牌体1×1.5。青绿框内为交互区，禁止装饰覆盖；HUD为独立屏幕层。',font=FONT,fill=P['light'])
    d.text((40,1090),'暖主光：左上；冷补光：右后（无阴影）；周边尘点限桌沿。UV阶段：桌面平铺 / 模型雕饰待建模。',font=FONT,fill=P['silver'])
    path=HERE/'previews/top-view.png';path.parent.mkdir(parents=True,exist_ok=True);im.save(path)
    im=Image.new('RGB',(1600,1000),'#181E20');d=ImageDraw.Draw(im)
    d.text((40,30),'VIS-02 · 材质与装配拆解 / 不代表已完成模型',font=FONT,fill=P['light'])
    for i,(label,c) in enumerate([('动态卡牌 / 原卡框、插画、标题和状态','#70BDA0'),('区域纹样 / 低对比透明平面，文字独立','#BA914C'),('平整酒红皮革面 / leather.tres','#582938'),('古金沿 / antique_gold.tres','#BA914C'),('暗木主体 / dark_wood.tres','#30201D')]):
        y=140+i*135;points=[(240,y),(940,y),(1060,y+55),(360,y+55)];d.polygon(points,fill=c);d.line(points+[points[0]],fill=P['light'],width=2);d.text((1090,y+14),label,font=ImageFont.truetype(str(PROJECT/'assets/fonts/NotoSansCJKsc-Regular.otf'),18),fill=P['light'])
    d.text((40,900),'侧边倒角、雕花、环境模块、牌堆代理：MOD-01～04 待建模。environment_stone.tres 已备，不预设新UV。',font=FONT,fill=P['silver'])
    im.save(HERE/'previews/exploded.png')

def gallery():
    previews=HERE/'previews';previews.mkdir(exist_ok=True)
    thumbs=[]
    for e in entries:
        if '/sheets/' in e['file'] or not(e['file'].endswith('_basecolor.png') or '/zones/' in e['file'] or '/ui/' in e['file'] or '/vfx/fx_' in e['file']):continue
        im=Image.open(ART/e['file']).convert('RGBA');im.thumbnail((220,160),Image.Resampling.NEAREST)
        thumbs.append((e,im.copy()))
    sheet=Image.new('RGB',(1400,math.ceil(len(thumbs)/5)*215+70),'#181E20');d=ImageDraw.Draw(sheet)
    d.text((25,15),'A 古籍金饰 · v1.5 非建模资源 / 独立原生组件',font=FONT,fill=P['light'])
    for i,(e,im) in enumerate(thumbs):
        x=(i%5)*280+20;y=(i//5)*215+65;sheet.paste(im,(x,y),im);d.text((x,y+166),e['id'],font=ImageFont.truetype(str(PROJECT/'assets/fonts/NotoSansCJKsc-Regular.otf'),14),fill=P['light'])
    sheet.save(previews/'contact-sheet.png')
    html=['<!doctype html><meta charset="utf-8"><title>WizardCard v1.5 非建模资产</title><style>body{background:#181e20;color:#f1e4c7;font-family:sans-serif;max-width:1400px;margin:auto}a{color:#70bda0}.grid{display:grid;grid-template-columns:repeat(5,1fr);gap:16px}img{max-width:100%;background:#282327;image-rendering:pixelated}figure{margin:0;padding:12px;border:1px solid #634837}small{display:block;overflow-wrap:anywhere}</style><h1>A 古籍金饰 · 非建模资源</h1><p>源图、可导入材质、特效场景、动作规范；模型暂缓。图中文字仅用于清单，运行组件不烘焙文字。</p><p><a href="../../../../docs/art-resource-status-v1.5.md">需求核对与接入状态</a> · <a href="previews/top-view.png">顶视图</a> · <a href="previews/exploded.png">拆解图</a></p><div class="grid">']
    html[-1]=html[-1].replace('<div class="grid">','<h2>当前镜头实机样稿</h2><img style="image-rendering:auto;width:100%" src="previews/engine/busy-hud-1920x1080.png"><p><a href="previews/engine/empty-1920x1080.png">空场fixture</a> · <a href="previews/engine/busy-hud-1280x720.png">720p</a> · <a href="previews/engine/busy-hud-1600x1000.png">16:10</a> · <a href="previews/engine/effects-normal.png">正常特效</a> · <a href="previews/engine/effects-reduced.png">精简特效</a></p><h2>独立资源</h2><div class="grid">')
    for e,im in thumbs:html.append(f'<figure><img src="../../{e["file"]}"><small>{e["id"]} · {e["requirement"]}</small></figure>')
    html.append('</div>');(HERE/'gallery.html').write_text(''.join(html),encoding='utf-8')

def main():
    for name,color,r,m,kind in [('dark_wood','#30201D',.78,0,'wood'),('leather','#582938',.86,0,'leather'),('antique_gold','#BA914C',.48,1,'gold'),('environment_stone','#343E3D',.92,0,'stone'),('paper_edge','#C5AD7B',.9,0,'leather')]:
        pbr(name,color,r,m,kind,512 if name=='paper_edge' else 2048)
    zones();effects();ui();animations();diagrams();gallery()
    manifest={'version':'wizardcard_a_gilded_nonmodel_v15','date':'2026-10-07','requirements':'docs/art-resource-requirements-godot-v1.5.md',
              'renderer':'Godot 4.7.2 Compatibility','assets':entries,'effects':[{'name':n,'loop':l,'seconds':d} for n,c,l,d in EFFECTS],
              'source':'assets/art/design/nonmodel-v15/build_assets.py','preserves':['30 card illustrations','A v2 314 images','legendary golden-yellow gemstone'],
              'model_delivery':False,'integration_record':'godot/art-integration.json'}
    textfile('manifest.json',json.dumps(manifest,ensure_ascii=False,indent=2))
    print(json.dumps({'png_count':len(entries),'material_sets':5,'effects':len(EFFECTS),'out':str(OUT)},ensure_ascii=False))

if __name__=='__main__':main()
