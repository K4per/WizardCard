# WizardCard v1.5 开发者接手文档

更新：2026-10-07。面向组内程序、美术与测试成员。本文件描述当前仓库实现；阶段验收证据见[本地迁移报告](reports/v1.5-local-client.md)。

## 当前可以运行什么

程序标识为 `1.5.0-dev`，规则与卡池仍为 `1.0.0`，30种卡、九套预设、40生命。Godot默认主场景已切换到真实本地客户端，可进入标题、开局、卡组构筑、设置、换手、三档AI和固定教学。SFML Alpha客户端继续保留作行为对照。

本轮使用用户指定的A古籍金饰素材。场地沿用1.0的双方镜像排布，每方包含解析、施法、言灵、行动、灰烬五区；构筑沿用左详情、中主卡组、右卡池三栏，每一份卡牌显示为独立缩略卡。

LAN、重连、新联动卡牌、完整Godot动画、独立导出包尚未交付。当前运行依赖仓库内素材、编译后的扩展DLL和Godot编辑器，不是可分发的便携发行包。阶段2仍需真人输入、IME、DPI、拖动边缘与音频体验验收。

## 环境与第一次启动

Windows x64使用 Visual Studio 2022 的C++桌面开发工具、CMake 3.28或更高、Python 3.9或更高。CI固定CMake 3.31.6；Python用于绑定代码生成与素材检查。首次配置需要获取固定提交的C++依赖。

Godot编辑器必须是 `4.7.2.stable.official.ed1daf0bf`；扩展API为4.7，godot-cpp固定提交见[工具链锁](../godot/toolchain-lock.json)。开发者不要自行更新编辑器、godot-cpp或导出模板中的单独一项。源码技能参考4.6，实际工程与测试以锁定的4.7.2为准。

在仓库根目录执行，编辑器路径改为自己的安装位置：

```powershell
$env:GODOT_BIN = 'E:/Project/Godot/Godot_v4.7.2-stable_win64_console.exe'
cmake --preset godot-debug
cmake --build --preset godot-debug --parallel 4
& $env:GODOT_BIN --headless --path godot --import
& $env:GODOT_BIN --path godot -- --user-data E:/Project/WizardCard/build/dev-data
```

替换示例中的仓库路径。`--user-data`用于隔离开发存档，省略时沿用 `%LOCALAPPDATA%/WizardCard`。`--assets`可显式指定素材根目录；默认读取Godot工程旁的 `assets`。编辑器偏好及 `user://` 也必须可写，导入报错应先解决路径或权限。

Debug和Release各有独立构建目录。`template_debug`与`template_release`决定Godot加载目标，MSVC Debug/Release决定运行库配置，两者是不同概念。扩展DLL位于 `godot/bin`，不提交二进制或 `.godot` 缓存。编辑器通常加载debug DLL；隔离自动探针明确加载被测DLL，release验证不会借用debug DLL。

仅构建引擎与CLI可完全关闭Godot和SFML：

```powershell
cmake -S . -B build/core-only -G "Visual Studio 17 2022" -A x64 -DWIZARD_BUILD_CLIENT=OFF -DWIZARD_BUILD_GODOT=OFF
cmake --build build/core-only --config Release --parallel 4
ctest --test-dir build/core-only -C Release --output-on-failure
```

## 模块与修改入口

| 模块/路径 | 职责 | 接手时的约束 |
|---|---|---|
| `src/core` / `wizard_core` | 状态、合法操作、费用、阶段、连锁、裁剪视图 | 规则修改先写核心测试；不依赖Godot或SFML |
| `src/content` / `wizard_content` | 卡池加载、格式2复盘、严格摘要检查 | 规则/内容版本及哈希不能静默绕过 |
| `src/ai` / `wizard_ai` | 三档AI评分 | 只消费行动者GameView，不读对手手牌或完整状态 |
| `src/application` / `wizard_application` | 对局生命周期、旧设置、卡组、教学与原子保存 | 保留损坏文件、未知卡ID、保存失败恢复 |
| `src/client/interaction.cpp` / `wizard_interaction` | 点选、目标、额外成本、最终确认 | Godot复用选择逻辑；选择过程不提交或支付 |
| `src/client/sound.cpp` | 可见事件音效映射 | 不按中文日志或卡名匹配，不读未过滤结果 |
| `include/wizard/bridge.hpp`、`src/bridge` | LocalSession、白名单DTO、GDExtension绑定 | 新API保持可选构建、精确ID与生命周期校验 |
| `godot/scripts/wizard_client.gd` | 页面导航、本地流程、UI回调 | 不实现规则；旧页面及拖动回调绑定代次/修订 |
| `godot/scripts/ui` | 卡牌、区域、主题、控件、音频 | 全部GDScript变量/参数/返回值显式标注类型 |
| `godot/tests`、`tests/bridge.cpp` | 页面、桥接及复盘验证 | 使用独立用户目录，不污染正式存档 |

