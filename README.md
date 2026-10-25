# 巫师牌 / WizardCard

**Alpha v2.5.5-hotfix.1**：C++17 权威规则内核、Godot 4.7.2 前端、40+10 构筑、三档 AI、教学、复盘和局域网会话。程序版本为 `2.5.5-hotfix.1`，规则与卡池版本为 `2.5.5`，机制修订为 `9`。

正式卡牌见[卡池图鉴](docs/card-catalog.md)和[图文浏览版](docs/card-catalog.html)，规则见[Alpha v2.5.5](docs/alpha-v2.5-rules.md)，术语见[GLOSSARY.md](GLOSSARY.md)。本次规则修正和验证结果见[2.5.5-hotfix.1发行说明](docs/releases/2.5.5-hotfix.1.md)与[交付报告](docs/reports/2.5.5-hotfix.1.md)；旧版文档与兼容夹具保留在源码仓库中，用于历史验证。

## 运行便携包

Windows x64 便携包解压后运行 `WizardCard.exe`。保留同级 `WizardCard.pck`、`bin/`、`assets/` 和运行库 DLL；`SHA256.json` 提供逐文件校验。无需安装 Godot。

主菜单保留当前 Logo，左侧标识、右侧七个纵向主按钮依次为单人游戏、多人游戏、卡组构筑、卡牌图鉴、对局复盘、游戏设置、退出游戏。单人游戏可选择挑战 AI、本地双人换手或新手教学。AI 有入门、标准、进阶三档；当前提供10套合法40+10预设，也可使用保存的合法自建卡组。普通对局使用随机种子；开发 CLI 可指定种子复现。

每方开局私下从自己的副卡组选择一张合法等级1阵法，双方确认后同时公开。初始阵法不保存在卡组草稿中。本地换手时先显示交接遮挡，确认接手后才显示该玩家的手牌；AI 模式始终采用本方视角。

场地每方有5个阵体槽位。阵体2的阵法连续占2槽，阵体4占4槽，环位铺在对应区域，剩余槽位保持空白。双方初始生命及普通生命上限均为40，开局魔素0、储存上限20；具体费用、荷载与时机由规则内核决定。

## 对局操作

悬停卡牌查看详情，手牌同时抬升并回正，离开后恢复。点击卡牌固定详情并显示合法操作；选择操作后，根据高亮选择宿主、环位、目标或额外材料，最后确认提交。可设置、附着或发动的牌亮金光，可盖伏的牌亮蓝光；有合法阵法设置动作时副卡组亮金光。

牌面右上角显示实时费用，解析法术为设置/施法双费用，咒符仅显示施法费且盖伏免费。附着符文以宿主旁的小方格显示，合法宿主可以附着多个符文。场上盖伏卡不显示卡名；己方可查看完整详情，对方默认只见匿名牌背，查看或展示效果按内核授予的权限生效。卡面写“选择一个目标”的伤害和治疗法术可以选择任一玩家；明确写“对方”的效果固定处理对方玩家。

副卡组、灰烬区及规则选择使用底部卡牌托盘。选择候选后显式确认；收起托盘可暂时查看手牌，强制选择仍然保留。响应可选择确认或放弃。手牌按钮及右键可收起、展开手牌；Esc 先取消当前临时选择，空闲时打开对局菜单。

双方状态栏区分生命、临时生命、魔素和荷载，荷载悬停可查看来源；对局与复盘的日志默认收起，点击日志按钮展开。回合、行动方和阶段提示位于切换阶段按钮上方。主要阶段可进入施法阶段或直接结束；结束时仍会处理必要响应及强制选择。准备施法、释放顺序、回位、弃牌及额外成本都来自当前合法动作，界面不会自行推算或支付费用。

对局菜单提供设置、阶段操作、保存复盘和离开。挑战 AI 时还可打开单人调试控制台，加入卡牌、调整资源、独立切换无限资源或回到上一回合；调试操作由会话校验并记录，联机、换手、教学与回放不可调用。本地与 AI 对局进入设置后暂停；局域网计时不因菜单或设置暂停。离开进行中的对局需要确认，终局记录保存失败时可取消或重试。教学通过正常规则引导阶段、解析、准备、反制和伤害，不强制结束对局。

## 卡组构筑

主卡组固定40张，包含解析、言灵、咒符和符文；副卡组固定10张阵法。主、副卡组分别限制同名最多3张，副卡组必须包含可用于开局选择的等级1阵法。正式卡池135种：30张阵法、42张解析法术、29张言灵法术、19张咒符法术和15张符文，包含19张味契系列卡、15张风契系列卡、16张七星系列卡及20张血契系列卡。正十字、逆十字和安卡十字保留为不可构筑草稿；完整迁移裁定见[Alpha v2.5.4逐卡差异报告](assets/design/alpha-v254/card-diff-report.md)。

从卡组列表新建草稿、复制预设或修改自建卡组。构筑页采用与对局同色的深蓝灰三栏布局：左侧插画与完整详情，中间同时显示40张主卡组和10张副卡组，右侧卡池与搜索筛选。保留卡牌插画，取消装饰背景、卡框和按钮素材。右键卡池加入一张，右键已构筑卡牌移除一张，也可拖动到另一栏增减；悬停或点击查看完整详情，Esc 或右键取消拖动。

