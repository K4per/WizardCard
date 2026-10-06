"""Read PNGs and write a manifest/gallery. Does not edit image pixels."""
from pathlib import Path
from hashlib import sha256
import json
from PIL import Image

ROOT = Path(__file__).resolve().parent
SPECS = [
    ('barbs', '银光锐语', '简化为三道银色锐笔组成的咒文符号。', 'exec-bd6cee44-eb1b-49de-8b35-5d1d8e33a9c1.png'),
    ('balance', '均衡', '两枚交织三角组成六芒星，古铜框饰与六点青绿宝石，中心镂空。', 'exec-5bc0039e-9494-4a78-b122-b2c1b7a022b3.png'),
    ('ring', '奥术圆环', '正面圆环，四向重复的宝石与成对符文，图案上下左右呼应，中心镂空。', 'exec-b3ee2490-10cd-4033-8f41-7c875b236802.png'),
    ('fireball', '火球术', '保留大型火焰核心和斜向尾焰，去除大厅、地面与反光。', 'exec-507405f7-93f1-4060-b67e-03b3b7a1243a.png'),
]
assets = []
for asset_id, title, note, original in SPECS:
    file = ROOT / 'cards' / f'{asset_id}.png'
    with Image.open(file) as im:
        im.load()
        assert im.mode == 'RGBA'
        a = im.getchannel('A')
        hist = a.histogram()
        corners = [a.getpixel(p) for p in [(0, 0), (im.width-1, 0), (0, im.height-1), (im.width-1, im.height-1)]]
        borders = [(0, 0, im.width, 1), (0, im.height-1, im.width, im.height), (0, 0, 1, im.height), (im.width-1, 0, im.width, im.height)]
        perimeter = max(a.crop(box).getextrema()[1] for box in borders)
        assert corners == [0, 0, 0, 0]
        assert hist[0] > im.width * im.height * .5
        if asset_id in ('balance', 'ring'):
            assert a.getpixel((im.width//2, im.height//2)) == 0
        assets.append({
            'id': asset_id, 'name_zh': title, 'file': f'cards/{asset_id}.png',
            'dimensions': list(im.size), 'format': im.format, 'mode': im.mode,
            'alpha_extrema': list(a.getextrema()), 'transparent_pixel_percentage': round(100*hist[0]/(im.width*im.height), 2),
            'semi_transparent_pixels': sum(hist[1:255]), 'corner_alpha': corners,
            'perimeter_max_alpha': perimeter, 'center_alpha': a.getpixel((im.width//2, im.height//2)),
            'source': 'OpenAI built-in imagegen', 'original_generated_filename': original,
            'source_kind': 'generated PNG; no layered source', 'version': 'pixel_fantasy_v2',
            'sha256': sha256(file.read_bytes()).hexdigest(), 'description': note,
            'status': 'revised_prototype_unintegrated',
            'license_note': 'AI-generated project artwork; no external stock assets or additional third-party license claim.',
        })
manifest = {'version': 'pixel_fantasy_v2', 'date': '2026-10-04', 'requirement': 'All four card illustrations without background; barbs simplified, balance hexagram, ring symmetrical pattern.',
            'previous_version': '../pixel_fantasy_v1/', 'runtime_integration': False, 'assets': assets}
(ROOT / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')

readme = '''# 无背景像素插画 v2

根据本轮反馈重做银光锐语、均衡、奥术圆环，并为火球术去掉环境。四张均为 **1448×1086 RGBA PNG**，背景透明，原版保留在相邻的 pixel_fantasy_v1 目录。

- [查看新版预览](index.html)：可切换透明网格、深色和浅色预览底色；底色不在图片里。
- [PNG规格、来源与哈希](manifest.json)
- [本轮生成与编辑提示词](prompts.json)
- [制作约束](spec_lock.md)
- [检查记录](validation.md)

由内置 OpenAI imagegen 生成。素材尚未接入客户端。

'''
for a in assets:
    readme += f"## {a['name_zh']}\n\n{a['description']}\n\n![{a['name_zh']}]({a['file']})\n\n"
(ROOT / 'README.md').write_text(readme, encoding='utf-8')

rows = '\n'.join(f"| {a['name_zh']} | 1448×1086 | {a['transparent_pixel_percentage']}% | {a['corner_alpha']} | {a['perimeter_max_alpha']} |" for a in assets)
validation = '''# v2检查记录

四张PNG均已解码、查看生成结果，并确认没有环境背景。四角alpha均为0；均衡与奥术圆环的中心alpha均为0。每张保留真实透明通道，没有把黑底或棋盘格画入背景。

| 图片 | 尺寸 | 完全透明像素比例 | 四角alpha | 最外缘最大alpha |
| --- | --- | ---: | --- | ---: |
''' + rows + '''

- 均衡：两枚三角交织形成六芒星，六点重复装饰，中心与三角孔洞镂空。
- 银光锐语：三道主要银色锐笔，无场景、法术球或复杂粒子群。
- 奥术圆环：正面圆形，四向重复锚点与成对符文；图案采用对称设计，不声称每个纹理像素都数学镜像一致。
- 火球术：仅保留主体、尾焰及邻近火星，地面和建筑已移除。
- 仍有抗锯齿/半透明边缘；圆环与火球术最外缘有alpha=1的近透明像素。未进行严格二值alpha、固定像素栅格或限色处理，不标记为正式像素制作验收通过。
- 本次PNG像素没有经过额外程序修改。原稿未覆盖；未修改客户端与卡牌规则。
'''
(ROOT / 'validation.md').write_text(validation, encoding='utf-8')

figures = ''.join(f'<figure><a href="{a["file"]}" target="_blank"><img src="{a["file"]}" alt="{a["name_zh"]}"></a><figcaption><h2>{a["name_zh"]}</h2><p>{a["description"]}</p><a href="{a["file"]}" download>下载 PNG</a></figcaption></figure>' for a in assets)
html = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>巫师牌 · 无背景插画 v2</title><style>
*{box-sizing:border-box}body{margin:0;background:#101320;color:#dce6eb;font:16px/1.7 system-ui,"Microsoft YaHei",sans-serif}main{max-width:1280px;margin:auto;padding:40px 24px}h1,h2{color:#e0be7c}h1{margin:6px 0}small{color:#72d7c5;letter-spacing:.15em}p{color:#a4b4cb}nav{display:flex;gap:12px;flex-wrap:wrap;margin:24px 0}button{font:inherit;padding:8px 16px;background:#242b3d;border:1px solid #9b7448;color:#dce6eb;cursor:pointer}a{color:#72d7c5}.grid{display:grid;grid-template-columns:1fr 1fr;gap:24px}figure{margin:0;background:#191d2b;border:1px solid #44352b}img{display:block;width:100%;height:auto;background:repeating-conic-gradient(#d0d3d6 0% 25%,#edf0f2 0% 50%) 50%/24px 24px}body.dark img{background:#101320}body.light img{background:#f1e9d9}figcaption{padding:16px 22px}h2{margin:0}figcaption p{margin:8px 0}@media(max-width:700px){.grid{grid-template-columns:1fr}main{padding:22px 12px}}
</style><main><small>WIZARDCARD / CARD ART V2</small><h1>四张插画 · 透明背景</h1><p>银光锐语简化 · 均衡六芒星 · 奥术圆环对称纹样 · 火球术去背景<br>1448 × 1086 · RGBA PNG · 原稿另存保留</p><nav><button onclick="document.body.className=''">透明网格</button><button onclick="document.body.className='dark'">深色底</button><button onclick="document.body.className='light'">浅色底</button><a href="README.md">素材说明</a><a href="prompts.json">提示词</a></nav><section class="grid">''' + figures + '</section></main></html>'
(ROOT/'index.html').write_text(html, encoding='utf-8')
print(json.dumps([{'id':a['id'], 'size':a['dimensions'], 'transparent_percent':a['transparent_pixel_percentage']} for a in assets], indent=2))
