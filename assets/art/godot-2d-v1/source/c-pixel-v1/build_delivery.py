"""Validate and organize the new asset batch; create HTML review, specs and ZIP.

Only reads AI pixels and existing illustrations. Native rendering is build_native.py.
"""
from pathlib import Path
import json, hashlib, shutil, zipfile, re, collections
from PIL import Image
import numpy as np

SRC=Path(__file__).resolve().parent; ROOT=SRC.parents[1]; PROJECT=ROOT.parents[2]
MAN=ROOT/'manifests'/'c-pixel-v1'; OUT=ROOT/'export'/'c-pixel-v1'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def text(p,t):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(t,encoding='utf-8',newline='\n')
def write(p,v):text(p,json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def make_ai():
    rows=[]
    for j in read(SRC/'ai_sources.json'):
        path=ROOT/j['file']; src=SRC/'raster'/path.name;src.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,src)
        im=Image.open(path)
        req=['BOARD-01'] if j['kind']=='background' else ['STYLE-04'] if j['kind']=='brand' else ['CARD-03'] if 'back' in j['id'] else ['CARD-01','CARD-02']
        row={'id':j['id'],'file':j['file'],'source':src.relative_to(ROOT).as_posix(),'size':list(im.size),'requirements':req,'version':1,'status':'待接入','sampling':'nearest','origin':'OpenAI built-in image_gen; unmodified raster','license':'AI-generated project artwork; no third-party stock claim','sha256':digest(path),'promptRecord':'source/c-pixel-v1/ai_sources.json','hasTransparency':j['transparent']}
        if '.detail.' in j['id']:
            row.update(cardType=j['id'].split('.')[-2],view='detail',normalizedLayout={'art':[.12,.165,.77,.355],'name':[.12,.075,.62,.055],'effect':[.13,.66,.73,.2],'rarityAnchor':[.84,.58],'topValue':[.87,.105],'bottomValue':[.135,.88]},layoutNote='Approximate normalized anchors; inspect body at target scale. Native template has exact pixel rectangles.')
        rows.append(row)
    return rows
def illustrations():
    mapping=read(PROJECT/'assets/art/runtime.json')['cards']; cards=read(PROJECT/'assets/cards.json')['cards'];rows=[]
    for c in cards:
        p=PROJECT/'assets/art'/mapping[c['id']]
        rows.append({'id':c['id'],'file':p.relative_to(PROJECT).as_posix(),'sha256':digest(p)})
    prior=MAN/'preserved-illustrations.json'
    if prior.exists():assert read(prior)==rows,'Existing illustration changed'
    else:write(prior,rows)
    return rows,cards,mapping
