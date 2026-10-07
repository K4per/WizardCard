# 巫师牌 / WizardCard

当前开发分支为 **1.5.0-dev**：C++ 内核已接入 Godot **2.5D 对局场景**，采用古籍金饰 3D 牌桌、双方五区、前景扇形手牌及场景内详情／连锁／操作浮层。规则与卡池仍为 1.0.0，保留换手、三档 AI、教学和 Alpha 复盘兼容。见[2.5D 交付与待验收项](docs/reports/v1.5-25d-client.md)、[开发者文档](docs/developer-guide-v1.5.md)和[运行说明](godot/README.md)。阶段 2 真人输入与体验验收仍待完成；下文 Alpha 说明保留为已交付基线。

C++17 专用卡牌规则引擎与 SFML 本地卡牌游戏，支持换手对战、三档AI及固定教学。当前规则见 [五阶段与轻量连锁](docs/alpha-v1.0-rules.md)，继承规则见 [原型规则](docs/design.md)，模块边界见 [架构](docs/architecture.md)。

Alpha v1.0 已交付：程序 `1.0.0-alpha`、规则/卡池 `1.0.0`，30种卡、九套预设与五种代表性方向，初始/最大生命40。下载解压后运行 `wizard_client.exe`；详见[发布说明](docs/releases/alpha-v1.0.md)和[自动验收报告](docs/reports/alpha-v1.0-release.md)。[v1.5计划](docs/plans/v1.5.md)新增Godot图形界面重构、保留C++内核，并继续推进局域网P2P、卡牌联动与动画。规则与内容设计入口：[新卡规则](docs/alpha-v1-cards.md)、[发布补充卡](docs/alpha-release-cards.md)、[设计指南](docs/card-design-guide.md)、[术语规范](docs/terminology.md)。

## 构建

Windows：安装 Visual Studio 2022 C++ Build Tools（含 Windows SDK）、Git、CMake 3.28+。依赖自动从固定 Git 提交获取；首次配置需要网络。不要把项目路径写入全局工具配置。

```powershell
cmake --preset windows
cmake --build --preset windows --parallel 4
ctest --preset windows
cmake --install build/windows --config Release --prefix dist/WizardCard
./dist/WizardCard/wizard_client.exe
```

Linux 无窗口核心（GCC/Clang、Git、CMake、Ninja）：

```sh
cmake --preset headless
cmake --build --preset headless --parallel 4
ctest --preset headless
./build/headless/wizard_cli validate assets
```

关闭 `WIZARD_BUILD_CLIENT` 时完全不下载或链接 SFML。所有构建产物在 `build/`，发布包在 `dist/`。依赖版本与提交在 CMakeLists.txt 中固定，不跟踪上游分支。

## 操作

启动进入覆盖整个窗口的标题画面与五项主菜单。选择“单人测试对战（换手）”，分别选择双方卡组及种子；基础阵法随卡组保存，在编辑器内修改后开始。当前提供九套合法预设，包括快速解析、来源荷载、五芒星塑能、防护续航、言灵节奏五种代表性方向，以及保存的合法自建卡组。标题接入用户指定的WizardCard Logo，缺图回退到文字。

选择“单人AI对战”，配置本方卡组、AI预设（基础阵法随卡组保存），点击难度切换简单、普通、困难，再开始。简单采用基础优先级，普通评估资源与收益，困难额外考虑施法顺序、公开反制风险及触发顺序；三档使用同一套正常规则。AI可选预设带打法说明，玩家可以使用合法自建卡组。单人模式始终显示本方视角，AI手牌及私有日志不向玩家公开；AI行动有短暂提示，设置期间暂停。重开保留难度与双方牌表。

AI子页面中的“固定引导教学”使用固定牌表及种子39，从五阶段、魔素、阵法、符文到解析、准备、被反制、次回合重试和释放逐步引导。按左侧提示操作，点击“继续”确认讲解；错误操作显示提示，重开还原相同开局。完成真实的火球术伤害及荷载清理后显示“教学完成”，随后可继续普通规则练习或从设置返回菜单。教学保留手动阶段推进，避免跳过讲解。

换手时点击确认接手，才显示当前操作者的手牌与日志。底部为独立手牌区，双方场地围绕中央施法区对称显示。点击卡牌在右侧栏查看详情，卡牌上方显示当前可用操作按钮；选择操作后，在场地或手牌中点击高亮目标、额外弃牌成本，最后确认。滚轮浏览手牌、公开区域或卡牌菜单；Esc 清空选择；无选择时打开局内设置；F5 保存完整复盘。每次接受操作自动保存用户数据目录的 `replays/match-<时间戳>-<对局代号>.json`；每局独立文件，终局可重开或返回菜单。

主菜单设置和局内设置共用总音量、音效音量、窗口/无边框/全屏、分辨率及空阶段自动通过。修改后点击“应用并保存”；返回会放弃尚未应用的修改。局内另有继续、保存复盘、投降和返回菜单；离开进行中的对局需要确认，并保存投降后的记录。保存失败保留原对局，可取消或重试。设置期间停止命令提交，恢复时保留决策、尚未确认的卡牌操作及换手遮挡。