正常操作路径：卡牌输入 → `LocalSession::interact`复用C++交互模型 → 待确认 → `confirm_action`复验 → `Application::submit` → 新查看者快照。普通阶段或强制决策也可从当前合法动作ID选择后确认。

所有费用、目标、支付、结算与胜负由C++决定。GDScript只保存只读DTO副本及UI状态；详情字典使用复制或重新赋空对象，禁止对共享定义调用 `clear()`。

## 桥接接口与ID

`WizardBridge`为RefCounted，C++独占会话；当前在Godot主线程同步调用。返回格式统一为 `{ok:true,data:...}` 或 `{ok:false,errorCode:...,error:...}`。

| 接口 | 用途 |
|---|---|
| `initialize` / `start_match` | 内容、用户目录、对局配置；mode为hotseat/ai/tutorial |
| `snapshot(viewer)` | 当前查看者白名单投影，含合法动作、公开/本方事件、暂停与修订 |
| `interact(viewer, request, generation, revision)` | select、activate、pick、pick_link、drop、cancel、state |
| `select_action` / `confirm_action` / `cancel_action` | 合法动作选择、最后确认、取消 |
| `step_ai` / `continue_tutorial` / `set_paused` | AI、讲解确认、本地设置暂停 |
| `library` / `query_cards` | 定义、预设、草稿、设置和C++组合筛选 |
| `create_draft` / `validate_draft` / `save_draft` / `erase_draft` | 构筑草稿及原子保存 |
| `apply_settings` / `save_replay` / `leave_match` / `restart_match` / `release_session` | 持久化与生命周期 |

卡牌定义ID是字符串；实例、链节、决策、代次、修订等ID跨桥接以十进制字符串传递，不能先转浮点。卡牌实例还要检查uint32范围；链节等64位字段用精确解析。动作ID形如 `generation:revision:viewer:index`，离开、重开、释放或修订变化后失效。

AI和教学模式只允许脚本读取玩家0投影；换手读取新查看者前先遮挡手牌、详情和日志。对手埋伏只显示卡背；图形层不得按隐藏卡的ID查全卡池补名称。`read_replay_plan`及`verify_replay`属于离线开发工具，完整复盘含双方私有数据，未来客机会话不得暴露这些接口。

成功提交返回 `replaySaved/saveError`；自动保存失败时命令已经生效，界面只能重试保存，不能再次提交。失败的离开和草稿保存保留原对局/编辑内容。响应中的 `sounds`来自同一查看者提交前后的过滤视图；刷新、暂停恢复和截图不重复消费历史音效。

## 美术与音频工作流

素材入口为 `assets/art/ui/a-gilded-v2/manifest.json`；卡图映射为 `assets/art/runtime.json`。Godot的 `WizardSkin`按素材ID缓存纹理，用中文字体独立绘制文字。运行接入说明见[视觉规范](design/v1.5-ui.md)及[素材证据](reports/evidence/v1.5-local/assets-check.json)。

- 现有Logo、30张插画、原始精绘卡框和稀有度徽章保留原件。精绘卡框保持比例，不能当九切片拉伸；原生面板/控件按manifest切片边距伸缩。
- 解析费用、施法费用、位阶、速度、阵体、环位、容量、收入和资源图标已经接入。新字段先确定语义再映射图标，不能仅凭颜色让玩家判断状态。
- 构筑保持三栏职责及逐份卡牌；合法30张主卡组不分页，卡池九张一页。小窗口使用独立滚动；长效果在详情滚动区完整显示。
- 音频复用 `assets/audio` 的17个MP3及CC0清单，`WizardAudio`缓存解码、最多6路声音、优先级替换、终局清理、单调时钟限频。音量为总音量×音效音量×单音效增益，静音立即清理声音。
- 当前素材从仓库路径加载。未来独立导出必须额外设计打包及路径适配，并检查DLL、资源、字体、音频与许可；不能只导出当前场景后宣称发行完成。

素材静态检查无需额外Python包：

```powershell
python tools/check_godot_assets.py --report build/v15/assets-check.json
```

截图脚本渲染真实页面至独立SubViewport，避免显示器工作区把1600×1000截成1600×900：

