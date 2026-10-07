"""Render the v1.5 review boards from current content and original art (no source-art edits).

Run with Pillow: python tools/generate_v15_design.py
Outputs the three review pages at all three agreed viewport sizes, plus a local gallery.
The board state is illustrative, not an engine screenshot or a playable UI.
"""
from __future__ import annotations

import html
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/art/design/v15"
FONT = ROOT / "assets/fonts/NotoSansCJKsc-Regular.otf"
PALETTE = {
    "bg": "#0A1119", "surface": "#101923", "raised": "#192D38",
    "line": "#364E59", "gold": "#C49A5A", "bright": "#F2DBA0",
    "teal": "#72D7C5", "quiet": "#A4B4CB", "text": "#E7E5D5",
    "red": "#ED8A78", "blue": "#629ED2", "purple": "#AD91D8",
}
CATALOG = json.loads((ROOT / "assets/cards.json").read_text(encoding="utf-8"))
CARDS = {card["id"]: card for card in CATALOG["cards"]}
ART = json.loads((ROOT / "assets/art/runtime.json").read_text(encoding="utf-8"))["cards"]
TYPE = {"action": "行动", "analytic": "解析法术", "word": "言灵", "formation": "阵法", "rune": "符文", "seal": "符文"}
RARITY = {"common": "普通", "uncommon": "罕见", "rare": "稀有", "epic": "史诗", "legendary": "传说"}