def validate(rows):
    errors=[]; checks=[];palette={tuple(int(h[i:i+2],16) for i in (1,3,5)) for h in read(MAN/'native-assets.json')['palette'].values()}
    ids=set()
    for a in rows:
        p=ROOT/a['file'];im=Image.open(p).convert('RGBA');arr=np.asarray(im);alpha=arr[:,:,3]
        native=a['origin'].startswith('new');rgb={tuple(v) for v in np.unique(arr[alpha>0,:3],axis=0)} if native else set()
        item={'id':a['id'],'sizeCorrect':list(im.size)==a['size'],'sourcePresent':(ROOT/a['source']).is_file(),'shaCorrect':digest(p)==a['sha256']}
        if native:
            item.update(binaryAlpha=set(np.unique(alpha))<={0,255},paletteCorrect=rgb<=palette)
            if 'artRect' in a:
                x,y,w,h=a['artRect'];item['artWindowTransparent']=bool((alpha[y:y+h,x:x+w]==0).all())
            if 'frameSize' in a:item['sheetValid']=im.size==(a['frameSize'][0]*a['frames'],a['frameSize'][1])
        elif a.get('hasTransparency'):
            item['transparentPixelsExist']=bool((alpha==0).any());item['rasterSourceMatches']=digest(ROOT/a['source'])==digest(p)
            if '.detail.' in a['id']:item['artWindowCenterTransparent']=int(alpha[int(im.height*.35),im.width//2])==0
        for k,v in item.items():
            if isinstance(v,bool) and not v:errors.append(a['id']+': '+k)
        if a['id'] in ids:errors.append('duplicate '+a['id'])
        ids.add(a['id']);checks.append(item)
    result={'ok':not errors,'png':len(rows),'nativeSvg':sum(a['origin'].startswith('new') for a in rows),'aiRaster':sum(not a['origin'].startswith('new') for a in rows),'checks':checks,'errors':errors,'notVerified':['Godot frontend integration','real game lifecycle and hidden-information authority','human DPI/IME/input/network acceptance','full-game performance'],'aiNote':'AI original colors and soft alpha preserved; native export exact 32-color binary alpha.'}
    write(MAN/'validation.json',result)
    if errors:raise ValueError(errors[:12])
    return result
def motion():
    moves={
    'draw':{'duration':.32,'track':'position','from':'deckAnchor','to':'handAnchor','easing':'cubicOut'},
    'place':{'duration':.26,'track':'position','from':'dragAnchor','to':'zoneAnchor','easing':'cubicOut'},
    'flip':{'duration':.28,'track':'scaleX','keyframes':[[0,1],[.14,0],[.28,1]],'swapTextureAt':.14},
    'raise':{'duration':.12,'track':'visualOffsetY','keyframes':[[0,0],[.12,-12]],'easing':'quadOut'},
    'return':{'duration':.2,'track':'position','from':'cancelAnchor','to':'originalAnchor','easing':'quadOut'},
    'discard':{'duration':.28,'track':'position','from':'cardAnchor','to':'ashAnchor','easing':'cubicIn'},
    'page-enter':{'duration':.18,'track':'opacity','keyframes':[[0,0],[.18,1]]},
    'popup-enter':{'duration':.16,'track':'scale','keyframes':[[0,.96],[.16,1]]},
    'opening':{'duration':.45,'track':'opacity','keyframes':[[0,0],[.45,1]]},
    'victory':{'duration':.45,'track':'scale','keyframes':[[0,.9],[.45,1]]},
    'defeat':{'duration':.32,'track':'opacity','keyframes':[[0,0],[.32,1]]},
    'draw-result':{'duration':.32,'track':'opacity','keyframes':[[0,0],[.32,1]]}}
    for m in moves.values():m.update(reduced={'duration':min(m['duration'],.1),'cameraMotion':False},target='Visual child only; never move input root or submit rules')
    write(MAN/'motion.json',{'requirements':['FX-01','FX-06'],'clips':moves,'effectsPolicy':{'spawnOnlyOnConfirmedVisualCue':True,'facedownAnonymous':True,'loopAllowed':['analysis-loop'],'reducedLoops':False,'maxSimultaneous':12,'clearOnPauseHandoffOrExit':True,'clearOnComplete':True,'numbersAndSourceLabels':'real-time core projection','cancelDoesNotPlaySuccess':True,'endCleanup':'one batch sweep, not N repeated flashes'}})
def docs(rows,v,originals):
    from build_native import FX
    sections={'STYLE':4,'PAGE':12,'CARD':7,'BOARD':13,'FX':7}
    cover=['# 新版 C 像素资产：需求覆盖与接入边界','', '依据 docs/art-resource-requirements-godot-2d.md，采用用户选定 C 风格及最新五条横向解析行。用户强调的旧卡框信息结构作为全部新卡体依据。','', '本批制作资源与页面设计预览；不等于 Godot 全前端或新版规则已经实现。','', '| 编号 | 本批交付 | 当前状态 |','|---|---|---|']
    descriptions={
    'STYLE-01':'同一素材装配主菜单/复杂对局/构筑 HTML，复用原插画','STYLE-02':'32色、字号、间距、锚点、缩放规范','STYLE-03':'通用控件及 normal/hover/pressed/focus/disabled；输入、切换、滑条、分页、滚动、加载/空态','STYLE-04':'新 WizardCard 透明标题与展示规范',
    'CARD-01':'5类 AI 卡体＋5类精确可编辑模板；阵法分隔配方区','CARD-02':'5×4手牌/场内/构筑/详情独立模板及可滚动阅读','CARD-03':'统一匿名背面、主/副牌堆外围组件','CARD-04':'5独立宝石，颜色/切形/文本联合标识','CARD-05':'5类型、4速度、等级/位阶各1–7、10属性/配方图标','CARD-06':'解析/盖伏/等待/附着等状态图标','CARD-07':'缺图与失效ID图标、阅读和移除状态',
    'BOARD-01':'背景/主战区/外缘三独立新层，另有AI环境背景','BOARD-02':'三主区域与阵法/手牌/主副牌堆组件；解析行可重复','BOARD-03':'宿主关联线、分组框及角标','BOARD-04':'生命/临时生命/魔素/荷载/上限及三档提示','BOARD-05':'五阶段图标、时点/优先权/放弃响应设计','BOARD-06':'手牌、三种牌堆、空牌库与递增枯竭示例','BOARD-07':'独立阅读面板、操作条、最终确认','BOARD-08':'C链节/逆序/优先权/取消/4速不可响应','BOARD-09':'可滚动日志与公开区域浏览样例','BOARD-10':'六态透明叠加框与规则说明','BOARD-11':'预览/合法/无效/返回四态','BOARD-12':'副卡组配方材料/确认/取消/额度状态','BOARD-13':'盖伏零支付、反转费用、咒符等待、不可发动状态',
    'FX-01':'6类移动曲线 JSON，锚点/视觉子节点规范','FX-02':'开始/进行/完成遮罩及正常/精简帧表','FX-03':'准备/发动/结算/反制4类序列','FX-04':'8种来源/资源序列，临时生命到期不是伤害','FX-05':'附着/阵毁/普通离场3种序列','FX-06':'4结局/开局纹章，页面/弹窗/结局曲线','FX-07':'盖伏/反转/送灰烬/设置/替换/结束批量清理6种序列'}
    pageNames=['主菜单','本地配置/初始阵法','AI配置','卡组列表','卡组编辑器','联机入口/房间','对局与决策','新版教学','设置/局内菜单','隐私接手/终局','断线/重连/恢复','确认/故障/未保存']
    for group,n in sections.items():
        for i in range(1,n+1):
            key=f'{group}-{i:02}';body=descriptions.get(key,pageNames[i-1]+'及关键状态 HTML，见页面下拉菜单' if group=='PAGE' else '')
            cover.append(f'| {key} | {body} | 已制作 / 待接入；页面功能待实现 |')
    cover+=['','## 保留与未验收','',f'- 原 {len(originals)} 张插画未改，SHA256清单记录独立引用；原字体OFL保留。','- AI卡体是原始栅格文件，不宣称可编辑分层；同类原生模板有可编辑SVG。AI卡体之间有细微纹理/边饰差异，锚点为近似，正式接入需按目标尺寸校准。','- C新卡体沿用指定布局，周围宝石为类型装饰；中央元数据栏右侧独立大宝石才表示稀有度，并辅以文字，不能把所有装饰晶石当稀有度。','- 原型显示规则条件和计数均为设计数据；未实现的规则、同名限制、配方/速度窗口、临时生命结算等仍以核心与正式规则为准。','- 浏览器装配/可见性检查不替代Godot导入、最终IME/DPI/拖放/联网/隐私验收。','- 场地演示五行不固定规则阵法上限；左右牌堆只显示公开状态，对手私有牌不露正面。','- FX序列为新原生像素图形，不是AI逐帧生成；帧表/锚点/混合/循环在manifest，动画曲线另见motion.json。','- PNG独立组件和SVG源稿是运行素材；HTML及截图是评审辅助，不能裁成背景代替程序UI。','']
    text(MAN/'coverage.md','\n'.join(cover))
    style='''# C 版制作与装配规范

## 视觉
清爽16位像素奇幻，深蓝绿低对比底、铜金阶梯边角、浅羊皮纸阅读面。主体纹理密度参考已确认C版；控件更克制，确保中文净空。原生资产严格32色，AI原件近似匹配，不强行量化或裁切。

## 文字与布局
复用 assets/fonts/NotoSansCJKsc-Regular.otf，授权 assets/fonts/OFL.txt。正文建议16–18px、辅助14px、标题24–28px；小卡只显示名称/必要数字与短状态，全文放在独立可滚动详情。长名省略并支持详情完整展示。8px基础间距、16px卡片间隙、24px外边距。不要强行把全部1920页面整体缩小到720p。

主菜单/构筑/设置按容器重排。对战中解析行水平伸展，左宿主锚定、右动态布置卡牌；上下双方各复用同一组组件。副卡组入口在主卡组上方，灰烬在另一侧。施法槽由投影提供，0/少量/多量由容器调整。

## 卡牌
顶部卡名左、数值圆槽右；大插画窗透明；插画下面类型/速度左、稀有度右；下部效果框；底左圆形数值、底右编号。阵法下部有配方分隔。AI细节卡体是高分辨率栅格原件；手牌/场内/构筑/详情原生模板采用四种明确尺寸，不能直接把大卡缩小后塞全文。具体字段值由程序分配，不将两个圆槽强制解释为某个未经定稿的费用。

## 图层与缩放
背景z0 → 主战区z1 → 外缘z2 → 区域z10 → 卡片z20 → 状态框z30 → 独立HUD/操作z40 → 决策弹窗z60 → 接手/断线隐私遮挡z80。原生组件 nearest，整数像素边界；AI像素纹理按推荐整数比例观察，小尺寸使用原生模板。九宫格用manifest边距；不要拉伸角图，横向解析行靠九宫格扩展。

## 交互
hover浅金轮廓；selected青色角标；legal绿色勾；target紫色准星；成本候选琥珀菱形；成本已选绿勾。叠加顺序hover → 选中 → 候选 → 已选目标/成本，文字需说明用途。落点合法/非法同时显示形状，拖放确认前不支付，取消返回原位不播放成功释放。无合法操作仍可读详情。

加载/保存/重连不可丢失编辑与对局状态。联机局内菜单不显示“已暂停对方”。接手遮挡所有私有牌和目标候选。盖伏背面与匿名特效不能暴露解析/咒符身份。副卡组对手可见信息由正式规则决定。

## 效果
64x64帧，每组8帧16fps，精简4帧24fps；alpha混合、UV图集留2px外距。循环只允许正常模式解析进行；暂停/接手/退出全部清理，短特效播放结束移除，最多12并发。伤害/过载/枯竭各有独立来源文字，临时生命到期不能画成伤害数字。4速不可响应图标与反制特效分开。取消材料选择无完成设置演出。关闭镜头动作，时间缩短不影响输入根节点。曲线只作用Visual子节点。
'''
    text(MAN/'integration.md',style)
    lines=['# C 版逐文件清单','',f'{len(rows)}个PNG资源；{v["nativeSvg"]}份原生SVG源稿；{v["aiRaster"]}个AI栅格原件。均记录稳定ID、来源、尺寸与SHA。','', '| ID | 尺寸 | PNG | 源稿 | 覆盖 |','|---|---|---|---|---|']
    for a in rows:lines.append(f'| `{a["id"]}` | {a["size"][0]}×{a["size"][1]} | [PNG](../../{a["file"]}) | [源](../../{a["source"]}) | {", ".join(a["requirements"])} |')
    text(MAN/'ASSET_LIST.md','\n'.join(lines)+'\n')
def review(rows,cards,mapping):
    types={'analytic':'analysis','action':'incantation','incantation':'incantation','formation':'formation','rune':'rune','talisman':'talisman'}
    data={'assets':rows,'palette':read(MAN/'native-assets.json')['palette'],'cards':[dict(c,designType=types.get(c['type'],'analysis'),art='../'+mapping[c['id']]) for c in cards]}
    html=(SRC/'review.template.html').read_text(encoding='utf-8').replace('__DATA__',json.dumps(data,ensure_ascii=False).replace('</',r'<\/'))
    text(ROOT/'index.html',html)
    text(ROOT/'README.md',f'''# WizardCard新版2D · C像素奇幻资源包

用户选定C视觉风格。新版资源共{len(rows)}个PNG，原生SVG源稿{sum(a['origin'].startswith('new') for a in rows)}份，另有8个AI栅格原件。五类卡体参考用户旧卡框布局；场地采用左阵法锚点＋右横向解析行，副卡组在主卡组上方。原30插图保留。

- [资产与12类页面/状态装配预览](index.html)
- [逐文件清单](manifests/c-pixel-v1/ASSET_LIST.md)
- [需求覆盖与待接入项](manifests/c-pixel-v1/coverage.md)
- [规范与切片/图层/交互](manifests/c-pixel-v1/integration.md)
- [运行资源manifest](manifests/c-pixel-v1/manifest.json)
- [运动曲线](manifests/c-pixel-v1/motion.json)
- [资源验证](manifests/c-pixel-v1/validation.json)
- [AI实际提示词](source/c-pixel-v1/ai_sources.json)
- [可复现原生生成器](source/c-pixel-v1/build_native.py)

资源制作、设计状态预览与程序功能分开：本包未实现Godot前端或新版规则。正式导入、真人DPI/IME/拖放/隐私/网络验收仍待完成。旧概念稿保留，不计入这批新资产数量。
''')
def package(originals):
    dst=PROJECT/'build/art-delivery/WizardCard-C-2D-v1.zip';dst.parent.mkdir(parents=True,exist_ok=True)
    included=[ROOT/'README.md',ROOT/'index.html']
    included+=list((ROOT/'export/c-pixel-v1').rglob('*'))+list(SRC.rglob('*'))+list(MAN.rglob('*'))
    included+=list((ROOT/'concepts/c-pixel-v1/verified').rglob('*'))
    included+=[PROJECT/r['file'] for r in originals]
    included+=[PROJECT/'assets/fonts/NotoSansCJKsc-Regular.otf',PROJECT/'assets/fonts/OFL.txt']
    with zipfile.ZipFile(dst,'w',zipfile.ZIP_DEFLATED) as z:
        for p in included:
            if p.is_file() and '__pycache__' not in p.parts and p != MAN/'package.json':z.write(p,p.relative_to(PROJECT).as_posix())
    write(MAN/'package.json',{'file':dst.relative_to(PROJECT).as_posix(),'bytes':dst.stat().st_size,'sha256':digest(dst),'addonRoot':'extract into project root; includes referenced illustration and licensed font files','derivedArtifact':True})
    return dst
def main():
    originals,cards,mapping=illustrations();native=read(MAN/'native-assets.json')['assets'];rows=native+make_ai();v=validate(rows)
    write(MAN/'manifest.json',{'version':1,'style':'C clean pixel fantasy','runtimeIntegrated':False,'nativePalette':read(MAN/'native-assets.json')['palette'],'assets':rows,'existingIllustrations':'preserved-illustrations.json','font':'assets/fonts/NotoSansCJKsc-Regular.otf','fontLicense':'assets/fonts/OFL.txt','requirements':'docs/art-resource-requirements-godot-2d.md','sourceContract':'source/c-pixel-v1/spec_lock.md'})
    motion();docs(rows,v,originals);review(rows,cards,mapping);archive=package(originals)
    print(json.dumps({'ok':v['ok'],'png':len(rows),'svg':v['nativeSvg'],'ai':v['aiRaster'],'illustrations':len(originals),'zip':str(archive)},ensure_ascii=False))
if __name__=='__main__':main()
