# 中文字体与字重

采用官方 **Noto Sans CJK SC / Sans2.004** 静态 OTF。新增 Bold 与 Black 两份原始字体，未改造字形、未子集化；正文继续使用已有的 `assets/fonts/NotoSansCJKsc-Regular.otf`。

| 用途 | 字重 | 设计字号 |
|---|---:|---|
| 主菜单五个入口 | Black 900 | 1080p：40px；1000p：36px；720p：30px |
| 页面和分组标题 | Bold 700 | 22px 及以上 |
| 按钮、下拉选项、开关、卡名与数值 | Bold 700 | 沿用对应控件字号 |
| 卡牌规则、说明、搜索与小字号辅助文字 | Regular 400 | 沿用对应控件字号 |

菜单配合米金色文字和 2px 深色描边提高复杂背景上的识别度。未开放入口保留超粗字重，但采用较暗文字与禁用皮肤。材质保持像素采样；文字使用 Godot 动态字体正常抗锯齿与 hinting。

## 来源与许可

- 官方仓库：[notofonts/noto-cjk](https://github.com/notofonts/noto-cjk/tree/Sans2.004)。具体下载地址、版本、实际字重和 SHA-256 记录在 `manifest.json`。
- 许可证：[SIL Open Font License 1.1](https://github.com/notofonts/noto-cjk/blob/Sans2.004/LICENSE)，完整原文为同目录 `OFL.txt`。
- 字体内部版权信息：© 2014-2021 Adobe (http://www.adobe.com/)。保留原始文件、内部版权元数据以及独立许可原文；随游戏分发字体时一并携带许可。
- 运行镜像位于 `godot/art/fonts/`。粗体与超粗体的许可由 `font.cjk.weights.license` 指向 `NotoSansCJK-OFL.txt`，已有 Regular 的许可继续独立保留。
- 统一入口为 `godot/ui/typography.gd` 与 `godot/ui/pixel_theme.tres`。同步脚本 `tools/prepare_godot_frontend_assets.py` 会校验原稿、运行字体和许可的哈希。

## 实际效果

`previews/` 保存 Godot 实际运行截图：1080p 与 720p 主菜单、1080p 设置页和卡牌详情。文字为运行时绘制，未烘焙进按钮图片。
