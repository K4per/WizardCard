# A 古籍金饰交付

入口 gallery.html；资产清单 ASSET_LIST.md；组件参数 ../../ui/a-gilded-v2/manifest.json。

build_assets.py 使用 Python/Pillow，依赖上一批 c-theme-v1/build_assets.py 和 v1/build_ui.py；这些依赖、当前 cards.json、runtime.json、字体和既有插画都包含在交付 ZIP 中。painted/ 下十二张原始生成 PNG 不可由该脚本重新生成，来源及提示词另附。其余 PNG 的分层 SVG 位于 sources/。

文字、数值及插画均动态组合；previews/ 属于排版示例，不是游戏中的底板资源。未修改游戏运行时。