可按卡名、类型、稀有度、费用、施法费用、位阶和标签筛选。草稿采用格式3，仅保存主卡组与副卡组，不保存固定初始阵法。卡组列表提供可读JSON文件导入和导出；非法导入会指出卡牌及原因并保留当前草稿。未完成草稿可保存；只有名称、40+10数量、同名上限及卡牌合法性全部通过才进入开局选项。内容升级后保留失效 ID 并显示问题，旧草稿可修复；损坏或不兼容文件保留原件并暂时禁止写入。

## 设置、数据与复盘

设置包含总音量、音效音量、窗口/无边框/全屏、分辨率、精简动态效果、自动阶段推进及场地背景。提供1280×720、1600×900、1440×900、1600×1000、1920×1080和2560×1440，也支持自由缩放窗口；全局字号支持90%、100%（默认）、110%、120%。修改后显式保存。无强制选择的非主要阶段可自动推进，主要阶段仍需显式结束。

Godot 默认数据目录为 `user://alpha-v2-frontend`，Windows 对应 `%APPDATA%/Godot/app_userdata/WizardCard Alpha v2.5.4/alpha-v2-frontend`。设置为 `settings.json`，卡组为 `decks.json`，复盘在 `replays/`。运行时可使用 `-- --user-data <目录>` 隔离数据；用户数据与安装资源分开保存。

当前复盘格式4包含程序版本、规则版本、卡池版本、内容哈希、种子、双方实际主卡组与副卡组、开局选择命令和每一步状态摘要。对局接受操作后自动保存，也可从对局菜单手动保存；完整复盘在终局后开放。旧版复盘因程序、规则、卡池版本或内容哈希不匹配而明确拒绝。旧牌组草稿可保留稳定ID修复；导入导出采用独立的 `wizardcard-deck` 格式1。复盘用于完整重放，不提供中断对局存档恢复。完整离线记录包含双方私有信息，玩家投影与日志按权限过滤；哈希用于复现校验。

## 开发构建与验证

Windows 安装 Visual Studio 2022 C++ Build Tools（含 Windows SDK）、Git 和 CMake 3.28+。依赖从固定 Git 提交获取，首次配置需要网络。现有 `windows` 预设构建原生核心及可选 SFML 开发客户端：

```powershell
cmake --preset windows
cmake --build --preset windows --parallel 4
ctest --preset windows
```

Linux 无窗口核心使用 GCC/Clang、Git、CMake 和 Ninja：

```sh
cmake --preset headless
cmake --build --preset headless --parallel 4
ctest --preset headless
./build/headless/wizard_cli validate assets
```

Godot 开发配置见源码中的 `godot/README.md`。使用固定引擎 `4.7.2.stable.official.ed1daf0bf`、对应 Windows 导出模板和 `godot/bin/wizard_godot_bridge.release.dll`；Debug 与 Release 桥接分别构建。关闭 `WIZARD_BUILD_CLIENT` 时不下载或链接 SFML。`tools/package_godot.py` 根据 `godot/export_presets.cfg` 导出前端，复制正式内容、运行库和文档，并在发布 EXE 启动、CLI 对局与复盘验证后生成 SHA256 清单及 ZIP。

CLI 和网络探针属于开发工具，不随玩家便携包分发。构建后从仓库运行：

```powershell
./build/windows/Release/wizard_cli.exe validate assets
./build/windows/Release/wizard_cli.exe smoke assets verification.json 42
./build/windows/Release/wizard_cli.exe replay assets verification.json
./build/windows/Release/wizard_cli.exe ai-match assets ai.json 42 2
./build/windows/Release/wizard_cli.exe tutorial-smoke assets tutorial.json
```

`smoke` 是固定启发式验证驱动；`ai-match` 的最后一项0/1/2分别对应入门/标准/进阶，对手固定标准。`tutorial-smoke` 使用正常核心完成教学目标。CLI `play`、`script` 属于开发入口，不提供本地换手遮挡。Godot 自动检查覆盖导入、冷启动、桥接、完整复盘、场地、决策托盘、构筑及真实输入；实际截图与双进程联机结果见交付报告。

## 内容与版本控制

`assets/catalog.json` 与分卡 JSON 为正式内容入口，`assets/cards.json` 为一致性聚合。新效果需同步扩展规则实现、内容校验和有效回归；对局创建后冻结内容，重启加载新版本。卡池图鉴由 `tools/generate_card_catalog.py` 生成，包含135种卡牌、完整插图映射和10套合法预设。最新设计快照、来源哈希和可重复导入流程见[迁移报告](assets/design/alpha-v254/card-diff-report.md)。

远程保留 `master` 与 `dev`：`dev` 用于开发验证，确认交付后合入 `master`，见[分支约定](docs/branching.md)。规则变更同步文档、JSON 与回归。自动验收与仍需实机确认的范围以本轮交付报告为准。

字体采用 Noto Sans CJK SC / SIL OFL，许可随运行资源发布；依赖许可位于 `licenses/`，详见[第三方说明](THIRD_PARTY_NOTICES.md)。插画与标识来源记录保留在源码的 `assets/art/`。
