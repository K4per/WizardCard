# 地下城对局美术接入

2026-10-11。新美术已接入现有 Godot 场地预览页，可从主菜单的“场地预览”进入。仍使用展示投影，尚未提交真实对局命令。

## 已接入

- 地下城背景、连续施法区地面刻纹、按阵法重复的解析行、牌库与灰烬区底座。
- 解析区显示卡图裁切；盖伏卡只显示匿名卡背，点击详情同样遵守隐藏投影。
- 手牌与放大卡牌沿用既有完整卡框。详情改为完整卡牌和一个独立信息框；信息框重新使用 imagegen 生成，未使用旧多框面板。
- 点击卡牌出现上方操作条，当前提供详情和明确标为示例的效果选择入口。
- 卡牌效果候选选择弹窗：候选点击、选中反馈、完整卡牌预览、滚动/翻页、选择数量校验、确认及可选取消。
- 生命、魔素、荷载图标和裁切填充的荷载条，显示值来自 FieldPresentation；展示数据仅由 FieldShowcase 提供。
- 弹窗锁定场地输入；强制决定隐藏返回且不能通过 Esc 退出，旧 generation/revision/decision ID 回调继续拒绝。

## 文件

Godot 素材位于 `godot/art/battle-v3/`。13张源图保留 imagegen 原始像素，以 `regions.json` 中的 AtlasTexture 区域复用，默认最近邻采样。`BattleArt` 单独缓存图集，不覆盖已有菜单/卡框资产注册表。

最新信息框源图为 `assets/art/godot-2d-v2/battle-production-v3/13-detail-information-panel.png`，提示词为同目录 `prompt-detail-v4.txt`。旧素材压缩包为生成阶段快照，不包含此次新增信息框；Godot 实际使用以上目录中的当前文件。

## 验证

Godot 4.7.2 导入成功。实际 OpenGL 渲染检查通过425项，覆盖1280×720、1600×1000、1920×1080以及30张官方卡牌。检查包括鼠标点击、拖放不改变投影、隐私遮挡、长文本、过期决定回调、候选选择和强制决定取消防护；最终日志无脚本错误。修改文件的 git diff --check 通过。

报告：`build/battle-v3-render-report.json`。实际截图：`build/battle-v3-captures/field-1920x1080.png`、`battle-selected-detail-1920x1080.png`、`battle-effect-selected-1920x1080.png`。

本次未修改原生规则、联网或权威效果解析。真实对局的动作列表、效果候选与命令提交仍需由会话接口接入；当前示例操作不会发动卡牌效果。
