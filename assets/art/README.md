# WizardCard 美术资源总索引

核对日期：2026-10-07。当前Godot程序1.5.0-dev，规则/卡池1.0.0，共30种卡牌。以 [Godot最新需求](../../docs/art-resource-requirements-godot-v1.5.md) 为准，制作/接入/验收分别见 [状态清单](../../docs/art-resource-status-v1.5.md)。机器索引：[catalog.json](catalog.json)。

| 分类 | 当前入口 | 状态 |
|---|---|---|
| 30张透明卡图 | [runtime.json](runtime.json)、cards/、[provenance.json](provenance.json) | 当前使用，原件保留 |
| A古籍金饰卡框/UI | [314 PNG清单](ui/a-gilded-v2/manifest.json)、[源稿画廊](design/a-gilded-v2/gallery.html) | 当前基础组件；五类卡框、五档稀有度，传说为金黄色宝石 |
| v1.5非建模增补 | [91 PNG及配置](nonmodel/a-gilded-v15/manifest.json)、[画廊](design/nonmodel-v15/gallery.html) | 四套场景PBR、纸边、区域、HUD、交互、连锁、15类特效和动作库 |
| 品牌 | [Logo](branding/wizardcard-logo-v1.png)、[图标](branding/icon-manifest.json) | 当前使用 |
| 字体 | ../fonts/NotoSansCJKsc-Regular.otf | 独立文字渲染，保留字体许可 |
| 声音 | [音效清单](../audio/manifest.json) | 沿用17文件、18事件 |

旧 `ui/manifest.json` 的190组件、C版提案、早期布局稿和 prototypes 保留为历史资料，不覆盖当前A版和2.5D布局。按索引整理，不搬迁或删除正在被引用的文件。原A版源清单的 `runtime_integrated:false` 是历史交付记录，实际Godot状态以 [art-integration.json](../../godot/art-integration.json) 为准。

新资源源稿位于 `design/nonmodel-v15/`，原件位于 `nonmodel/a-gilded-v15/`，Godot镜像位于 `godot/art/nonmodel/a-gilded-v15/`，逐文件SHA核对。像素插画/图标nearest，卡框/PBR使用线性或mipmap。运行组件不烘焙文字，预览图不用作背景。

本批为项目原生数学纹理与SVG制作，未修改AI卡图、卡框或金黄宝石，来源/许可沿用原记录。模型MOD-01～04与摆件PRP-01待制作。
