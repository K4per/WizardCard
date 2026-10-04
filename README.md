# 巫师牌 / WizardCard

C++17 专用卡牌规则引擎与 SFML 本地双人原型。规则见 [docs/design.md](docs/design.md)，模块边界见 [docs/architecture.md](docs/architecture.md)，里程碑见 [docs/development-plan.md](docs/development-plan.md)。

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

换手时点击确认接手，才显示当前操作者的手牌与日志。底部为独立手牌区，双方场地围绕中央施法区对称显示。点击卡牌在右侧栏查看详情，卡牌上方显示当前可用操作按钮；选择操作后，在场地或手牌中点击高亮目标、额外弃牌成本，最后确认。滚轮浏览手牌、公开区域或卡牌菜单；Esc 清空选择；F5 保存完整复盘。每次接受操作自动保存 `logs/match-<时间戳>.json`；每局独立文件，终局可保存并重开。

解析完成后准备施法，结束主要阶段后选择待施放顺序。对方有预置言灵时自动换手响应；若不使用选择跳过。清心每次选择一个临时荷载来源，最多移除两点。结束阶段手牌超限时逐张选择弃牌。UI 只提供通过核心校验的操作，费用不足等操作不会出现。

默认种子42用于方便复现，启动加 `--seed 12345` 可改变洗牌与先后手。重开会生成新种子。客户端默认从可执行文件同级 `assets` 读取资源；开发时可以使用 `--assets assets`。

## 无窗口、脚本与复盘

```powershell
./dist/WizardCard/wizard_cli.exe validate assets
./dist/WizardCard/wizard_cli.exe play assets
./dist/WizardCard/wizard_cli.exe smoke assets logs/smoke.json 42
./dist/WizardCard/wizard_cli.exe replay assets logs/smoke.json
./dist/WizardCard/wizard_cli.exe script assets examples/quick-match.json
```

`play` 为开发控制台，不提供遮挡保证；实际本地双人使用图形客户端。`smoke` 是固定启发式测试驱动，不是对局 AI 或平衡测试。`script` 输入 `{actor, command}` 数组，命令编码见 `encodeCommand`。脚本、客户端和复盘共用 `submit`。

复盘包含程序版本、规则版本、卡池版本、内容哈希、种子、卡组及每一步状态摘要。版本不兼容或记录被修改会明确报错。哈希用于复现校验，不是安全签名。完整离线复盘包含双方私有信息；面向玩家的 `GameView` 与日志会过滤这些信息。

## 内容扩展

`assets/cards.json` 中参数可调；加载错误会报告文件与字段。新卡必须采用已实现的有限效果。增加新效果时需扩展 EffectKind、解析校验、结算器与回归测试，不能只写卡面文字。对局创建后冻结内容；重启客户端加载新版本。

卡组固定30张，同名最多3张。基础均衡与无技能角色不占牌库。首版只有文档中的13种示例牌、单符文和单层言灵窗口。

## 验收与版本控制

main 保持可构建，功能分支使用 `codex/`。规则变更同步修改文档、JSON 和回归测试；数值调整独立 `balance:` 提交。CI 在 Windows 测试并打包，在 Linux 构建纯规则核心并重放 Windows 生成的记录。

当前版本名为 `0.4.1-dev`，不代表完成全部里程碑验收。真人探索按 [docs/playtesting.md](docs/playtesting.md) 记录；通过自动化测试不等于已完成平衡。尚未完成验收的版本不创建发布标签。

字体采用 Noto Sans CJK SC / SIL OFL，许可随资源发布；其他依赖许可证见 `assets/licenses/`。

开发验证还可使用 `wizard_client --ui-smoke <复盘路径>`，通过实际卡牌菜单、目标、成本、换手与确认按钮重放；`--screenshot <PNG路径> --showcase` 用于输出初始界面截图。省略 `--showcase` 时截图为换手遮挡界面。

行动卡不设每回合次数上限，仍须满足费用、时机、目标和荷载条件。规则版本为 `0.2.2`，旧版本复盘明确拒绝加载。现有四张像素美术已接入（均衡、火球术、银光锐语、奥术圆环），其余卡牌使用类别纹章占位；资源来源见 [assets/art/README.md](assets/art/README.md)。

截图时可追加 `--inspect-card fireball` 展开可见卡牌，或 `--ui-smoke <复盘> --capture-step 42 --showcase` 查看指定命令后的场地。
