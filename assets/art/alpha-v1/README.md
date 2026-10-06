# Alpha v1 卡牌插图

日期：2026-10-06。依据用户提供的 `WizardCard-CardDesign/Alpha卡池/v1.0卡牌设计.md` 绘制全部11张卡牌插图，延续项目像素奇幻风格。

打开 [预览画廊](index.html) 查看所有图片，并切换深浅底检查透明主体。原始PNG和本目录ZIP均可独立使用。

| 卡牌 | 透明PNG | 插画主题 |
|---|---|---|
| 紊乱波动 | [messy-wave.png](messy-wave.png) | 能量波缠绕重物，表现累积负担 |
| 昏昏入睡 | [instantly-sleep.png](instantly-sleep.png) | 闭眼月牙与下垂羽饰 |
| 魔法飞弹 | [magic-missile.png](magic-missile.png) | 三枚紫蓝色力场飞弹 |
| 支援术 | [aid.png](aid.png) | 完整生命核心外的额外保护层 |
| 法术反制 | [counter-spell.png](counter-spell.png) | 夹断法术的符印与代价配重 |
| 克敌机先 | [true-strike.png](true-strike.png) | 洞察之眼指引卡牌选择 |
| 避火咒 | [protective-flame.png](protective-flame.png) | 将外来火焰偏转的防护结构 |
| 生命之五芒星 | [pentagram-of-life.png](pentagram-of-life.png) | 单尖朝上的交织五芒星 |
| 恩格亚斯四角星 | [engeas-four-point-star.png](engeas-four-point-star.png) | 四个轴向尖角、直线构成的聚能结构 |
| 昂扬 | [uplift.png](uplift.png) | 根据提供的image.png保留上升符文轮廓 |
| 瞬发回路过载 | [instant-circuit-overload.png](instant-circuit-overload.png) | 强行汇入能量的回路与应力裂损 |

全部使用 OpenAI 内置 imagegen 生成，以透明背景模式输出；保留原始RGBA文件，无裁剪、量化或程序重绘。半透明魔法效果与微小alpha边缘残留如实保留，不宣称严格网格、限色或二值透明精修。透明度、实际尺寸和SHA-256见 [manifest.json](manifest.json)。

完整提示词、参考关系及原始文件位置见 [production.json](production.json)。紊乱波动修正了首稿的波纹截断；四角星重新生成以强化四个尖角和内凹轮廓。弃用草稿不纳入交付包。

本批来自新的Alpha规则草案，采用独立文件ID，未修改现有游戏卡定义或运行插画映射。设计案原文件与参考图保持不变。

运行接入：2026-10-06 已映射全部11张至 `../runtime.json`，原始PNG与manifest SHA-256一致；仅修改元数据，不修改图像字节。
