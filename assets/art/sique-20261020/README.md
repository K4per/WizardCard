# 味契插图来源与再生成

本批次接入用户提供的 16 张 JPEG，未生成新的插图。`source/` 保留原始文件字节；`manifest.json` 记录原始名称、卡牌名、稳定 ID、源文件与导出文件 SHA-256，以及全部处理参数。用户目录中的旧 `weqi_*.png` 设计不参与本次正式映射。

`tools/prepare_sique_art.py` 使用 Pillow 去除近黑背景、清理右下角源图标记、按非透明内容裁切，再以 nearest 采样居中放入 1024×1024 RGBA 画布。插图主体最大 900×900，四周保留透明安全边。运行时镜像位于 `godot/art/card/sique-*.png`，通过 `illustration.sique-*` 与定义关联。

```powershell
python tools/prepare_sique_art.py
python tools/prepare_godot_frontend_assets.py --check
python tools/check_art_assets.py --update-inventory
```

原图、处理导出和说明仅用于创作维护，不进入便携包；发布使用运行时镜像。接入时逐张视觉确认了 16 张图的主体和名称，并在中灰背景检查透明边缘、内孔和原图标记清理。稳定 ID 以正式卡牌定义为准，旧设计中的「抗性煎饼」「沸汤滚裂」分别映射至本版「糖浆煎饼」「沸汤滚袭」。
