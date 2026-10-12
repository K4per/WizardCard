# Alpha v2.5.0 对局界面参考研究

日期：2026-10-12。本次使用官方页面、发布商截图和原项目源码建立交互基线，没有安装或实际运行竞品客户端。静态截图只证明可见布局，源码才用于核实输入与计时行为。

## 来源和范围

| 对象 | 来源 | 已核实行为 |
| --- | --- | --- |
| 游戏王：大师决斗 | [KONAMI 官方页](https://www.konami.com/yugioh/masterduel/us/en/)、[Steam 商店](https://store.steampowered.com/app/1449850/YuGiOh_Master_Duel/)、[发布商对局截图](https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/1449850/ss_f4e2257edc1cb8093f1f0b40d7daf96211405f90.1920x1080.jpg) | 稳定场地区域、手牌、牌库、详情栏、回合和阶段信息。不能由截图推断悬停事件或扣时规则。 |
| YGOPro 2 | [原项目 gameCard.cs](https://code.moenext.com/nanahira/ygopro2/-/blob/c0d2c904822319ef819b0a7685b11c7ee3ffd409/Assets/SibylSystem/Ocgcore/OCGobjects/gameCard.cs)、[gameField.cs](https://code.moenext.com/nanahira/ygopro2/-/blob/c0d2c904822319ef819b0a7685b11c7ee3ffd409/Assets/SibylSystem/Ocgcore/OCGobjects/gameField.cs)、[Ocgcore.cs](https://code.moenext.com/nanahira/ygopro2/-/blob/c0d2c904822319ef819b0a7685b11c7ee3ffd409/Assets/SibylSystem/Ocgcore/Ocgcore.cs) | 卡牌 excited/悬停状态更新阅读详情、手牌抬起和上下文操作；场地位置及服务端时间消息由核心更新。 |
| MD Pro3 | [原项目](https://code.moenext.com/sherry_chaos/MDPro3)、[GameCard.cs](https://code.moenext.com/sherry_chaos/MDPro3/-/blob/e177cd8d53092ae9249e37b38d3cef0040e41095/Assets/Scripts/MDPro3/Duel/GameCard.cs)、[DuelBGManager.cs](https://code.moenext.com/sherry_chaos/MDPro3/-/blob/e177cd8d53092ae9249e37b38d3cef0040e41095/Assets/Scripts/MDPro3/Duel/BG/DuelBGManager.cs) | 操作按钮驱动可用卡高亮；悬停抬手牌、点击详情；双方牌库坐标中心对称。 |

YGOPro 2 固定提交为 `c0d2c904822319ef819b0a7685b11c7ee3ffd409`，MD Pro3 固定提交为 `e177cd8d53092ae9249e37b38d3cef0040e41095`。前期 EDOPro 阅读只作为 YGOPro 家族的补充对照，不用它代替本计划中的 YGOPro 2。

MD Pro3 [PhaseButtonHandler.cs](https://code.moenext.com/sherry_chaos/MDPro3/-/blob/e177cd8d53092ae9249e37b38d3cef0040e41095/Assets/Scripts/MDPro3/UI/UIComponents/PhaseButtonHandler.cs) 维护阶段/回合与合法阶段入口；[TimerHandler.cs](https://code.moenext.com/sherry_chaos/MDPro3/-/blob/e177cd8d53092ae9249e37b38d3cef0040e41095/Assets/Scripts/MDPro3/UI/UIComponents/TimerHandler.cs) 接收时间后更新数字与比例；[PortHelper.cs](https://code.moenext.com/sherry_chaos/MDPro3/-/blob/e177cd8d53092ae9249e37b38d3cef0040e41095/Assets/Scripts/MDPro3/Helper/PortHelper.cs) 和 [BackgroundManager.cs](https://code.moenext.com/sherry_chaos/MDPro3/-/blob/e177cd8d53092ae9249e37b38d3cef0040e41095/Assets/Scripts/MDPro3/Managers/BackgroundManager.cs) 支持背景导入和比例适配。本版 WizardCard 仅实现用户确认的内置背景主题范围。

## 本项目交互基线

| 需求 | 可验证的落地标准 |
| --- | --- |
| 背景自定义 | 设置中的内置主题持久化，对局与复盘使用相同标识。 |
| 解析区分格 | 每方 5 行，每行是阵法槽和独立法术区域；环位数来自投影，空位常驻。 |
| 并列状态栏 | 双方生命、临时生命、魔素、荷载和时间可同时扫描。 |
| 可用卡高亮 | 只根据 `snapshot.actions[].source` 标记；点击详情与提交动作分开。 |
| 手牌展开/收起 | 单一控件切换，右键和 Esc 保留取消行为。 |
| 牌库位置 | 主牌库、副卡组和灰烬区位于两侧，双方做中心变换；隐藏牌库仅展示数量。 |
| 悬停详情 | 无需点击即可阅读公开卡；盖伏未知卡不显示身份；点击可固定详情。 |
| 中央施法区 | 双方中央紧凑轨道，不挤占五行解析区；法术发动实体在施法区。 |
| 回合/时间/阶段/推进 | 固定 HUD；结束按钮只有权威提供 AdvancePhase 时启用。 |
| 中心对称 | 同一份场地几何映射到对手，文字保持正向；详情面板为浮层。 |

游戏王的怪兽区和魔法陷阱区不等同于 WizardCard 的解析区、施法区。区域数、来源和操作限制以本项目规则及 [GLOSSARY](../../GLOSSARY.md) 为准。

## 用户确认后的布局

用户提供的参考图明确采用纵向 5 行，每行左侧阵法槽、右侧法术区域；此前 3+2 阵法模块原型已被该决定替代。双方采用中心对称，施法区置于中央两条紧凑轨道，场地覆盖窗口。顶部仅保留回合/阶段/状态及“结束当前阶段”，暂停、设置、复盘和离开等放到 Esc 菜单。对局不使用此前装饰美术，采用简洁色块、边框和文字。

统一操作闭环为：悬停阅读 → 点击选中 → 选择权威合法动作 → 选择位置/目标/成本 → 确认提交 → 更新投影。Esc 每次先取消当前交互层，空闲时打开菜单。强制决策不能通过取消跳过。

专注、计时和永续首次发动也进入施法区，之后按既定时序回位。维持询问和递减不构建发动链；前端不会为此生成第二次发动。

## 原型和验收入口

灰盒：`godot/pages/field_greybox.tscn`。实际对局：`godot/pages/match_page.tscn`。共享场地：`godot/ui/field_canvas.tscn` 和 `godot/ui/default_field_layout.tres`。编辑器预览不连接原生会话或读取用户文件。

自动化入口：`field_v25_smoke.gd` 检查投影环位、区域对称、背景、悬停与时钟；`match_frontend_smoke.gd` 验证真实鼠标、Esc、手牌、开局隐私、AI和教学；`frontend_smoke.gd` 验证页面骨架和三种尺寸。截图必须来自 Godot 实际渲染，不能以概念图替代。

竞品实机体验、双台实体设备、真人平衡测试仍没有由本次源码研究证明。
