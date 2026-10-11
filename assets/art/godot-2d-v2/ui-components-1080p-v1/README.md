# 精细奇幻像素 UI 组件与设置界面

2026-10-11。沿用用户选定的 C 风格：金色金属纹饰、深蓝底材、青色宝石。使用内置 `image_gen.imagegen` 生成六份独立透明组件，以现有原始 AI 卡框和主菜单按钮为风格参考。

原稿保存在 `source/`；按照用户已授权的尺寸归一化，仅去掉透明空边、最近邻调整尺寸，再居中放到 **1920×1080 RGBA** 画布，导出位于 `export/`。没有改色、重画或抹除图案。实际区域、九宫格边距、SHA-256 与原稿尺寸见 [manifest.json](manifest.json)。这是 AI 栅格原稿，不是 Pixelorama 分层工程。

| 资源 | 稳定 ID 后缀 | 实际区域尺寸 | Godot 使用位置 |
|---|---|---:|---|
| 纹饰面板 | `panel` | 440×280 | 设置、图鉴详情、通用面板、下拉弹层；九宫格拉伸 |
| 输入/选择框 | `field` | 500×72 | 下拉选择、搜索框、通用页签与滚动条材质 |
| 滑条轨道 | `slider-track` | 520×32 | 总音量、音效音量；横向九宫格 |
| 宝石滑块 | `slider-knob` | 36×44 | 音量控件的拖动把手 |
| 开关开启 | `toggle-on` | 104×44 | 精简动画、自动通过 |
| 开关关闭 | `toggle-off` | 104×44 | 同上，灰色左侧旋钮 |

ID 前缀统一为 `c2d.v2.ui.`。普通、悬停、按下、禁用按钮复用已经完成的 C 风格 ImageGen 主菜单五态素材；焦点采用透明描边，保留底层状态。控件文字与数值全部由 Godot 实时显示，图案不烘焙功能文案。

设置界面按声音、显示和操作分组，保留现有设置字段与保存逻辑。通用主题同步用于返回按钮、图鉴搜索和详情；已有战场专用皮肤继续通过独立覆盖使用。本批不增加联机、账号、语言等未实现设置项。

## 实际效果

- [1080p 设置](previews/settings-1920x1080.png)
- [720p 设置](previews/settings-1280x720.png)
- [1080p 下拉选择](previews/settings-dropdown-1920x1080.png)
- [1080p 图鉴详情](previews/library-ai-detail-1920x1080.png)

以上都是 Godot 实际渲染截图。源组件全部为 1080p 画布；720p 图片仅用于运行适配验证。

## 生成与同步

完整提示词由 [style-prompt.json](style-prompt.json) 的共用风格提示词加各项 `assetPrompts` 组成。内置工具生成路径另记在 [generation-prompts.json](generation-prompts.json)，不作为唯一资产保存位置。

```powershell
python tools/prepare_refined_ui_art.py assets/art/godot-2d-v2/ui-components-1080p-v1/generation-prompts.json
python tools/prepare_godot_frontend_assets.py
python tools/prepare_godot_frontend_assets.py --check
```

运行资产位于 `godot/art/ui-v2/`。按来源清单同步，不依赖硬编码临时图片路径。详见 [接入报告](../../../../docs/reports/refined-ui-art-integration.md)。