对局顶部显示五阶段进度及当前行动方/响应优先权，HUD显示生命和荷载条，解析卡显示进度。出牌、抽牌、附着、准备、释放、响应及反制有移动和光效反馈；实际生命、魔素和荷载变化以飘字显示。动画完成前AI及空阶段自动推进稍作等待，玩家仍可查看卡牌、手动操作和打开设置。右上“动画：完整/精简”可即时切换并保存，也可在两种设置页中修改；精简模式保留短暂数字与状态反馈，关闭飞牌、粒子和循环光效。设置冻结动画，换手或重开清除旧快照。

Windows 用户数据默认在 `%LOCALAPPDATA%/WizardCard`，设置文件为 `settings.json`，用户卡组为 `decks.json`；与安装资源分离。开发验证可用 `--user-data <目录>` 隔离数据。复盘保存不提供中断对局存档恢复。

解析完成后准备施法，结束主要阶段后选择待释放顺序。响应依优先权换手，双方连续放弃才逆序结算；无合法响应自动放弃。左侧显示公开连锁，多个链目标时弹窗选择。银光锐语可从预置或满足反转期限的埋伏状态响应对手准备；所有手牌禁止加入连锁。魔法飞弹可在对方施法阶段的合法时点从就绪状态快速释放；四速后不可追加响应。心如止水选择临时荷载来源，结束阶段超限逐张弃置手牌。

施法阶段可选择“本阶段不再释放”，保留已准备的法术、费用及荷载，后续可释放。言灵可单独设置，零环即时言灵另可选择设置并释放。

默认自动结束无强制选择的非主要阶段；主要阶段始终显式结束。`--manual-phases` 可逐个阶段停留并点击结束按钮；自动推进同样进入复盘。设置页面可切换自动通过；命令行选项在本次进程中强制关闭自动通过。

默认种子42用于方便复现，启动加 `--seed 12345` 可预填普通对局配置种子，也可在换手开局页面修改。普通对局重开生成新种子；教学始终使用39。客户端默认从可执行文件同级 `assets` 读取资源；开发时可以使用 `--assets assets`。

## 卡组构筑

本轮两份内容侧清单的逐项核对与待定决策见[对接报告](docs/reports/content-handoff-review.md)，远程分支约定见[master/dev说明](docs/branching.md)。

全卡效果、费用、插画、使用要点与九套预设牌表见[卡池图鉴](docs/card-catalog.md)，可在[图文浏览版](docs/card-catalog.html)按卡名、类型、稀有度与学派组合筛选。

从主菜单进入卡组列表，可新建空白草稿、从预设复制、点击自建卡组修改或确认删除。编辑器中点击名称输入中文/英文，支持Ctrl+A及Ctrl+V；列表与编辑器均为独立的全窗口页面。编辑器左侧显示详情，中间逐张展示主卡组，右侧为卡池。右键卡池卡牌加入一张，右键主卡组卡牌移除一张；拖动到另一栏同样增减一张。点击查看详情，拖动时按Esc或右键取消。

输入卡名作部分匹配，类型、稀有度、费用、施法费用、位阶和标签可以组合筛选；点击筛选按钮循环值，“重置”恢复全部。卡池按9张分页；合法主卡组的30张卡全部同时可见，超量历史草稿按30张分页修复；长说明可以滚动。基础阵法从具有资格的阵法中选择，不计入30张主卡组。位阶仅筛选法术，施法费用仅筛选解析法术，费用含义按卡牌类型说明。

草稿可随时保存；只有名称、30张、同名最多3张、卡牌存在和基础资格均合法的卡组才进入双方开局选项。点击底部校验信息查看全部问题。离开或关闭窗口时确认未保存修改；保存失败保留编辑内容。内容升级后保留失效ID并显示问题，允许移除修复。损坏或不兼容的用户文件保留原件，暂时禁止写入。

## 无窗口、脚本与复盘

```powershell
./dist/WizardCard/wizard_cli.exe validate assets
./dist/WizardCard/wizard_cli.exe play assets
./dist/WizardCard/wizard_cli.exe smoke assets logs/smoke.json 42
./dist/WizardCard/wizard_cli.exe replay assets logs/smoke.json
./dist/WizardCard/wizard_cli.exe script assets examples/quick-match.json
./dist/WizardCard/wizard_cli.exe ai-match assets logs/ai.json 42 2
./dist/WizardCard/wizard_cli.exe tutorial-smoke assets logs/tutorial.json
```

`play` 为开发控制台，不提供遮挡保证；实际本地双人使用图形客户端。`smoke` 是固定启发式测试驱动，不是对局 AI 或平衡测试。`script` 输入 `{actor, command}` 数组，命令编码见 `encodeCommand`。脚本、客户端和复盘共用 `submit`。

`ai-match` 使用与客户端相同的投影决策模块运行完整对局；最后一个参数0/1/2分别为简单/普通/困难，测试方固定普通。`tutorial-smoke` 经正常核心完成教学目标，保存可验证的教学记录；它不是强制结束对局。两者无需SFML。

