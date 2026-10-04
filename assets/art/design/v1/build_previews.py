"""Editable, labeled UI design compositions; never used as game background textures."""
from build_ui import *

PREV=ROOT/'previews'
CARDS={c['id']:c for c in json.loads((ASSETS/'cards.json').read_text(encoding='utf-8'))['cards']}
RARE={'common':('普通','white'),'uncommon':('罕见','green'),'rare':('稀有','mint')}
FLOW_RECORDS=[]

def ui(c,name,category,x,y,w=None,h=None):c.sprite(UI/category/f'{name}.png',x,y,w,h)
def art(c,ident,x,y,w,h):c.sprite(ART/'cards'/f'{ident}.png',x,y,w,h,True)
def ico(c,name,x,y,size=20):ui(c,name,'icons',x,y,size,size)

def card(c,ident,x,y,mode='hand',state=None,instance='',base=False,seal=False,w=None,h=None):
    d=CARDS[ident];w=w or {'hand':101,'strip':125,'portrait':90}[mode];h=h or {'hand':118,'strip':44,'portrait':112}[mode]
    ui(c,f'frame_{d["type"]}_{mode}','cards',x,y,w,h)
    c.layer('dynamic_card_text')
    if mode=='strip':
        art(c,ident,x+3,y+6,25,h-12);c.text(d['name'],x+34,y+5,12)
        c.text(instance or TYPES[d['type']][0],x+34,y+24,10,'silver')
    else:
        c.text(d['name'],x+11,y+6,11);art(c,ident,x+5,y+24,w-10,h-56)
        if mode=='hand' and d.get('rank'):
            c.rect(x+w-19,y+25,14,14,'panel');c.text(str(d['rank']),x+w-15,y+26,11,'lightgold')
        line=(f'解 {d["cost"]}  /  施 {d["castCost"]}' if d['type']=='analytic' else f'魔素 {d.get("cost",0)}') if mode=='hand' else instance or TYPES[d['type']][0]
        c.text(line,x+7,y+h-27,10,'mint');c.text(TYPES[d['type']][0],x+7,y+h-15,9,'silver')
        ui(c,'rarity_'+d['rarity'],'cards',x+w-30,y+h-14,25,7)
    if base:ico(c,'state_protected',x+w-18,y+2,15)
    if seal:
        c.rect(x+w-23,y+h-24,22,23,'bronze1');ico(c,'state_attached',x+w-21,y+h-22,18)
    if state:ui(c,'overlay_'+mode+'_'+state,'interaction',x-1,y-1,w+2,h+2)
    return (x,y,w,h)

def detail(c,ident,x=1360,y=330,inst='解析完成 · 可准备施法',summary=None,noactions=False):
    d=CARDS[ident];ui(c,'frame_'+d['type']+'_detail','cards',x,y)
    c.layer('readonly_detail')
    c.text(d['name'],x+12,y+10,18,'lightgold');ico(c,'close',x+184,y+9,19)
    c.text(TYPES[d['type']][0],x+12,y+35,12,TYPES[d['type']][1]);c.text(RARE[d['rarity']][0],x+166,y+35,11,RARE[d['rarity']][1])
    art(c,ident,x+12,y+53,191,94)
    if d['type']=='analytic':c.text(f'解析 {d["cost"]}   /   施法 {d["castCost"]}',x+12,y+164,13,'mint')
    else:c.text(f'魔素费用  {d.get("cost",0)}',x+12,y+164,13,'mint')
    if d['type']=='analytic':
        c.text(f'位阶 {d["rank"]} · 解析 {d.get("analysisTurns",1)} 回合 · '+('持续' if d.get('duration') else '瞬发'),x+12,y+185,11,'silver')
        c.text('目标：'+{'opponent':'对手','self':'自己','empty_enemy_formation':'对手空闲扩展阵法'}[d['target']],x+12,y+201,10,'silver')
    elif d['type']=='word':c.text(f'位阶 {d["rank"]} · 对手准备时可响应',x+12,y+187,11,'silver')
    elif d['type']=='formation':c.text('设置 0 · 替换 2 魔素',x+12,y+187,11,'silver')
    elif d['type']=='seal':c.text('宿主：己方阵法 / 解析区法术',x+12,y+187,11,'silver')
    else:c.text('目标：'+('对手' if d.get('target')=='opponent' else '自己')+f' · 使用负担 {d.get("burden",0)}',x+12,y+187,11,'silver')
    c.wrapped(d['text'],x+12,y+218,191,12,'text',16,4)
    c.wrapped(inst,x+12,y+287,190,11,'silver',16,2)
    if summary:c.wrapped(summary,x+12,y+320,190,10,'mint',14,2)
    elif noactions:c.text('当前无合法操作 · 仍可查看',x+12,y+328,10,'silver')
    else:c.text('规则与实例信息 · 只读',x+12,y+330,10,'gray')