class Board:
    def __init__(self, width: int, height: int):
        self.w, self.h = width, height
        self.s = min(width / 1920, height / 1080)
        self.image = Image.new("RGB", (width, height), PALETTE["bg"])
        self.d = ImageDraw.Draw(self.image)
        self.fonts = {}
        self.clipped = []
        self.panel((12, 12, width - 12, height - 12), border="line", fill="bg")

    def size(self, n: int) -> int:
        return max(1, round(n * self.s))

    def font(self, n: int, small: bool = False):
        size = max(14 if small else 16, self.size(n))
        if size not in self.fonts:
            self.fonts[size] = ImageFont.truetype(str(FONT), size)
        return self.fonts[size]

    def text(self, x, y, text, n=22, color="text", max_width=None, small=False):
        f = self.font(n, small)
        text = str(text)
        if max_width and self.d.textlength(text, font=f) > max_width:
            while text and self.d.textlength(text + "…", font=f) > max_width:
                text = text[:-1]
            text += "…"
        self.d.text((round(x), round(y)), text, fill=PALETTE.get(color, color), font=f)

    def wrap(self, x, y, text, width, n=22, color="text", lines=99):
        font = self.font(n)
        line, rows = "", []
        for char in text:
            if char in "。，；：！？、）”" and line and self.d.textlength(line + char, font=font) > width:
                rows.append(line[:-1])
                line = line[-1] + char
            elif char == "\n" or self.d.textlength(line + char, font=font) > width:
                rows.append(line)
                line = "" if char == "\n" else char
            else:
                line += char
        rows.append(line)
        for index, row in enumerate(rows[:lines]):
            self.text(x, y + index * (font.size + 9), row, n, color)
        return y + min(lines, len(rows)) * (font.size + 9)

    def panel(self, rect, border="line", fill="surface", accent=False):
        x, y, r, b = map(round, rect)
        cut = self.size(7)
        points = [(x + cut, y), (r - cut, y), (r, y + cut), (r, b - cut), (r - cut, b),
                  (x + cut, b), (x, b - cut), (x, y + cut)]
        self.d.polygon(points, fill=PALETTE[fill], outline=PALETTE[border])
        for px, py, dx, dy in [(x, y, 1, 1), (r, y, -1, 1), (x, b, 1, -1), (r, b, -1, -1)]:
            c = PALETTE["gold" if accent else border]
            self.d.line([(px + dx * 10, py + dy * 4), (px + dx * 4, py + dy * 4), (px + dx * 4, py + dy * 10)], fill=c, width=2)

    def rune(self, cx, cy, radius, dim=True):
        line = PALETTE["line" if dim else "gold"]
        self.d.ellipse((cx-radius, cy-radius, cx+radius, cy+radius), outline=line, width=2)
        self.d.ellipse((cx-radius*.88, cy-radius*.88, cx+radius*.88, cy+radius*.88), outline=line, width=1)
        points = [(cx + math.sin(i*math.pi/3)*radius*.76, cy + math.cos(i*math.pi/3)*radius*.76) for i in range(6)]
        for indices in [(0, 2, 4, 0), (1, 3, 5, 1)]:
            self.d.line([points[i] for i in indices], fill=line, width=2)
        for px, py in points:
            self.d.rectangle((px-3, py-3, px+3, py+3), fill=PALETTE["teal"] if not dim else line)

    def art(self, card_id, rect):
        image = Image.open(ROOT / "assets/art" / ART[card_id]).convert("RGBA")
        x, y, r, b = map(round, rect)
        image.thumbnail((max(1, r-x), max(1, b-y)), Image.Resampling.NEAREST)
        self.image.paste(image, (x + (r-x-image.width)//2, y + (b-y-image.height)//2), image)

    def button(self, rect, label, active=False, secondary=False):
        self.panel(rect, "teal" if active else "gold" if not secondary else "line", "raised" if active else "surface", not secondary)
        x, y, r, b = rect
        f = self.font(24)
        self.text((x+r-self.d.textlength(label, font=f))/2, (y+b-f.size)/2-3, label, 24, "teal" if active else "text")

    def card(self, card_id, rect, status="", compact=False, selected=False):
        x, y, r, b = rect
        card = CARDS[card_id]
        pad = self.size(12)
        self.panel(rect, "teal" if selected else "gold", "raised" if selected else "surface", True)
        name_size = 20 if compact else 24
        self.text(x+pad, y+pad-3, card["name"], name_size, max_width=r-x-pad*2, small=True)
        footer = max(34, self.size(48))
        self.art(card_id, (x+pad, y+max(32, self.size(40)), r-pad, b-footer))
        if status:
            self.d.rectangle((round(x+2), round(b-footer), round(r-2), round(b-2)), fill=PALETTE["raised"])
            self.text(x+pad, b-footer+5, status, 19, "teal", max_width=r-x-pad*2, small=True)
        else:
            cost = f"解{card.get('cost', 0)} / 施{card.get('castCost', 0)}" if card["type"] == "analytic" else f"魔素 {card.get('cost', 0)}"
            self.text(x+pad, b-footer+3, cost, 19, "teal", max_width=r-x-pad*2, small=True)
            if footer > 40:
                self.text(x+pad, b-25, TYPE[card["type"]], 17, "quiet", small=True)

    def detail(self, card_id, rect):
        x, y, r, b = rect
        p = max(16, self.size(24))
        card = CARDS[card_id]
        self.panel(rect, "gold", accent=True)
        self.text(x+p, y+p, "卡牌详情", 20, "gold")
        yy = y+p+max(30, self.size(42))
        self.text(x+p, yy, card["name"], 32, "bright", max_width=r-x-2*p)
        yy += max(35, self.size(46))
        self.text(x+p, yy, TYPE[card["type"]]+" · "+RARITY[card["rarity"]], 20, "quiet")
        yy += max(28, self.size(36))
        art_h = min(self.size(200), (b-y)*.24)
        self.art(card_id, (x+p, yy, r-p, yy+art_h))
        yy += art_h+16
        if card["type"] == "analytic":
            fields = f"解析费用 {card.get('cost',0)}   施法费用 {card.get('castCost',0)}\n{card.get('rank',0)}阶 · {card.get('speed',1)}速 · 解析{card.get('analysisTurns',1)}回合"
        else:
            fields = f"魔素费用 {card.get('cost',0)} · {card.get('speed',1)}速"
        yy = self.wrap(x+p, yy, fields, r-x-2*p, 21, "teal")+15
        self.d.line((x+p, yy, r-p, yy), fill=PALETTE["line"], width=1)
        yy = self.wrap(x+p, yy+16, card["text"], r-x-2*p, 22)
        if yy+90 < b:
            self.wrap(x+p, yy+22, card.get("flavor", ""), r-x-2*p, 18, "quiet", lines=max(1, int((b-yy-70)/32)))
        self.text(x+p, b-38, "详情只读 · 操作显示在选中卡牌附近", 17, "quiet", max_width=r-x-2*p, small=True)


def title(board):
    b, w, h = board, board.w, board.h
    b.text(42, 32, "巫师牌 · 奥术对决", 24, "gold")
    b.text(w-255, 34, "本地 · 局域网 · 构筑", 19, "quiet")
    split = w*.60
    cx, cy = split*.52, h*.47
    b.rune(cx, cy, min(split*.40, h*.38))
    logo = Image.open(ROOT / "assets/art/branding/wizardcard-logo-v1.png").convert("RGBA")
    logo.thumbnail((round(split*.83), round(h*.29)), Image.Resampling.NEAREST)
    b.image.paste(logo, (round(cx-logo.width/2), round(cy-logo.height*.5)), logo)
    b.text(cx-split*.28, h*.68, "构筑法阵 · 编织言灵 · 掌控连锁", 28, "bright")
    b.text(cx-split*.28, h*.735, "每一次准备，都是一场奥术博弈。", 22, "quiet")
    x, r = split+20, w-65
    b.text(x, h*.24, "踏入奥术竞技场", 36, "bright")
    b.text(x, h*.30, "选择你的对决方式", 21, "quiet")
    labels = ["单人AI对战", "本地换手对战", "局域网对战", "卡组构筑", "游戏设置"]
    bh = max(42, b.size(64))
    for index, label in enumerate(labels):
        y = h*.37+index*(bh+16)
        b.button((x, y, r, y+bh), label, active=index==0)
    b.text(x, h*.88, "引导教学位于单人AI页面", 19, "quiet")
    b.text(42, h-54, "规则 1.0.0  ·  30种卡牌  ·  40生命", 19, "quiet")
    b.text(w-205, h-54, "退出游戏  →", 21, "gold")


def match(board):
    b, w, h = board, board.w, board.h
    gap = max(12, b.size(18))
    left = max(172, b.size(240))
    right = max(250, b.size(346))
    x, r = left+gap*2, w-right-gap*2
    b.text(32, 27, "巫师牌", 28, "gold")
    state=json.loads((OUT/"board-view.json").read_text(encoding="utf-8"))
    b.text(x, 28, f"第{state['players'][state['activePlayer']]['ownTurn']}回合  /  {state['phaseName']}阶段", 26, "bright")
    b.text(w-315, 31, "动画：完整    设置    记录", 20, "quiet")
    y = max(68, b.size(96))
    phases = ["抽卡", "准备", "主要", "施法", "结束"]
    cell = (r-x-4*8)/5
    for i, label in enumerate(phases):
        b.button((x+i*(cell+8), y, x+i*(cell+8)+cell, y+max(32,b.size(42))), f"{i+1} {label}", i==state['phase'], i!=state['phase'])
    top, bottom = y+max(50,b.size(68)), h-max(164,b.size(230))
    middle = (top+bottom)/2
    row_h = (bottom-top-max(95,b.size(130)))/2
    slot_w = (r-x-4*10)/5
    opponent=state['players'][1]
    b.text(x, top-27, f"对手场地 · 手牌 {opponent['handCount']} · 牌库 {opponent['deckCount']}", 19, "quiet")
    fields=state['cards']
    for row, owner in enumerate([1,0]):
        sy = top if row==0 else bottom-row_h
        formations=[c for c in fields if c['owner']==owner and c['zone']==3 and c.get('definition',{}).get('type')==3]
        slot=0
        for c in formations:
            body=max(c['definition']['body'],1)
            sx=x+slot*(slot_w+10)
            group_w=body*(slot_w+10)-10
            linked=[a for a in fields if a['host']==c['id']]
            b.card(c['definition']['id'],(sx,sy,sx+group_w,sy+row_h),f"阵体{c['definition']['body']} · 环位{c['occupiedRings']}/{c['effectiveRings']}",compact=True)
            if linked:
                label=" / ".join(a.get('definition',{}).get('name','埋伏卡') for a in linked)
                b.panel((sx+6,sy+row_h-72,sx+group_w-6,sy+row_h-38),"teal","raised")
                b.text(sx+12,sy+row_h-68,label,18,"teal",max_width=group_w-24,small=True)
            slot+=body
        for i in range(slot,5):
            sx=x+i*(slot_w+10)
            b.panel((sx,sy,sx+slot_w,sy+row_h))
            b.rune(sx+slot_w/2,sy+row_h*.40,min(slot_w*.28,row_h*.22))
            b.text(sx+12,sy+row_h-38,"未设置",18,"quiet",small=True)
    cy = middle-max(42,b.size(57))
    ch = max(80,b.size(110))
    b.panel((x,cy,r,cy+ch),"teal", "raised")
    chain=state['chain']
    b.text(x+18,cy+10,f"公开连锁 · {chain['windowName']} · {'本方' if chain['priority']==0 else '对手'}响应",22,"teal")
    names={c['id']:c.get('definition',{}).get('name','埋伏卡') for c in fields}
    label=" → ".join(f"{i+1} {names.get(link['source'],link['kindName'])} · {link['speed']}速" for i,link in enumerate(chain['links']))
    b.text(x+18,cy+max(38,b.size(52)),label,21,"text", max_width=(r-x)*.67)
    b.button((r-max(166,b.size(232))-16,cy+18,r-16,cy+ch-18),"放弃响应" if chain['priority']==0 else "等待对手响应",active=chain['priority']==0)
    # Public resource bars and readable private log.
    for index, label in enumerate(["对手", "本方"]):
        hy = top if index==0 else middle+gap
        hh = max(142,b.size(202))
        b.panel((gap,hy,left+gap,hy+hh),"gold",accent=True)
        b.text(gap+16,hy+12,label,22,"gold")
        stats=state['players'][1-index]
        b.text(gap+16,hy+43,f"{stats['life']} / 40",38,"bright")
        b.text(gap+16,hy+max(83,b.size(106)),f"魔素  {stats['mana']} / 12",22,"teal")
        b.text(gap+16,hy+max(113,b.size(143)),f"荷载  {stats['load']} / {stats['capacity']}",22,"text")
    b.detail("fireball", (r+gap,top,w-gap,bottom))
    hand_y = bottom+gap
    b.panel((x,hand_y,r,h-gap),"line")
    b.text(x+16,hand_y+9,"本方手牌  ·  点选查看 / 拖放使用",19,"quiet")
    cards=[c['definition']['id'] for c in fields if c['owner']==0 and c['zone']==1][:6]
    cw = min(max(124,b.size(188)),(r-x-(len(cards)+1)*12)/max(len(cards),1))
    for i, card_id in enumerate(cards):
        sx=x+12+i*(cw+12)
        b.card(card_id,(sx,hand_y+42,sx+cw,h-gap-12),compact=True,selected=i==0)
    b.text(gap+8,h-120,"只显示公开及本方日志",17,"quiet",max_width=left-14,small=True)
    events=state['events'][-2:]
    for i,event in enumerate(events):
        b.text(gap+8,h-86+i*34,event['text'],18,"text" if i==0 else "quiet",max_width=left-14,small=True)
    b.button((r+gap,bottom+gap,w-gap,bottom+gap+max(44,b.size(62))),"当前响应窗口",secondary=True)
    b.text(r+gap+16,bottom+gap+max(60,b.size(90)),"准备取消不退款",20,"quiet")


def editor(board):
    b, w, h = board, board.w, board.h
    gap=max(14,b.size(24))
    detail_w=max(266,b.size(380))
    pool_w=(w-detail_w-4*gap)*.55
    middle_x=detail_w+2*gap
    pool_x=w-pool_w-gap
    top=max(106,b.size(138))
    bottom=h-max(92,b.size(116))
    b.text(gap,24,"← 卡组列表",22,"gold")
    b.text(middle_x,24,"我的奥术牌组",32,"bright")
    b.text(middle_x,73,"卡组名称  /  编辑中",19,"quiet")
    b.text(w-300,35,"主卡组 30 / 30 · 合法",23,"teal")
    b.detail("fireball",(gap,top,detail_w+gap,bottom))
    b.panel((middle_x,top,pool_x-gap,bottom),"line")
    b.text(middle_x+16,top+14,"主卡组",26,"bright")
    b.text(middle_x+16,top+54,"基础：均衡之六芒星 · 更换",20,"teal",max_width=pool_x-middle_x-gap-32)
    default=json.loads((ROOT / "assets/deck.json").read_text(encoding="utf-8"))
    rows=[(row["id"], row["count"]) for row in default]
    row_h=min(max(41,b.size(55)),(bottom-top-148)/max(len(rows),1))
    yy=top+104
    for i,(card_id,count) in enumerate(rows):
        if yy+row_h > bottom-32: break
        if i%2==0: b.d.rectangle((middle_x+10,yy,pool_x-gap-10,yy+row_h-3),fill=PALETTE["raised"])
        b.text(middle_x+22,yy+4,CARDS[card_id]["name"],21,"text",max_width=pool_x-middle_x-gap-100)
        b.text(pool_x-gap-63,yy+4,f"×{count}",22,"gold")
        yy+=row_h
    b.text(middle_x+16,bottom-34,"右键移除 · 拖至卡池移除",18,"quiet",small=True)
    b.panel((pool_x,top,w-gap,bottom),"line")
    b.text(pool_x+18,top+14,"卡池",26,"bright")
    b.text(pool_x+18,top+58,"搜索卡名 / 效果",20,"quiet")
    b.text(pool_x+18,top+96,"类型：全部   稀有度：全部   重置",19,"teal",max_width=pool_w-36)
    cols=3
    card_w=(pool_w-4*16)/cols
    start_y=top+144
    card_h=(bottom-start_y-74)/2-12
    examples=["fireball","spark","recall","mend","reservoir","ring"]
    for i,card_id in enumerate(examples):
        xx=pool_x+16+(i%cols)*(card_w+16)
        yy=start_y+(i//cols)*(card_h+16)
        b.card(card_id,(xx,yy,xx+card_w,yy+card_h),compact=True,selected=i==0)
    b.text(pool_x+18,bottom-46,"←  1 / 5  →     右键加入 · 拖至卡组加入",18,"quiet",max_width=pool_w-36,small=True)
    b.text(gap,bottom+24,"30张 · 同名最多3张 · 基础阵法不计入主卡组",20,"teal",max_width=w-400)
    bw=max(128,b.size(184))
    b.button((w-2*bw-2*gap,bottom+18,w-bw-2*gap,h-22),"返回",secondary=True)
    b.button((w-bw-gap,bottom+18,w-gap,h-22),"保存草稿",active=True)


def states(board):
    b,w,h=board,board.w,board.h
    b.text(30,22,"操作与生命周期 · 状态设计",30,"bright")
    entries=[
        ("01 选牌与操作", "选中火球术，详情保持只读。\n合法操作在卡牌附近显示。", "准备施法"),
        ("02 选择目标 / 成本", "仅高亮当前合法候选。\n多宿主和多链目标提供列表。", "确认选择"),
        ("03 最终确认", "准备火球术：施法费用1。\n最后确认才支付，过期选择拒绝。", "确认提交"),
        ("04 换手遮挡", "私有手牌、详情和日志已遮挡。\n由当前操作者确认接手。", "确认接手"),
        ("05 设置与草稿", "本地设置暂停，保留原决策。\n离开未保存草稿需确认。", "应用并保存"),
        ("06 教学与终局", "跟随真实规则逐步操作。\n终局后可保存、重开或返回。", "继续 / 重开"),
        ("07 房间与准备", "创建或输入LAN IP加入。\n双方卡组合法并准备后开局。", "准备"),
        ("08 断线与重连", "规则冻结，保留当前连锁。\n60秒内恢复裁剪快照。", "重新连接"),
        ("09 保存失败", "保留已接受的对局和编辑内容。\n重试只保存，不重复提交动作。", "重试保存"),
    ]
    gap=18
    tile_w=(w-gap*4)/3
    tile_h=(h-110-gap*3)/3
    for i,(name,description,action) in enumerate(entries):
        x=gap+(i%3)*(tile_w+gap)
        y=80+(i//3)*(tile_h+gap)
        b.panel((x,y,x+tile_w,y+tile_h),"line")
        b.text(x+18,y+15,name,24,"gold",max_width=tile_w-36)
        b.wrap(x+18,y+55,description,tile_w-36,21)
        b.button((x+18,y+tile_h-65,x+tile_w-18,y+tile_h-18),action,active=i in [2,3])


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    pages=[("title","标题",title),("match","复杂对局场地",match),("editor","卡组编辑器",editor),("states","状态流程",states)]
    captured=json.loads((OUT/"board-view.json").read_text(encoding="utf-8"))
    manifest={"rulesVersion":CATALOG["rulesVersion"],"cardSetVersion":CATALOG["cardSetVersion"],
              "cardCount":len(CARDS),"sourceReplay":captured['sourceReplay'],"sourceStep":captured['capturedStep'],
              "sourceDigest":captured['sourceDigest'],"outputs":[],"palette":PALETTE}
    for width,height in [(1920,1080),(1280,720),(1600,1000)]:
        for key,label,render in pages:
            board=Board(width,height)
            render(board)
            name=f"{key}-{width}x{height}.png"
            board.image.save(OUT/name)
            manifest["outputs"].append({"page":key,"label":label,"width":width,"height":height,"file":name})
    (OUT/"manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    gallery='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>WizardCard v1.5 · 设计评审</title>
<style>*{box-sizing:border-box}body{margin:0;background:#0a1119;color:#e7e5d5;font:16px/1.6 system-ui,"Microsoft YaHei",sans-serif}header{padding:22px 32px;border-bottom:1px solid #364e59}h1{font-size:24px;margin:0;color:#f2dba0}p{margin:8px 0;color:#a4b4cb}nav{display:flex;gap:10px;flex-wrap:wrap;margin:18px 0}button,select{font:inherit;padding:9px 16px;background:#192d38;color:#e7e5d5;border:1px solid #6d5035;cursor:pointer}button[aria-pressed=true]{border-color:#72d7c5;color:#72d7c5}main{padding:24px 32px}figure{margin:0}img{display:block;width:100%;height:auto;border:1px solid #364e59}figcaption{color:#a4b4cb;margin-top:10px}.zoom img{width:auto;max-width:none}.zoom figure{overflow:auto}a{color:#72d7c5}details{margin:22px 0;max-width:1000px}kbd{border:1px solid #364e59;padding:2px 7px}footer{padding:16px 32px;color:#a4b4cb}</style>
<header><h1>WizardCard v1.5 · 精致像素奇幻</h1><p>新视觉方案 / 沿用Logo与30张原始插画 / 深色石板、古铜边饰、青绿符文</p>
<nav aria-label="设计页面"><button data-page="title" aria-pressed="true">标题</button><button data-page="match" aria-pressed="false">复杂对局场地</button><button data-page="editor" aria-pressed="false">卡组编辑器</button><button data-page="states" aria-pressed="false">状态流程</button><select id="size" aria-label="设计尺寸"><option>1920x1080</option><option>1280x720</option><option>1600x1000</option></select><button id="zoom" aria-pressed="false">原始尺寸</button></nav></header>
<main><figure><img id="board" src="title-1920x1080.png" alt="WizardCard标题设计稿"><figcaption id="caption"></figcaption></figure>
<details open><summary>评审重点与状态流程</summary><p>标题：AI、换手、局域网、构筑、设置五入口；教学位于AI子页。对局：资源与阶段优先，阵法按阵体占槽并展示宿主关联，中央公开连锁保持可读；卡牌详情只读。构筑：详情 / 主卡组 / 筛选卡池，保留基础阵法、右键、拖放与草稿。</p><p>操作：点选 → 合法操作 → 目标 / 额外成本 → 最终确认。<kbd>Esc</kbd>或右键取消拖动，确认前不支付。换手：遮挡 → 确认接手 → 本方视图。设置：暂停 → 应用或放弃 → 恢复原决策。联网：创建 / 加入 → 选牌 → 准备 → 对战 → 断线冻结 → 重连快照或结束。</p><p>复杂场面数据取自实际复盘的查看者0投影，版式为设计示意；当前规则1.0.0、生命上限40。设计稿不代表页面已实现。待设计确认后迁移正式场景。</p></details>
<a href="../../../../docs/design/v1.5-ui.md">完整组件与状态规范</a></main><footer>现有源插画保持原始字节。所有文字由当前cards.json读取；私有对手手牌不展示。</footer>
<script>let page='title';const labels={title:'标题',match:'复杂对局场地',editor:'卡组编辑器',states:'状态流程'};const buttons=[...document.querySelectorAll('[data-page]')];const size=document.getElementById('size');const image=document.getElementById('board');function update(){image.src=page+'-'+size.value+'.png';image.alt=labels[page]+'设计稿';document.getElementById('caption').textContent=labels[page]+' · '+size.value+' · 设计评审稿';buttons.forEach(b=>b.setAttribute('aria-pressed',String(b.dataset.page===page)))}buttons.forEach(b=>b.addEventListener('click',()=>{page=b.dataset.page;update()}));size.addEventListener('change',update);document.getElementById('zoom').addEventListener('click',function(){const on=this.getAttribute('aria-pressed')!=='true';this.setAttribute('aria-pressed',String(on));document.querySelector('main').classList.toggle('zoom',on);this.textContent=on?'适应窗口':'原始尺寸'});update();</script></html>'''
    (OUT/"index.html").write_text(gallery,encoding="utf-8")
    print(f"Rendered {len(manifest['outputs'])} review boards using {len(CARDS)} current definitions: {OUT}")


if __name__ == "__main__":
    main()
