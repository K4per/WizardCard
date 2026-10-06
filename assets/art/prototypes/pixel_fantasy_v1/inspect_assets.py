"""Read generated PNGs and write metadata/preview; never modify image pixels."""
from collections import Counter
from hashlib import sha256
from html import escape
import json
from pathlib import Path
import re

from PIL import Image

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT / 'source/WizardCard_PixelFantasy_64x64_20261004'
PALETTE = {c.upper() for c in re.findall(r'^- (#[0-9A-Fa-f]{6})$', (PROJECT / 'spec_lock.md').read_text(encoding='utf-8-sig'), re.M)}
SPECS = [
    ('match_ui_concept', '对局 UI 风格稿', 'ui', (1600, 1000), False, ['ART-01'], 'exec-1065c3ab-bed3-450e-8efc-585615ee9bf7.png', '保留双人区域、五列公共区与横向卡条。内部卡牌与日志文字已移除；布局坐标、语义标记和中文需在真实客户端重新核对。'),
    ('fantasy_ui_kit', '奇幻 UI 组件探索板', 'ui', (1536, 1024), True, ['ART-02', 'ART-06', 'ART-09'], 'exec-7047ca1a-25a9-4001-a4cd-323432b40e70.png', '面板、详情框、四种按钮状态、两种卡条、四项资源符号与额度标记。整板未切分，仍有半透明背景，不能直接作为透明图集使用。'),
    ('balance', '均衡', 'items', (1024, 768), False, ['ART-08'], 'exec-09a6122f-895b-4840-ba81-4843eae06fd5.png', '对称多环阵图、均匀锚点与稳定承载核心。无基础身份标记，后续由UI动态叠加。'),
    ('fireball', '火球术', 'items', (1024, 768), False, ['ART-08'], 'exec-e59adab1-2905-48a3-9031-a55c3f51304a.png', '大型火焰核心与明确斜向尾焰，区别于后续小型火花术。'),
    ('barbs', '银光锐语', 'items', (1024, 768), False, ['ART-08'], 'exec-cdbeebf4-5ded-4f3d-a344-b0ed514261c6.png', '银色锐利咒文干扰准备连接，目标法术球仍完整。取消准备语义仍需真人辨识确认。'),
    ('ring', '奥术圆环', 'items', (1024, 768), False, ['ART-08'], 'exec-d1d2cc5f-aab6-439e-85c5-bea4983a2a67.png', '单枚竖立开口圆环与顺向循环能量；未画固定宿主，保持阵法和法术附着通用。'),
]

assets = []
for asset_id, label, category, target, wants_alpha, requirement_ids, original, note in SPECS:
    file = PROJECT / 'assets' / category / f'{asset_id}.png'
    with Image.open(file) as source:
        source.load()
        rgba = source.convert('RGBA')
        histogram = Counter(rgba.getdata())
        visible = {rgb[:3] for rgb, count in histogram.items() if rgb[3] > 0}
        exact_count = sum(count for (r, g, b, a), count in histogram.items() if a > 0 and f'#{r:02X}{g:02X}{b:02X}' in PALETTE)
        total_visible = sum(count for rgba_value, count in histogram.items() if rgba_value[3] > 0)
        transparent = sum(count for rgba_value, count in histogram.items() if rgba_value[3] == 0)
        partial = sum(count for rgba_value, count in histogram.items() if 0 < rgba_value[3] < 255)
        opaque = sum(count for rgba_value, count in histogram.items() if rgba_value[3] == 255)
        outside = sum(1 for r, g, b in visible if f'#{r:02X}{g:02X}{b:02X}' not in PALETTE)
        issues = []
        if source.size != target:
            issues.append('dimensions_differ_from_requested')
        if len(visible) > 24:
            issues.append('exceeds_24_color_target')
        if outside:
            issues.append('colors_outside_target_palette')
        if partial:
            issues.append('partial_alpha')
        if wants_alpha:
            issues.extend(['background_cleanup_required', 'atlas_slicing_required'])
        assets.append({
            'id': asset_id, 'name_zh': label, 'requirement_ids': requirement_ids,
            'file': file.relative_to(ROOT).as_posix(), 'source_file': file.relative_to(ROOT).as_posix(),
            'source_kind': 'original generated PNG; no layered source',
            'original_generated_filename': original, 'requested_dimensions': list(target),
            'actual_dimensions': list(source.size), 'mode': source.mode, 'format': source.format,
            'alpha_requested': wants_alpha, 'alpha_extrema': list(rgba.getchannel('A').getextrema()),
            'transparent_pixels': transparent, 'semi_transparent_pixels': partial, 'opaque_pixels': opaque,
            'unique_visible_rgb_colors': len(visible), 'out_of_palette_visible_rgb_colors': outside,
            'exact_palette_pixel_percentage': round(100 * exact_count / total_visible, 5),
            'sha256': sha256(file.read_bytes()).hexdigest(), 'byte_size': file.stat().st_size,
            'author_source': 'OpenAI built-in imagegen, directed by Codex for WizardCard',
            'license_note': 'AI-generated project artwork; no external stock assets; no additional third-party license or exclusivity claim.',
            'version': 'pixel_fantasy_v1', 'status': 'prototype_unintegrated',
            'production_ready': False, 'pending': issues, 'visual_review': note,
        })