def button(c,label,x,y,w=160,kind='primary',state='normal'):
    ui(c,f'button_{kind}_{state}','interaction',x,y,w,36)
    textwidth=c.d.textlength(label,font=font(14));c.text(label,x+max(9,int((w-textwidth)/2)),y+10+(state=='pressed'),14,'silver' if state=='disabled' else 'text')

def actionbar(c,anchor,labels=('开始解析',),confirm=False,kind='primary'):
    ax,ay,aw,ah=anchor;width=244 if confirm else len(labels)*168-8
    x=max(260,min(ax+aw//2-width//2,1340-width));y=max(76,ay-44)
    c.layer('floating_execution')
    if confirm:button(c,'确认操作',x,y,148);button(c,'取消',x+156,y,88,'secondary')
    else:
        for j,label in enumerate(labels):button(c,label,x+j*168,y,160,kind)
    return (x,y,width,36)

def mirror(r):
    x,y,w,h=r;return 1600-x-w,960-y-h,w,h

def region(c,name,r,label):
    x,y,w,h=r;ui(c,'region_'+name,'board',x,y,w,h);c.text(label,x+10,y+6,12,'silver')

def hud(c,enemy=False,complex=False):
    x,y=(1360,127) if enemy else (25,642);ui(c,'hud','board',x,y)
    c.text('对手 · 玩家 2' if enemy else '本方 · 玩家 1',x+13,y+11,15,'lightgold')
    ico(c,'life',x+14,y+49,28);c.text('24' if complex and enemy else '30',x+55,y+42,33)
    ico(c,'mana',x+13,y+96,22);c.text('魔素  6 / 12' if complex else '魔素  1 / 12',x+45,y+100,15,'mint')
    ico(c,'load',x+13,y+131,21);ico(c,'capacity',x+110,y+132,20)
    c.text('12' if complex else '0',x+45,y+135,16);c.text('/ 15' if complex else '/ 8',x+143,y+135,16)
    c.text(('手牌 5' if enemy else '手牌 9')+'   ·   牌库 18' if complex else '手牌 5   ·   牌库 25',x+14,y+173,12,'silver')
    dx,dy=(107,135) if enemy else (1388,696);ui(c,'card_back_deck','cards',dx,dy)
    c.rect(dx+22,dy+87,55,27,'panel');c.text('18' if complex else '25',dx+37,dy+91,20)

def board(c,complex=False,handoff=False):
    ui(c,'match_background','board',0,0);ui(c,'board_decoration','board',0,0)
    c.layer('dynamic_headings');c.text('巫师牌',27,20,29,'lightgold');c.text('ARCANE DUEL',28,58,11,'silver')
    names=['抽卡','准备','主要','施法','结束']
    for i,name in enumerate(names):
        xx=330+i*114;ui(c,'phase_current' if i==2 else 'phase_inactive','board',xx,22);c.text(name,xx+29,25,14,'mint' if i==2 else 'silver')
    c.text('第 6 回合 · 玩家 1 操作' if complex else '第 1 回合 · 玩家 1 操作',329,61,12,'silver')
    for enemy in (False,True):
        for name,r,label in [('casting',(260,487,1080,65),'施法区'),('words',(809,560,531,106),'言灵区'),('action',(809,674,531,49),'行动区'),('ash',(809,731,531,101),'灰烬区')]:region(c,name,mirror(r) if enemy else r,label)
        for row in range(5):
            r=(260,560+55*row,106,52);a=(373,560+55*row,427,52)
            if enemy:r=mirror(r);a=mirror(a)
            ui(c,'slot_empty','board',r[0],r[1]);region(c,'analysis',a,'解析行 '+str(row+1))
        r=(264,564,98,99);r=mirror(r) if enemy else r
        card(c,'balance',r[0],r[1],'portrait',instance='环位 2 / 3' if complex else '环位 0 / 3',base=True,w=r[2],h=r[3])
        # Body=2 spans slots 1 and 2; same connector color identifies the host row.
        ar=(368,570,6,91);ar=mirror(ar) if enemy else ar;c.line([(ar[0],ar[1]),(ar[0],ar[1]+ar[3])],'green',2)
        if complex:
            for ident,r in [('reservoir',(264,674,98,44)),('conduit',(264,729,98,99))]:
                if enemy:r=mirror(r)
                card(c,ident,r[0],r[1],'strip' if r[3]<80 else 'portrait',instance='1 / 1 环' if ident=='reservoir' else '0 / 1 环',w=r[2],h=r[3])
            hosted=[('fireball',(380,568,125,44),'解析 · 1'),('spark',(512,568,125,44),'解析完成'),('mend',(380,677,125,44),'解析 · 1')]
            for ident,r,txt in hosted:
                if enemy:r=mirror(r)
                card(c,ident,r[0],r[1],'strip',instance=txt,w=r[2],h=r[3],seal=ident=='spark')
            r=(272,512,125,34);r=mirror(r) if enemy else r;card(c,'ward',r[0],r[1],'strip',instance='持续 · 2',w=r[2],h=r[3])
            r=(820,585,101,75);r=mirror(r) if enemy else r;card(c,'barbs',r[0],r[1],'portrait',instance='已预置',w=r[2],h=r[3])
            for j,ident in enumerate(['recall','disrupt','clarity']):
                r=(820+j*132,757,125,44);r=mirror(r) if enemy else r;card(c,ident,r[0],r[1],'strip',instance='灰烬',w=r[2],h=r[3])
        if not complex:
            r=(820,700,300,18);r=mirror(r) if enemy else r;c.text('行动在此结算 · 不限次数',r[0],r[1],11,'gray')
        hud(c,enemy,complex)
    for i in range(5):ui(c,'card_back_opponent','cards',680+i*48,78)
    ui(c,'region_hand','board',260,862);c.text('手牌',174,902,19,'gold')
    if handoff:
        ui(c,'handoff','interaction',260,862);c.text('请将操作交给玩家 2',630,875,20,'lightgold');button(c,'确认接手 · 显示自己的手牌',580,924,440)
        c.text('公开区域保留；私有手牌、操作条、详情和日志不绘制。',509,906,13,'silver')
        return {}
    hand={}
    hand_ids=['unravel','fireball','ring','recall','reservoir','mend','barbs','clarity','disrupt'] if complex else ['unravel','fireball','ring','recall','reservoir']
    for i,ident in enumerate(hand_ids):hand[ident]=card(c,ident,270+i*115,871)
    button(c,'保存',1278,24,90,'secondary');button(c,'记录',1380,24,90,'secondary');button(c,'投降',1484,24,90,'danger')
    button(c,'结束主要阶段',1360,889,215);c.text(f'显示 1–{len(hand_ids)} / {len(hand_ids)}',1200,850,11,'silver')
    ui(c,'scroll_track','board',1120,848,70,4);ui(c,'scroll_thumb','board',1120,848,35,4)
    return hand

def choose_window(c,title,subtitle,choices,remaining='尚需选择 1 项',allow_skip=True):
    x,y=500,287;ui(c,'decision','interaction',x,y,600,368)
    c.text(title,x+20,y+12,21,'lightgold');c.text(subtitle,x+20,y+48,14,'silver');c.text(remaining,x+20,y+75,14,'mint')
    for i,label in enumerate(choices):
        button(c,label,x+20,y+111+i*43,560,'secondary')
    button(c,'跳过 / 停止' if allow_skip else '请完成必选任务',x+20,y+308,270,'secondary','normal' if allow_skip else 'disabled');button(c,'确认选择',x+310,y+308,270)

def scene(title,mode='inspect',ident='fireball',complex=True,decision=None,edge=False,flow_kind=''):
    if flow_kind=='02_formation' and mode=='inspect':complex=False
    c=Canvas(1600,1000);hands=board(c,complex,mode=='handoff')
    if mode=='handoff':return c
    anchor=hands.get(ident,hands['fireball']);c.layer('flow_state')
    if flow_kind=='04_prepare':
        anchor=card(c,'unravel',380,568,'strip',instance='解析完成',state='selected')
        c.rect(69,775,41,24,'panel');c.text('13',70,777,16)
    if flow_kind=='06_words' and mode in ('inspect','confirm'):
        region(c,'words',(809,560,531,106),'言灵区');c.rect(69,775,41,24,'panel');c.text('8',70,777,16)
    if edge:anchor=hands['disrupt'];ident='disrupt'
    if mode not in ('initial','complex','decision','log','victory','defeat','draw','surrender'):
        x,y,w,h=anchor;ui(c,'overlay_strip_selected' if h<80 else 'overlay_hand_selected','interaction',x-1,y-1,w+2,h+2)
        inst='操作源 · '+CARDS[ident]['name'];summary=None
        if mode=='confirm':
            d=CARDS[ident];summary=f'本次魔素 {d.get("castCost",d.get("cost",0))} · 新增荷载 '+str(d.get('castCost',d.get('burden',0)))
            if ident=='unravel':summary='施法魔素 2 · 荷载 +2\n额外弃牌：奥术回想'
            if flow_kind=='03_analysis':summary='解析魔素 3 · 荷载 +3\n宿主：基础均衡 #10'
            if flow_kind=='02_formation':summary='替换魔素 2 · 荷载 +0\n目标：空闲导流阵 #14'
            if flow_kind=='06_words':summary='预置魔素 4 · 荷载 +4'
            if flow_kind=='09_discard':summary='弃置：奥术回想\n剩余需弃：0；无魔素支付'
        detail(c,ident,inst=inst,summary=summary,noactions=mode=='noactions')
        step={'target':1,'replace':1,'cost':2,'confirm':3}.get(mode,0)
        ui(c,'stepper_'+str(step),'interaction',25,430,215,18)
        for k,label in enumerate(['查看','目标','成本','确认']):c.text(label,27+k*51,455,10,'mint' if k==step else 'silver')
        if mode=='inspect':
            labels={'formation':['设置阵法','替换阵法'],'analytic':['开始解析'],'action':['使用行动'],'word':['预置言灵'],'seal':['附加符文']}[CARDS[ident]['type']]
            if flow_kind=='04_prepare':labels=['准备施法']
            if flow_kind=='02_formation':labels=['设置阵法']
            actionbar(c,anchor,labels)
        elif mode=='confirm':
            actionbar(c,anchor,confirm=True)
            target={'03_analysis':'基础均衡 #10','02_formation':'空闲导流阵 #14','06_words':'预置于本方言灵区','05_action':'本方抽2张，临时荷载+1','07_seal':'基础均衡 #10','09_discard':'弃置奥术回想，无效果目标'}.get(flow_kind,'对手空闲导流阵 #24' if ident=='unravel' else '当前合法候选')
            c.rect(315,56,900,22,'bg');c.text('确认：'+CARDS[ident]['name']+' · '+target,320,59,14,'mint')
        elif mode in ('target','replace'):
            c.rect(315,56,925,22,'bg');c.text('选择合法扩展阵法（基础阵法不可选）' if mode=='replace' else '选择高亮的合法宿主 / 卡牌目标',320,59,14,'lightgold')
            button(c,'取消选择',1100,26,120,'secondary')
            candidates=[(264,729,98,99),(264,674,98,44)] if mode=='replace' else [(264,564,98,99)] if ident=='fireball' else [(264,564,98,99),(264,674,98,44),(264,729,98,99),(380,568,125,44),(380,677,125,44)] if ident=='ring' else [mirror((264,729,98,99))]
            for x,y,w,h in candidates:ui(c,'overlay_strip_target_candidate','interaction',x-2,y-2,w+4,h+4)
            x,y,w,h=candidates[0];ui(c,'overlay_strip_target_selected','interaction',x-2,y-2,w+4,h+4)
        elif mode=='cost':
            c.rect(315,56,925,22,'bg');c.text('额外成本 · 选择 1 张手牌弃置（不是效果目标）',320,59,14,'violet');button(c,'取消选择',1100,26,120,'secondary')
            for name in ['recall','mend','clarity']:
                x,y,w,h=hands[name];ui(c,'overlay_hand_cost_candidate','interaction',x,y,w,h)
            x,y,w,h=hands['recall'];ui(c,'overlay_hand_cost_selected','interaction',x,y,w,h)
        elif mode=='attached':
            detail(c,'ring',inst='宿主 #12 · 火花术\n符文附着角标独立点击');actionbar(c,(512,568,125,44),['拆除符文'])
        elif mode=='cancelled':
            c.rect(315,56,925,22,'bg');c.text('已取消选择 · 资源尚未扣除',340,62,14,'silver');actionbar(c,anchor,['准备施法'])
        elif mode=='prepare_cancelled':
            card(c,'unravel',380,568,'strip',instance='准备被取消',state='selected');ico(c,'state_cancelled',476,588,18)
            c.rect(270+3*115,871,101,118,'panel');c.rect(66,741,150,28,'panel');c.text('魔素  4 / 12',70,742,15,'mint')
            c.rect(39,814,185,14,'panel');c.text('手牌 8   ·   牌库 18',39,815,12,'silver');c.rect(1200,848,135,14,'bg');c.text('已弃置奥术回想',1200,850,11,'silver')
            detail(c,'unravel',inst='法术留在解析区；本回合不可再准备',summary='已付魔素和弃牌不退\n施法荷载清零，解析荷载保留')
            c.rect(315,56,925,22,'bg');c.text('银光锐语取消准备 · 法术未销毁 · 支付不退',320,59,14,'coral')
    if mode=='decision':
        kind=decision or 'response'
        settings={
            'response':('言灵响应','对手宣告准备施法；选择自己的预置言灵。',['银光锐语 · 取消这次准备'],'可选响应，可跳过',True),
            'casting':('选择施法顺序','从待施放的法术中选择下一张。',['火球术 · 待施放','生命缝合 · 待施放'],'待释放 2 张',False),
            'trigger':('选择触发顺序','按当前核心提供的候选逐项处理。',['炽燃结界 #21 · 准备阶段伤害','炽燃结界 #22 · 准备阶段伤害'],'尚余 2 项',False),
            'discard':('结束阶段弃牌','手牌超出上限；与准备施法的额外成本区分。',['奥术回想','清心','蓄能阵'],'本次尚需弃置 1 张',False),
            'temporary':('移除临时荷载','只列临时来源，不修改绑定法术荷载。',['魔力扰动 · 可移除 2','奥术回想 · 可移除 1'],'清心可再移除 2 点',True),
            'overflow':('移除多余法术','容量变化后的强制任务；选合法候选。',['火花术 · 宿主 #10','生命缝合 · 宿主 #10'],'尚需移除 1 张',False),
        }
        args=settings[kind];choose_window(c,*args)
    if mode=='log':
        ui(c,'log_window','interaction',470,240,660,500);c.text('对局记录',493,253,23,'lightgold');ico(c,'close',1085,252,22)
        for i,s in enumerate(['玩家 1 开始解析火球术。','支付 3 魔素，绑定解析荷载 3。','玩家 1 结束主要阶段。','玩家 2 完成接手。','玩家 2 预置银光锐语。','记录按实际游戏事件动态生成。']):c.text(s,493,303+i*45,17,'silver')
    if mode in ('victory','defeat','draw','surrender'):
        ui(c,'result_window','interaction',540,280,520,400)
        result=mode if mode!='surrender' else 'defeat';ui(c,'result_'+result,'interaction',752,327)
        labels={'victory':'玩家 1 获胜','defeat':'玩家 1 败北','draw':'双方平局','surrender':'确认投降？'}
        c.text(labels[mode],670,408,29,'lightgold')
        reasons={'victory':'对手生命之核归零。','defeat':'当前荷载严格超过荷载容量。','draw':'双方在同一次状态检查满足败北条件。','surrender':'投降将立即结束本局。'}
        c.wrapped(reasons[mode],574,466,450,18,'silver')
        button(c,'确认投降' if mode=='surrender' else '保存并开始新对局',575,574,260,'danger' if mode=='surrender' else 'primary')
        button(c,'取消' if mode=='surrender' else '查看记录',845,574,180,'secondary')
    return c

def save_scene(slug,title,c,annotations=''):
    c.save(PREV/f'{slug}.png',PREV/f'{slug}.svg')
    FLOW_RECORDS.append({'id':slug,'title':title,'png':f'previews/{slug}.png','source':f'previews/{slug}.svg','annotation':annotations})

def sheets():
    c=Canvas(1600,1000,'bg');c.text('卡牌体系 / 五类 × 三种展示',40,27,31,'lightgold');c.text('类型独立标记 · 稀有度独立宝石 · 操作高亮在最外层',40,78,17,'silver')
    ids=['recall','fireball','barbs','balance','ring']
    for i,ident in enumerate(ids):
        x=40+i*312;c.text(TYPES[CARDS[ident]['type']][0],x,120,22,TYPES[CARDS[ident]['type']][1]);card(c,ident,x,163);card(c,ident,x+114,163,'portrait',instance='状态 / 计数');card(c,ident,x,306,'strip',instance='实例状态');detail(c,ident,x,389,inst='示例实例 · 状态独立显示')
    c.text('手牌 101×118    场内：卡条 125×44 / 竖版 90×112    详情 215×354（只读）',40,788,20,'silver')
    c.text('卡名、数值、费用、类型名、规则全文由程序动态绘制；本图仅为模板组合示例。',40,833,17,'silver')
    c.text('解析费用与施法费用分开显示；附着符文角标保持独立命中区域。',40,872,17,'mint');save_scene('cards_all_types','五类卡牌与三种展示规格',c)
    c=Canvas(1600,1000,'bg');c.text('图标与状态 / 32px 原生网格',40,30,30,'lightgold')
    for i,(name,label) in enumerate(ICONS.items()):
        col=i%8;row=i//8;x=42+col*194;y=110+row*113;ico(c,name,x,y,32);ico(c,name,x+42,y,24);c.text(label,x,y+43,13,'text');c.text(name,x,y+66,10,'silver')
    save_scene('icons_catalog','类型、属性、状态、资源与区域图标',c)
    c=Canvas(1600,1000,'bg');c.text('交互组件 / 形状与语义',40,30,30,'lightgold')
    for i,kind in enumerate(['primary','secondary','danger']):
        c.text(['主操作','次操作','危险操作'][i],40,110+i*72,18)
        for j,state in enumerate(['normal','hover','pressed','disabled']):button(c,['普通','悬停','按下','禁用'][j],200+j*220,103+i*72,190,kind,state)
    for i,state in enumerate(['hover','selected','target_candidate','target_selected','cost_candidate','cost_selected']):
        card(c,'fireball',55+i*248,370,state=state);c.text(['悬停','操作源选中','合法目标','当前目标','弃牌候选','弃牌已选'][i],55+i*248,510,16,'silver')
    for i,r in enumerate(['victory','defeat','draw']):ui(c,'result_'+r,'interaction',70+i*260,602);c.text(['胜利','失败','平局'][i],177+i*260,624,19)
    for i,t in enumerate(['success','no_action','wrong_target','rejected']):ui(c,'feedback_'+t,'interaction',40+(i%2)*650,730+(i//2)*74,600,48);c.text(['操作已完成','当前无合法操作，可继续查看','请选择高亮候选','操作被拒绝：显示核心原因'][i],100+(i%2)*650,746+(i//2)*74,17)
    save_scene('interaction_catalog','按钮、选择层级、结果和反馈样式',c)
    c=Canvas(1600,1000,'bg');c.text('卡背与隐私 / 独立可复用资源',40,30,30,'lightgold')
    for i,(n,w,h) in enumerate([('hand',101,118),('opponent',39,41),('deck',100,132)]):ui(c,'card_back_'+n,'cards',80+i*300,140,w*2,h*2);c.text(n,80+i*300,435,18,'silver')
    ui(c,'handoff','interaction',260,640);c.text('请将操作交给玩家 2',600,664,24,'lightgold');button(c,'确认接手 · 显示自己的手牌',580,712,440)
    c.text('换手时：不绘制私有手牌、详情、卡牌操作条、日志与相关提示。',260,817,20,'silver');c.text('公开区域仍可查看；接手后才重建私有组件。',260,856,20,'mint');save_scene('card_backs_privacy','通用卡背、牌库和换手遮挡',c)

def extra_sheets():
    c=Canvas(1600,1000,'bg');c.text('全卡池详情 / 13 张规则全文与属性',30,22,28,'lightgold')
    for i,ident in enumerate(CARDS):detail(c,ident,22+(i%7)*225,88+(i//7)*420,inst='实例：由核心提供状态与宿主')
    save_scene('cards_full_details','全卡池规则、位阶、解析时间与目标',c,'全部当前规则全文；后续更长文本使用详情内部滚动，执行按钮不进入详情。')
    c=Canvas(1600,1000,'bg');c.text('法术状态与关联 / 不把取消准备画成销毁',40,32,29,'lightgold')
    states=[('analyzing','解析中','解析剩余 1'),('analyzed','解析完成','可准备施法'),('prepared','待施放','已付施法段'),('active','持续生效','持续剩余 2'),('cancelled','准备被取消','本回合不可重试'),('protected','基础阵法保护','不可替换 / 拆除'),('attached','符文附着','角标单独可点')]
    for i,(state,title,inst) in enumerate(states):
        xx=48+(i%4)*386;yy=132+(i//4)*350;ident='balance' if state=='protected' else 'ward' if state=='active' else 'fireball'
        card(c,ident,xx,yy,'portrait',instance=inst,base=state=='protected',seal=state=='attached');ico(c,'state_'+state,xx+112,yy+4,32)
        c.text(title,xx+112,yy+54,20,'lightgold');c.text(inst,xx+112,yy+89,13,'silver')
    c.wrapped('取消准备：法术保留，施法荷载清零，解析荷载保留；魔素和额外弃牌不退。本回合不可重试。',48,858,1480,20,'coral')
    save_scene('card_state_examples','七类状态与关联标识组合',c)

def build():
    PREV.mkdir(parents=True,exist_ok=True);sheets();extra_sheets()
    save_scene('board_initial','初始场地',scene('初始','initial',complex=False),'双方按当前mirror公式布局；基础均衡占2槽，行动不限次数。')
    save_scene('board_complex','复杂场地',scene('复杂','complex'),'阵体2跨槽、解析宿主、持续计数、附着角标、资源与滚动提示。手工设计状态，非规则重放存档。')
    flows=[
      ('01_inspect','选中与查看','fireball',[('inspect','查看与合法操作'),('noactions','无合法操作仍展示详情')]),
      ('02_formation','设置与替换阵法','reservoir',[('inspect','设置/替换菜单'),('replace','选择合法扩展阵法'),('confirm','设置/替换确认')]),
      ('03_analysis','开始解析','fireball',[('inspect','开始解析'),('target','选择合法宿主'),('confirm','确认解析支付')]),
      ('04_prepare','准备施法与额外成本','unravel',[('inspect','查看法术'),('target','选择空闲扩展阵法'),('cost','选择额外弃牌成本'),('confirm','确认目标、成本与支付'),('prepare_cancelled','被言灵取消后的法术保留')]),
      ('05_action','使用行动','recall',[('inspect','使用行动菜单'),('confirm','无卡牌目标时直接确认')]),
      ('06_words','预置与触发言灵','barbs',[('inspect','预置言灵'),('confirm','预置确认'),('handoff','响应换手'),('decision','response')]),
      ('07_seal','附加与拆除符文','ring',[('inspect','附加符文'),('target','选择合法宿主'),('confirm','附加确认'),('attached','独立角标查看与拆除')]),
      ('08_casting','施法顺序选择','fireball',[('decision','casting')]),
      ('09_discard','结束阶段弃牌','recall',[('decision','discard'),('confirm','弃牌确认')]),
      ('10_temporary','移除临时荷载','clarity',[('decision','temporary')]),
      ('11_order','触发顺序与多余法术','ward',[('decision','trigger'),('decision','overflow')]),
      ('12_cancel','取消与重新选择','unravel',[('cost','尚未提交的成本选择'),('cancelled','Esc返回查看，资源不变')]),
      ('13_result','终局与重开','fireball',[('victory','胜利'),('defeat','失败'),('draw','平局'),('handoff','重开后换手')]),
    ]
    flow_index=[]
    for fid,title,ident,steps in flows:
        sequence=[]
        for i,(mode,label) in enumerate(steps):
            slug=f'flow_{fid}_{i+1:02}';c=scene(title,mode,ident,decision=label if mode=='decision' else None,flow_kind=fid)
            note=f'{title}：{label}。源卡、目标、额外成本与确认分层；仅确认提交后改变规则状态。'
            save_scene(slug,f'{title} · {label}',c,note);sequence.append(slug)
        flow_index.append({'id':fid,'title':title,'states':sequence})
    save_scene('edge_action_bar','右侧边缘操作条避让',scene('边缘','inspect',edge=True),'操作条右界不超过1340，避免与右侧只读详情面板重叠。')
    save_scene('log_window','记录窗口',scene('记录','log'),'记录支持关闭；换手时不绘制。')
    save_scene('surrender_confirm','投降确认',scene('投降','surrender'),'危险操作单独确认，取消不结束对局。')
    (ROOT/'flows.json').write_text(json.dumps({'flows':flow_index,'sheets':FLOW_RECORDS},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Exported {len(FLOW_RECORDS)} labeled PNG/SVG design states across {len(flows)} flows.')

if __name__=='__main__':build()
