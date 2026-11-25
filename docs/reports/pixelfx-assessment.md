# PixelFX 在 WizardCard 中的适用性

2026-11-24，核对上游提交 `5294518ce7c72e48b6100a744596e0acf57f2826`。

**适合用作 Godot 客户端的离线特效素材工具。** 上游提供 244 种代码生成的像素动画；开发时用 Node.js 18 或更高版本烘焙少量选中的效果，客户端只需加载透明 PNG 图集并按帧播放。项目现有架构已经把战场绘制放在 Godot，把规则结算放在权威 C++ 内核。采用离线图集无需改变规则、联机协议或服务端依赖。[上游用法](https://github.com/patch-gg/pixelfx/blob/5294518ce7c72e48b6100a744596e0acf57f2826/README.md#use-one) · [工具版本要求](https://github.com/patch-gg/pixelfx/blob/5294518ce7c72e48b6100a744596e0acf57f2826/package.json) · [项目架构决策](../adr/0001-alpha-v2-modular-native-engine.md)

## 实测导出

在独立临时检出中运行 `node tools/render.mjs arcane/mana-burst --sheet`，生成了预览图、1456 × 1200 的 RGBA 图集和 Aseprite 风格 JSON。图集共 7 列，包含 55 帧，每帧 208 × 150，30 fps，单次播放。锚点位于帧内 (104, 124)，应映射到目标位置；直接把整帧中心贴在卡牌中心会产生偏移。正式素材可调整配色并重新导出，与当前卡面和文字实测对比度。此次验证覆盖导出与元数据读取，对局内观感及并发播放性能仍待验证。[样例效果](https://github.com/patch-gg/pixelfx/blob/5294518ce7c72e48b6100a744596e0acf57f2826/src/fx/arcane/mana-burst.js) · [图集生成实现](https://github.com/patch-gg/pixelfx/blob/5294518ce7c72e48b6100a744596e0acf57f2826/tools/lib/fxlib.mjs)

另用项目配套的 Godot 4.7.2 在临时工程中以无窗口模式导入 `ice/frost-nova` 图集。脚本读取 JSON，为 103 帧逐一建立图集区域并验证都在 2592 × 2304 的纹理范围内，单帧 288 × 192，锚点 (144, 150)。`AnimatedSprite2D` 以导出的 30 fps 播到末帧（索引 102）并发出 `animation_finished`。该验证覆盖引擎导入与单次播放；实际对局中的画面效果尚待试用。[冰霜新星源文件](https://github.com/patch-gg/pixelfx/blob/5294518ce7c72e48b6100a744596e0acf57f2826/src/fx/ice/frost-nova.js)

## 接入位置

- **第一项试用建议选生命回复。** 现有 `FieldTransition.prepare()` 已比较双方生命值并区分回复和伤害，`paint()` 已取得双方区域。可用一张短促的回复图集叠在现有生命反馈上，复用目标与时序，不占用点击区域。施法发动等需要判定动作性质的表现应消费查看者已确认可见的事件；仅凭卡片位置或状态变化，无法判定具体法术已经生效。[场地过渡](../../godot/ui/field_transition.gd) · [对局事件消费](../../godot/pages/match_page.gd) · [上游效果目录](https://github.com/patch-gg/pixelfx/blob/5294518ce7c72e48b6100a744596e0acf57f2826/docs/CATALOG.md)
- **最少的适配工作是切帧、定时、定位和清理。** 对于首个效果，可按 JSON 中的有效帧数、帧矩形和时长在现有 `FieldCanvas` 绘制，或在编辑器中预建 `SpriteFrames`。换手遮罩出现、查看者改变或用户开启减少动效时，应沿用现有反馈的跳过与清理路径。试用时复核动画期间卡牌仍可点选、连续投影更新即时生效、换手后无残留画面。[场地绘制与隐私控制](../../godot/ui/field_canvas.gd) · [现有反馈验收](../../godot/tests/feedback_smoke.gd) · [Godot 精灵图集动画](https://docs.godotengine.org/en/stable/tutorials/2d/2d_sprite_animation.html#sprite-sheet-with-animatedsprite2d)
- **按需收录素材。** 样例图集单份按未压缩 RGBA8 计算约 6.7 MiB，实际显存取决于导入压缩和 mipmap 设置。先以单个短动效在实际卡牌尺寸下检查清晰度、卡面遮挡和连续触发时的表现，再决定要收录的数量。仓库采用 MIT 许可，复用代码或实质性部分时需保留 Patch 与 Hugo Duprez 的版权和许可声明；随引用素材归档上游 LICENSE 可保留来源记录。[上游许可](https://github.com/patch-gg/pixelfx/blob/5294518ce7c72e48b6100a744596e0acf57f2826/LICENSE)