manifest = {'version': 'pixel_fantasy_v1', 'date': '2026-10-04', 'generator': 'OpenAI built-in imagegen', 'runtime_integration': False,
            'production_validation_passed': False, 'palette_target': sorted(PALETTE), 'assets': assets,
            'omitted_scope': 'Remaining 9 card illustrations; standalone 21 semantic icons and 7 attribute icons; handoff, confirm and result UI states; production slicing and integration.'}
(ROOT / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

rows = []
for a in assets:
    w, h = a['actual_dimensions']
    tw, th = a['requested_dimensions']
    rows.append(f"| {a['id']} | {w}×{h} | {tw}×{th} | {a['unique_visible_rgb_colors']:,} | {a['semi_transparent_pixels']:,} | 试制，未通过正式规格 |")
report = '''# 素材检查记录

检查日期：2026-10-04。所有 PNG 均可解码；已逐张查看构图。**正式像素素材验收未通过**，因此未执行量产导出、限色覆盖、图集打包或客户端接入。

| 资产 | 实际尺寸 | 请求尺寸 | 可见 RGB 颜色数 | 半透明像素 | 结论 |
| --- | --- | --- | ---: | ---: | --- |
''' + '\n'.join(rows) + '''

## 检查说明

- 运行了技能自带 asset_validator.py，退出码为1：六张图均超出24色预算及目标色板；UI组件图有半透明像素。
- 本报告另统计所有 alpha > 0 的可见颜色；技能校验器只统计 alpha >= 128，所以透明图的颜色数可能略有差异。
- 实际尺寸以 PNG 为准，不用提示词里的请求尺寸代替。原始像素未被重采样或自动量化。
- 24色色板是严格像素制作目标；目前图像的像素风外观不代表已达到固定像素网格、限色或二值alpha要求。
- UI组件做过一次背景修订，但清理仍未达标。整板只能作为重绘和切图参考，不能直接叠加到客户端背景。
- 对局稿保留空间分区，但不是按客户端坐标渲染的真实截图。手牌精确尺寸、状态图标、目标提示、公共信息与私有信息遮挡仍需正式UI实现验证。
- 插画无卡名、费用或规则文字；没有声称分层源文件、可伸缩边距、图集坐标、动画或程序接入已经存在。

## 逐项视觉检查

''' + '\n\n'.join(f"- **{a['name_zh']}**：{a['visual_review']}" for a in assets) + '\n'
(ROOT / 'validation.md').write_text(report, encoding='utf-8')

readme = '''# 巫师牌：像素奇幻美术试制 v1

根据 docs 美术清单制作的首批方向样稿：**2张UI探索稿 + 4张无文字卡牌插画**。视觉采用深靛石板、古铜框饰、青绿符文和硬边像素块。

**当前为试制，未接入游戏，未通过严格限色与透明度验收。** 不是全部美术需求的完成交付。UI组件尚需清理背景及切图；其余9张插画和其他P0资源仍待制作。

- [浏览图片预览](index.html)
- [资产清单、尺寸、来源与哈希](manifest.json)
- [生成提示词（含修订）](prompts.json)
- [技术与视觉检查记录](validation.md)
- [本批范围](requirements.md)
- [设计规格](source/WizardCard_PixelFantasy_64x64_20261004/design_spec.md)
- [执行规格与色板](source/WizardCard_PixelFantasy_64x64_20261004/spec_lock.md)

所有图片由内置 OpenAI imagegen 生成，保留原始 PNG；没有另行引入外部素材。初稿放在 source 工程的 images 目录，仅供追踪；早期UI初稿含错误生成文字，已弃用。

'''
for a in assets:
    readme += f"## {a['name_zh']}\n\n{a['visual_review']}\n\n![{a['name_zh']}]({a['file']})\n\n"
(ROOT / 'README.md').write_text(readme, encoding='utf-8')

figures = ''.join(f'''<figure class="{'wide' if a['id'] in ('match_ui_concept','fantasy_ui_kit') else ''}"><a href="{a['file']}" target="_blank"><img src="{a['file']}" alt="{escape(a['name_zh'])}" loading="lazy"></a><figcaption><h2>{escape(a['name_zh'])}</h2><span>{a['actual_dimensions'][0]} × {a['actual_dimensions'][1]} · PNG</span><p>{escape(a['visual_review'])}</p></figcaption></figure>''' for a in assets)
html = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>巫师牌 · 像素奇幻试制</title><style>
*{box-sizing:border-box}body{margin:0;background:#101320;color:#dce6eb;font:16px/1.75 system-ui,"Microsoft YaHei",sans-serif}main{max-width:1360px;margin:auto;padding:48px 28px 80px}header{border-bottom:1px solid #6d5035;padding-bottom:24px;margin-bottom:28px}small{letter-spacing:.2em;color:#72d7c5}h1{font-size:34px;color:#e0be7c;margin:8px 0}p{color:#a4b4cb}nav{display:flex;gap:24px;flex-wrap:wrap}a{color:#72d7c5}.notice{padding:18px 24px;background:#242b3d;border-left:3px solid #c49a5a;margin:24px 0}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:26px}figure{margin:0;border:1px solid #44352b;background:#191d2b;overflow:hidden}.wide{grid-column:1/-1}figure img{display:block;width:100%;height:auto;background:repeating-conic-gradient(#242b3d 0% 25%,#354057 0% 50%) 50%/24px 24px}body.pixel img{image-rendering:pixelated}figcaption{padding:18px 24px}h2{margin:0;color:#e0be7c;font-size:22px}figcaption span{font-size:13px;color:#72d7c5}figcaption p{margin:8px 0 0}button{background:#242b3d;color:#dce6eb;border:1px solid #9b7448;padding:8px 14px;cursor:pointer;font:inherit}@media(max-width:700px){main{padding:24px 14px}.grid{grid-template-columns:1fr}h1{font-size:27px}}
</style><main><header><small>WIZARDCARD / ART STUDY 01</small><h1>巫师牌 · 像素奇幻</h1><p>深靛石板 · 古铜框饰 · 青绿符文<br>首批 UI 风格探索与四张无文字卡牌插画 · 2026.10.04</p><nav><a href="README.md">交付说明</a><a href="manifest.json">资产清单</a><a href="validation.md">检查记录</a><a href="prompts.json">生成提示词</a><button onclick="document.body.classList.toggle('pixel')">切换最近邻预览</button></nav></header><div class="notice">试制稿 · 未接入游戏。严格限色、固定像素栅格和 UI 透明边缘尚待处理；图片不是正式图集。点击图片可查看原始尺寸。</div><section class="grid">''' + figures + '</section></main></html>'
(ROOT / 'index.html').write_text(html, encoding='utf-8')
print(json.dumps([{'id': a['id'], 'dimensions': a['actual_dimensions'], 'alpha': a['alpha_extrema'], 'colors': a['unique_visible_rgb_colors'], 'partial_alpha': a['semi_transparent_pixels']} for a in assets], ensure_ascii=False, indent=2))
