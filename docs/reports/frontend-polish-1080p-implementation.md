# WizardCard 前端精修实施记录

日期：2026-10-10

本轮将 Godot 前端的主设计尺寸切换为 1920×1080，并保留 1280×720 与 1600×1000 的响应式重排。新用户首次启动采用 1920×1080；已有 `settings.json` 的合法尺寸继续生效，窗口会按桌面可用区域居中和限制。

菜单、设置和战场已完成第一轮灰盒精修：菜单品牌与主操作尺寸增大；设置分为声音、显示与操作分组，支持滚动、滑条数值、专用下拉/开关/复选皮肤；战场接入背景、主战面和边缘覆盖层，手牌按 160×224 设计并在大手牌时受控重叠，阵法与环位仍由展示投影驱动。

Pixelorama 已生成四个可编辑分层样板，源稿位于 `assets/art/godot-2d-v2/source/pixelorama/`，导出位于 `assets/art/godot-2d-v2/export/`，并记录在 `assets/art/godot-2d-v2/manifests/pixelorama-v2-samples.json`：192×48 主按钮、64×64 魔素图标、64×64 阵法环位和 128×96 石质竞技场材质。样板暂不写入正式 `godot/art/assets.json`，接入前仍需完成稳定 ID、哈希和运行资源同步。

## 验证

- `python tools/prepare_godot_frontend_assets.py --check`：242 项、43,447,018 字节通过。
- Godot 4.7.2 导入通过，无脚本错误。
- `frontend_smoke.gd --capture`：301 项通过，覆盖 1280×720、1600×1000、1920×1080；实际卡池 30 张。
- 已检查设置页和 1920×1080 战场截图；对局仍为 `FieldShowcase` 展示页，不代表真实规则会话已经接入。
