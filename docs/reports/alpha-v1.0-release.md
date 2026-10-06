# Alpha v1.0 自动交付验收

日期：2026-10-07。程序 `1.0.0-alpha`、规则/卡池 `1.0.0`；Windows x64、MSVC2022、Release。用户明确取消本次约20局真人门槛，按自动验收交付；此报告不把自动跑局称为真人试玩。

## 交付变化

文本渲染按字形边界定位，按钮、卡名、决策提示、详情和日志按实际字体宽高适配；卡名预留状态与防护徽标空间。施法顺序决策可选择结束释放，已准备法术保留在施法区及绑定荷载，下一次自己的施法阶段可释放且不重复支付。言灵设置与即时释放通过命令的显式参数区分，UI、拖放、AI和复盘统一使用。初始及最大生命均为40，治疗与教学条件同步。

增加六张泛用牌与原始AIGC插画，卡池30种；九套合法预设中包含五种代表性方向，每种都附打法说明。未以同一策略简单换牌作为五种方向。源设计与规则见[卡池说明](../alpha-release-cards.md)。

EXE包含版本资源和七尺寸图标（16、24、32、48、64、128、256），窗口重建时同步加载徽标；缺失窗口图标不改变规则。图标原始PNG、ICO及生成提示见 `assets/art/branding/`。六张新增卡面均保存原始透明PNG、提示和SHA256，未改写用户原图。

## 已运行

| 检查 | 结果与证据 |
|---|---|
| Windows Release完整回归 | 179/179，83.15秒；`build/alpha-release-tests.log` |
| 无窗口Windows Release完整回归 | 179/179，70.23秒；`build/alpha-release-headless-tests.log`；无SFML，不能当作Linux运行结果 |
| 五方向、三档AI完整对局 | 15局40生命正常核心对局，全部终局且逐步确定性重放；`build/alpha-v1-new/`与新卡AI回归 |
| 原生菜单与构筑 | 实际窗口菜单、设置/暂停、换手遮挡、生命周期、保存/查询/独立卡组通过；`build/alpha-release-menu-test.log`、`build/alpha-release-deck-test.log` |
| 原生三档AI与固定教学 | 三档完整对局、私有视角、设置暂停、重开和真实反制/释放教学通过；`build/alpha-release-ai-test.log` |
| 最终客户端点击重放 | 257条上下文操作，摘要 `a8a287146d4d175d`；`build/alpha-final-ui.log` |
| 最终客户端拖放重放 | 292条上下文操作、86次拖放，摘要 `1f4269a8686b1b80`；`build/alpha-final-drag.log` |
| 音频 | 17个MP3解码及实际播放控制、六声音上限、限频/终局优先、静音、缺失/损坏回退通过；`build/alpha-final-audio.log`；invalid.mp3错误是故意的损坏回归 |
| 示例跨构建复盘 | Release CLI生成，无窗口Release CLI读取，摘要均 `78116a78d64b0d90`；`examples/quick-match.json`已升级版本 |
| 图标和版本资源 | 直接读取PE资源验证七个RT_ICON及RT_GROUP_ICON；FileVersion/ProductVersion均1.0.0-alpha；`build/alpha-release-icon-check.json` |
| 窗口比例与文字 | 1600×1000主场地/防护详情、1280×800构筑、1920×1080场地、960×600设置截图人工检查；`build/alpha-release-final-match.png`、`build/alpha-final-editor-protective.png`、`build/alpha-ratio-*.png` |

新增规则回归实际覆盖“跳过后保留准备法术与荷载、下一施法阶段无重复支付”“零环只设置、满后场与设置释放区别”“六张新牌的伤害、防护、清理、恢复与独立荷载”。旧版本拒绝、强制决策、私有视角和三档AI继续沿用已有回归。

## 独立发行目录

`dist/WizardCard-Alpha-v1.0/` 为便携目录，`dist/WizardCard-Alpha-v1.0-windows-x64.zip` 为发行ZIP，SHA256校验文件在ZIP旁单独保存，避免将ZIP自己的哈希写回ZIP导致循环。

从ZIP提取到新的 `build/alpha-release-extracted-final2/`，不传源码资源路径，CLI验证卡池与复盘、客户端构筑/菜单及音效检查通过。发行资源包含全部运行卡面、字体、界面纹理、17个音效、图标、来源清单和依赖许可；不携带开发原型资源、源码或测试用户数据。完整操作日志为 `build/alpha-release-package-check.log`。测试用隔离用户目录，不改写现有用户设置与卡组。

## 实际限制

未运行LinuxCI、另一台机器的复盘或其他声卡/显示设备验证，本机两个Windows配置的一致性不替代这些结果。本次取消真人门槛，只确认功能与自动稳定性；五种方向的相对强度仍需后续反馈。没有发布外部托管链接、Git标签或提交；交付为本地可分享ZIP。后续目标见[v1.5计划](../plans/v1.5.md)。
