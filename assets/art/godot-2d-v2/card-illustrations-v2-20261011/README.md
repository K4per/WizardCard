# WizardCard 2.0 卡牌插图与图鉴接入

2026-10-11。依据用户提供的《新法术设计.md》《新阵法设计.md》，导入 20 张法术、12 张阵法，共 32 张。按用户确认，本次范围为插图、卡牌数值/文字与图鉴展示。

采用内置 ImageGen，共交付 **16 张新绘透明 PNG**，另外 **16 张复用现有插图**。原始 PNG 与生成的透明通道保持不变；运行副本在 `godot/art/illustrations-v2/`，复用插图直接引用原有资源，不重复复制。新卡使用原来的四类完整卡框。

## 在游戏中查看

主菜单 → 卡牌图鉴 → **2.0 新卡（32）**。可以按卡牌类型筛选，搜索名称、学派、召唤条件或效果，点选查看完整卡牌与全部信息。另一页“现有卡池（30）”展示原生卡池。

费用保持原稿中的单值或斜杠写法；法术等级、学派、解析时间、速度、效果，阵法等级、阵体、最大解析数、荷载上限、魔素收入、召唤条件完整展示。稀有度与阵法速度未提供，保持空白。

## 文件

- [全部生成与修正提示词](prompts.json)：使用内置 ImageGen；记录参考图、原提示词及最终提示词。
- [PNG 尺寸、透明度与哈希清单](manifest.json)：仅包含 16 张新插图。
- 原设计文件：`assets/card-designs/v2/新法术设计.md`、`新阵法设计.md`。
- 图鉴数据：`godot/data/card-previews-v2.json`，由 `tools/import_card_previews_v2.py` 从原稿生成。
- 原图同步：`python tools/prepare_godot_frontend_assets.py`；校验：`python tools/prepare_godot_frontend_assets.py --check`。

原稿中的“解析法数”“解析回合”“阵法大小”“太阳的施法等级”仅进行显示字段归一；去除效果中的 Markdown 加粗标记以及法术反制末尾误加的顿号。原始字段保存在图鉴 JSON 的 `sourceFields`，原文件保留。

复用同名旧卡插图；另外“缝合生命”对应旧版“生命缝合”，“六芒星”对应“均衡之六芒星”，“五芒星”对应“生命之五芒星”。新版效果文字和数值来自本次设计文件。

## 逐卡资源

| 卡牌 | 类型 | 插图处理 | 插图 ID |
|---|---|---|---|
| 火球术 | 解析法术 | 复用旧图 | `fireball` |
| 寒霜射线 | 解析法术 | 复用旧图 | `ray-of-frost` |
| 闪电束 | 解析法术 | 复用旧图 | `lightning-bolt` |
| 魔法飞弹 | 解析法术 | 复用旧图 | `magic-missile` |
| 繁彩球 | 解析法术 | 新绘透明 PNG | `v2-chromatic-orb` |
| 曳光弹 | 解析法术 | 新绘透明 PNG | `v2-guiding-bolt` |
| 缝合生命 | 解析法术 | 复用旧图 | `mend` |
| 虚假生命 | 解析法术 | 复用旧图 | `false-life` |
| 炽热射线 | 解析法术 | 新绘透明 PNG | `v2-scorching-ray` |
| 炽燃结界 | 解析法术 | 复用旧图 | `ward` |
| 阳炎爆 | 解析法术 | 新绘透明 PNG | `v2-sunburst` |
| 治疗之环 | 解析法术 | 新绘透明 PNG | `v2-healing-circle` |
| 火花术 | 言灵法术 | 复用旧图 | `spark` |
| 魔力扰动 | 言灵法术 | 复用旧图 | `disrupt` |
| 克敌机先 | 言灵法术 | 复用旧图 | `true-strike` |
| 银光锐语 | 咒符法术 | 复用旧图 | `barbs` |
| 护盾术 | 咒符法术 | 复用旧图 | `shield` |
| 法术反制 | 咒符法术 | 复用旧图 | `counter-spell` |
| 避火咒 | 咒符法术 | 复用旧图 | `protective-flame` |
| 滑稽魔盒 | 咒符法术 | 新绘透明 PNG | `v2-funny-box` |
| 小六芒星 | 阵法 | 新绘透明 PNG | `v2-small-hexagram` |
| 六芒星 | 阵法 | 复用旧图 | `balance` |
| 五芒星 | 阵法 | 复用旧图 | `pentagram-of-life` |
| 八芒星 | 阵法 | 新绘透明 PNG | `v2-octagram` |
| 太阳 | 阵法 | 新绘透明 PNG | `v2-sun` |
| 正十字 | 阵法 | 新绘透明 PNG | `v2-upright-cross` |
| 逆十字 | 阵法 | 新绘透明 PNG | `v2-inverted-cross` |
| 安卡十字 | 阵法 | 新绘透明 PNG | `v2-ankh` |
| 元素之环 | 阵法 | 新绘透明 PNG | `v2-elemental-ring` |
| 混沌之环 | 阵法 | 新绘透明 PNG | `v2-chaos-ring` |
| 空洞 | 阵法 | 新绘透明 PNG | `v2-void` |
| 硫磺 | 阵法 | 新绘透明 PNG | `v2-sulfur` |

## 验证

所有新图为 RGBA，具有实际 alpha 透明通道；16 张新图均已核对主体、像素风格与结构。八芒星修正为两个交织方形构成的八角结构；炽热射线修正为四条射线。

Godot 4.7.2 实际图形运行通过 **850 项检查**，覆盖 1280×720、1600×1000、1920×1080；验证素材加载、类别、数值、完整详情、中文检索、版本切换、720p 面板边界，并覆盖已有场地交互。运行资产共 266 项，哈希检查通过。原生卡池仍为 30 张；新卡暂不进入对局规则或构筑列表。

[1080p 阵法与详情](previews/library-v2-formation-1920x1080.png) · [1080p 新卡图鉴](previews/library-1920x1080.png) · [720p 法术详情](previews/library-ai-detail-1280x720.png) · [验证报告](previews/verification.json)
