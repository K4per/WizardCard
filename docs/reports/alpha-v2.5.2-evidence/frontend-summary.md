# Alpha v2.5.2 Godot 前端最终验证

日期：2026-10-21。正式卡池 102 张，规则/卡池版本 2.5.2，机制修订 5。使用 `.cache/godot-portable/Godot_v4.7.2-stable_win64_console.exe`，测试串行执行。

| 脚本 | 结果 | 明确报告的检查数 |
|---|---|---:|
| frontend | PASS，六种桌面分辨率、102 张正式卡 | 2837 |
| match_frontend | PASS，设置、本地双人、权威提交、回放、AI 隐私、教学、房间 | 未输出计数 |
| field_v25 | PASS，环位、阵体、9 枚符文独立命中 | 未输出计数 |
| phase_selector | PASS，退出码 0 | 未输出计数 |
| card_selection | PASS，权威目标、旧 revision、移除阵法确认 | 未输出计数 |
| card_tray | PASS | 未输出计数 |
| deck_frontend | PASS | 354 |
| typography | PASS，六种分辨率、四档字号 | 700 |
| feedback | PASS，快速投影、隐私、失焦、旧 Tween 取消 | 13 |
| deck_exchange | PASS，0 failures | 未输出计数 |
| network_frontend | PASS，0 failures | 未输出计数 |
| art_residency | PASS，102 张卡、98 个唯一插图资源 | 699 |

所有最终 `.log` 均无 `WARNING:`、`ERROR:` 或 `SCRIPT ERROR`。`frontend-final.json` 保存完整页面报告，`final-logs/script-results.json` 保存脚本状态，逐项日志位于 `final-logs/`。

## 实际渲染

Windows Compatibility 渲染器，OpenGL 3.3，NVIDIA GeForce RTX 4070 Laptop GPU：

- `final-captures/art_residency/official-art-library.png`：102 张正式卡的图鉴样式，1600×1800。
- `final-captures/art_residency/official-art-deck.png`：102 张正式卡的构筑样式，1600×1800。
- `final-captures/field_v25/`：1280×720、1600×1000、1920×1080 战场、悬停和不同阵体截图。
- 实际渲染图片专项：703 项检查、0 failures；战场实际渲染脚本 PASS。

已逐张目视检查两张全卡池总览，正式插图均可见，无占位图或空白。正式卡牌中存在共用设计插图，因此 102 张正式卡对应 98 个唯一插图资源。

## 测试脚本修正

- 战场脚本在 headless 模式忽略截图目录，防止等待不会发出的 `frame_post_draw`。
- 合成卡夹具显式指定当前正式插图，不再把合成中文 ID 误报为缺失资源。
- 图片专项分别报告正式卡数量和唯一插图数量，避免把共用插图误算为卡池缺牌。

上述为追加三张味契卡图前的完整前端回归。追加透明卡图后，正式资源为 102 卡、101 个唯一插图；工作区实际渲染 718 项通过。重新导出的干净包 headless 714 项、实际渲染 718 项通过，0 failures，见同目录 `clean-art-headless.log` 与 `clean-art-rendered.log`；同目录两张 `official-art-*.png` 已更新为干净包截图。