复盘格式2包含程序版本、规则版本、卡池版本、内容哈希、种子、双方实际主卡组与基础阵法及每一步状态摘要。版本不兼容或记录被修改会明确报错。哈希用于复现校验，不是安全签名。完整离线复盘包含双方私有信息；面向玩家的 `GameView` 与日志会过滤这些信息。

## 内容扩展

`assets/cards.json` 中参数可调；加载错误会报告文件与字段。新卡必须采用已实现的有限效果。增加新效果时需扩展 EffectKind、解析校验、结算器与回归测试，不能只写卡面文字。对局创建后冻结内容；重启客户端加载新版本。

卡组固定30张，同名最多3张。具有 `baseEligible` 资格的基础阵法与无技能角色不占牌库；目前均衡之六芒星与生命之五芒星有基础资格，选择在卡组编辑页面保存，对战配置直接采用卡组的基础阵法。正式卡池30种，单符文；新卡与旧13张对齐见[卡牌接入规范](docs/alpha-v1-cards.md)，六张补充卡见[发布卡池](docs/alpha-release-cards.md)。

## 验收与版本控制

main 保持可构建，功能分支使用 `codex/`。规则变更同步修改文档、JSON 和回归测试；数值调整独立 `balance:` 提交。CI 在 Windows 测试并打包，在 Linux 构建纯规则核心并重放 Windows 生成的记录。

当前发行版本为 `1.0.0-alpha`，规则/卡池 `1.0.0`；阶段1至7已完成Windows本地自动交付，九套预设覆盖五种代表性方向。用户取消本次20局真人门槛；远程Windows/Linux CI及跨平台复盘已通过，见[同步验收](docs/reports/repository-sync.md)。发行说明与实际限制见[发布文档](docs/releases/alpha-v1.0.md)。历史 Alpha v0.5 标签保留为原测试基线；后续路线见[v1.5计划](docs/plans/v1.5.md)。

字体采用 Noto Sans CJK SC / SIL OFL，许可随资源发布；其他依赖许可证见 `assets/licenses/`。

开发验证还可使用 `wizard_client --ui-smoke <复盘路径>`，通过实际卡牌菜单、目标、成本、换手与确认按钮重放；`--screenshot <PNG路径> --showcase` 用于输出初始界面截图。省略 `--showcase` 时截图为主菜单；`--capture-page settings|setup|ai|decks|editor|tutorial|ai-match` 可检查对应页面。`--menu-smoke --user-data <隔离目录>` 验证菜单和生命周期；`--deck-smoke --user-data <隔离目录>` 验证构筑、草稿和选牌开局；`--ai-smoke --user-data <隔离目录>` 通过实际交互验证三档完整对局、私有视角、设置暂停、重开及教学全流程。测试会写入指定目录，请使用隔离测试目录。

行动卡不设每回合次数上限，仍须满足费用、时机、目标和荷载条件。规则 `1.0.0` 明确拒绝旧版复盘。30张卡牌插画已映射，用户提供的新11张保持原始字节，补充六张由AIGC生成；主要像素UI组件沿用现有资源，图片不可读时使用类别纹章回退；资源来源见 [assets/art/README.md](assets/art/README.md)。

截图时可追加 `--inspect-card fireball` 展开可见卡牌，或 `--ui-smoke <复盘> --capture-step 42 --showcase` 查看指定命令后的场地。

同版本复盘可用`--capture-page ai-replay`按固定本方视角检查动画；录制中的AI命令直接重放，人类命令走实际控件。`--animation-time 0.22`采样本次真实提交的动画，`--animation-frames <目录>`另存20张原生渲染帧；与`--screenshot`及`--capture-step`合用，见[复现说明](docs/reports/match-ui-motion.md)。这些选项仅用于开发捕获，不是对局存档恢复。

## 拖动与整理手牌

- 将行动拖至行动区、言灵拖至言灵区、阵法拖至空闲解析区域；替换阵法时拖到可替换的阵法上。
- 将解析法术拖到宿主阵法，符文拖到宿主；解析完成的法术拖至施法区，或直接拖到合法卡牌目标。
- 强制弃牌和额外弃牌成本可将高亮手牌拖至本方灰烬区。选目标、额外成本与最终确认共用原有流程，松手不会支付资源。
- 在手牌区内拖动可手动换序；左侧“按类型”“按费用”整理当前手牌。顺序按玩家独立保存至本局结束，新抽牌追加在末尾，不改变牌库或复盘。
- 拖至手牌边缘会自动滚动。Esc、右键或鼠标离开窗口可取消拖动；无效落点返回原处。

`--drag-smoke <复盘>` 使用按下/移动/松手入口重放并验证最终状态，包含手牌重排和无效拖放检查。`--showcase --inspect-card spark --preview-drag --screenshot <PNG>` 输出拖动中的预览图。

Alpha v0.5：初始生命20，恢复上限30。解析法术拖到解析区任意位置即可选择操作：只有一个合法阵法时自动匹配，有多个时弹窗选择；最后确认才会支付费用。行动区扩大、灰烬区缩小，双方镜像同步。
