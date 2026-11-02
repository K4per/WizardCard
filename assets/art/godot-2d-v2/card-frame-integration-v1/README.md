# 原始 ImageGen 卡框接入 Godot

2026-10-11，按用户最新决定，五种 C 风格完整卡框直接投入 Godot，不再选择简化模板。卡框与匿名卡背来自 `assets/art/godot-2d-v1/export/c-pixel-v1/cards/`，尺寸均为 1086×1448，运行副本与原图 SHA-256 完全一致，没有重绘、裁图、改色或缩放导出。

运行时按原始 3:4 比例绘制，使用最近邻纹理采样；文字、插画、稀有度组件动态叠加。场上卡、手牌、图鉴和详情统一使用原始框体。小尺寸减少文字密度，完整能力文本可在详情面板阅读。传说稀有度使用金黄色宝石。

以下 PNG 均是 **Godot 实际渲染的 1920×1080 截图**：

- [五种卡框与稀有度对照](card-frame-types-1920x1080.png)：仅美术排版示例，示例类型与稀有度不改写正式卡池。
- [正式卡池图鉴与完整卡面详情](library-ai-detail-1920x1080.png)：现有 30 张卡牌。
- [场地预览与手牌](field-1920x1080.png)：现有展示数据，不代表可运行的正式对局。

源稿统一通过 `tools/prepare_godot_frontend_assets.py` 同步，字段排版由运行 manifest 的 `runtimeLayout` 指定。详见 [接入报告](../../../../docs/reports/card-frame-art-integration.md)。