```powershell
& $env:GODOT_BIN --path godot --script res://tests/capture_local_client.gd -- --capture 1 --user-data E:/Project/WizardCard/build/capture-data --output E:/Project/WizardCard/build/captures --reference E:/Project/WizardCard/docs/reports/evidence/v1.5/godot-ai_match-replay.json
```

此命令使用开发机GPU，截图不是性能或真人输入验收。参考复盘须保持规则、卡池与内容哈希相符；捕获场面来自真实对局前107条已确认操作。

## 测试与复盘复现

```powershell
cmake --preset godot-release
cmake --build --preset godot-release --parallel 4
ctest --preset godot-release --output-on-failure --parallel 4
ctest --preset godot-debug -R '^godot_(local_ui|bridge_boundary|bridge_alpha_replay)$' --output-on-failure
```

当前Release完整套件208项：191项C++/CLI和17项Godot相关检查。自动探针复制实际页面代码并加载当前目标DLL。`local_ui_smoke`检查逐卡构筑、右键、保存失败恢复、暂停、遮挡、过期回调、音频解码与限声；`local_ui_replay`通过实际页面的选择/确认、AI计时入口和教学按钮完成对局，再逐步比较CLI参考摘要。

三档AI参考分别为简单323步、普通308步、困难318步；换手289步，教学48步。归档证据及SHA256在[清单](reports/evidence/v1.5-local/manifest.json)。格式2兼容名单允许当前程序及同规则 `1.0.0-alpha`；规则、卡池、内容哈希与每步摘要仍严格检查。不要用手工改摘要来修复不一致。

CI文件为 `.github/workflows/godot-bridge.yml`，包含Windows两种Godot目标及Linux PIC构建。当前文档只引用本机通过证据；未推送的新CI不能算远程已验证。

## 后续分工与合入

程序成员先对照Alpha补齐本地输入边缘：失焦/出窗取消、拖动滚动、复杂候选和过期回调；避免在同一轮提前加入LAN或新规则。美术成员对实际截图检查缩略卡可读性、纹理边缘、图标语义和高DPI效果；不重做已有原始插画。测试成员用隔离目录核对IME、窗口/全屏、三种尺寸、暂停、草稿损坏和重试保存，并记录设备、步骤、截图与复盘。

本地功能和真人输入验收通过后，阶段3再增加纯C++网络会话、权威提交、裁剪视图、TCP与重连。首轮LAN保持Alpha规则与卡池；两台实体Windows验收不能由loopback替代。联动新卡由用户定稿，之后按2–4张小批次推进。

继续在 `dev` 开发，保留已有未提交修改。合入 `master` 前附相关测试、素材检查和阶段证据；独立导出、远程CI、两机LAN及性能分别记录通过情况。本轮没有提交、合并或发布发行版本。


## 界面精修交接（2026-10-07）

本轮呈现层修改与[视觉规范](design/v1.5-ui.md)、[运行验证报告](reports/v1.5-ui-polish.md)配套。启动仍为 Godot 打开 `godot/project.godot` 后 F5；已运行的游戏需重新启动以加载脚本。

`WizardSkin` 管理字体档位、抗锯齿、按钮主题和纹理缓存；`WizardWidgets` 提供控件默认字号、间距和操作角色；`WizardCardView` 负责最终像素文字布局、两行省略和完整提示，禁止恢复整卡 `draw_set_transform` 缩小文字。精绘详情框用单独的线性过滤底层，图标仍为nearest。新增控件优先引用现有字体常量和Theme，不另造页面字号。

标题不再添加装饰卡。卡组编辑保持详情/主卡组/卡池三栏，网格5列/3列；小窗口采用纵向滚动，不减少列数或缩小文字。对局布局修改后，应检查带错误提示、确认、目标和终局的720p画面，不能只看空场。施法区固定所需高度，余量让给其他场地区域。

捕获脚本现覆盖23张真实GPU画面（包括三尺寸核心页面、30卡详情分批、选择/确认/遮挡/暂停/终局、实际窗口调整和200%渲染）。保存失败截图采用提示注入检查布局；真实原子保存失败、草稿保留和重试仍由 `local_ui_smoke.gd` 故障注入验证。真实窗口使用显示器工作区允许的1280×720、1440×900，1920×1080和1600×1000证据来自离屏视口；不要把离屏渲染称为物理显示器缩放验收。

新增标题按钮尺寸及无装饰卡断言，页面烟测共96项断言。全套Godot回归17项，覆盖换手、三档AI、教学、旧复盘、边界和逐步摘要。当前证据仅支持本地界面精修，不构成LAN、独立发行包或性能验收。
