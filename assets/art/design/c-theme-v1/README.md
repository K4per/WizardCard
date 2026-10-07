# C 风格资源

入口：gallery.html；完整清单：ASSET_LIST.md；运行时资源：../../ui/c-theme-v1/manifest.json。

运行 build_assets.py 需要 Python / Pillow，并依赖 ../v1/build_ui.py、assets/fonts/NotoSansCJKsc-Regular.otf、assets/cards.json、assets/art/runtime.json 与既有卡牌插画。所有这些依赖均包含在 ZIP 中。素材目录不会修改旧 UI 或运行时配置。

分层 SVG 中图形可编辑，透明插画窗以 mask 定义；实际游戏使用独立 PNG 与动态文字。预览 SVG 引用原始插画，文字可编辑。
