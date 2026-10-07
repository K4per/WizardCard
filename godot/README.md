# WizardCard Godot本地客户端

默认入口为真实本地客户端，对局采用 2.5D 古籍金饰牌桌、1.0 双方五区、前景扇形手牌及叠加式详情／连锁／操作界面。支持换手、三档 AI、固定教学和旧用户数据；构筑保持逐卡三栏。C++ 规则／AI／内容保持引擎无关。见[2.5D 报告](../docs/reports/v1.5-25d-client.md)，阶段 2 人工验收仍待完成，LAN 与独立发行在后续阶段。

## 构建

Godot与绑定固定版本见`toolchain-lock.json`。`godot-cpp` 10.x已独立版本化，使用API 4.7，不寻找不存在的4.7分支。编辑器通过`GODOT_BIN`定位，不复制入仓库。桥接默认关闭。

```powershell
$env:GODOT_BIN = 'E:/Project/Godot/Godot_v4.7.2-stable_win64_console.exe'
cmake --preset godot-debug
cmake --build --preset godot-debug --parallel 4
& $env:GODOT_BIN --headless --path godot --import
ctest --preset godot-debug
& $env:GODOT_BIN --path godot -- --user-data E:/Project/WizardCard/build/v15/lab-data
```

仓库缓存中的Python启动器可能失效，本机可用`.cache/tools/cmake/data/bin/{cmake,ctest}.exe`原生程序，Python绑定生成器通过`Python3_EXECUTABLE`明确指定。示例用户目录用于隔离验证；省略`--user-data`时沿用原有WizardCard用户目录。

Release桥接用`godot-release`预设构建，产生不同DLL。Godot编辑器/调试模板加载debug DLL，发行模板加载release DLL；这两个命名表示Godot目标，CMake编译配置另按预设指定。debug与release绝不混用静态库运行时。

自动验证在各自构建目录创建隔离Godot工程，明确加载当前目标DLL，因此release测试不会意外使用debug DLL。CI采用CMake Release分别编译两个Godot目标；本地debug预设保留MSVC Debug运行库。绑定库与桥接均启用C++异常处理，Linux共享库启用PIC；第三方头作为system头引入。

## API

`WizardBridge`是RefCounted，会话由C++独占；主线程同步调用，暂无后台任务或原生信号。释放引用会释放会话，显式`release_session()`也可清理。所有方法返回`{ok, data}`或`{ok:false, errorCode, error}`；跨脚本异常转成错误。

- `initialize(assets, user_directory)`加载内容和合法预设/自建卡组、旧设置；空用户路径采用应用层默认位置。初始化失败不替换当前有效会话。
- `start_match(options)`支持`mode=hotseat|ai|tutorial`、种子、难度0/1/2、双方预设ID；离线验证也可传双方实际牌表。已有对局时拒绝另开。
- `snapshot(viewer)`提供字段白名单投影。AI/教学仅允许viewer=0；换手模式本机可读取两个玩家投影，图形入口在换手时先遮挡。
- `select_action(viewer, action_id, generation, revision)`仅选择当前合法动作；`confirm_action(generation, revision)`才汇入Application::submit。`cancel_action()`不改变规则。
- `set_paused(bool)`保留未确认选择，暂停规则提交；`step_ai(generation, revision)`复用现有AI计划/提交；`continue_tutorial`复用教学正常推进。
- `save_replay()`使用原子保存；操作已接受但自动保存失败时返回`replaySaved=false/saveError`，不回滚或重复提交，允许单独重试保存。
- `restart_match(seed)`仅终局可用；`release_session()`清理内容、应用和选择，代次仍递增，避免初始化后复用旧动作。

动作ID为`generation:revision:viewer:index`，只在对应修订的合法列表中有效，不是跨版本内容ID。卡牌定义ID为原字符串；实例、决策、链节、阶段门、代次和修订统一十进制字符串。快照为复制数据，不持有引擎引用；脚本修改副本不能改写规则。

可见事件ID为该查看者事件流的追加索引，去重键须包含会话代次、查看者、事件ID。换查看者或释放会话清除演出队列。未过滤CommandResult事件、完整GameState、牌库顺序、随机种子和权威摘要不进入普通快照。

`read_replay_plan(path)`和`verify_replay(path)`是明确的**离线开发验证工具**，可读取包含双方隐私的本地记录，不属于联网接口；未来GuestSession不能暴露这两项能力。


本地页面API补充：`library/query_cards/create_draft/validate_draft/save_draft/erase_draft/apply_settings/leave_match`复用应用层与原子保存；`interact`复用纯C++交互模型。成功提交的`sounds`仅由同一查看者的过滤视图生成。细节与示例见开发者文档。

## 验证与边界

`tests/bridge_smoke.gd`逐条匹配当前合法动作、选择、确认，再检查独立记录的每一步摘要与CLI参考相同；同时检查选择无支付、重复确认拒绝和释放后句柄失效。CMake测试自动生成普通、AI、教学三份参考，数据写入独立构建目录。

`ctest -R '^godot_'`现有17项，包括导入、边界、旧Alpha、实际页面冒烟、换手/三档AI/教学及CLI对照。完整Release回归208项（191项C++/CLI、17项Godot），见[本轮报告](../docs/reports/v1.5-local-client.md)。编辑器导入需要正常可写的Godot `user://`/偏好目录；受限环境应先解决目录权限，不应忽略非零退出码。

格式2复盘只接受当前1.5.0-dev和经验证的1.0.0-alpha程序标识；规则/卡池1.0.0、内容哈希及逐步摘要仍须匹配，未知程序或历史不同规则拒绝。

导出模板的版本和下载来源已锁定，独立导出包尚需阶段7验收。Linux PIC已在目标属性设置，但Windows本机不能据此声称Linux桥接运行通过。真实Godot本地页面已实现；真人输入、LAN、联网重连、新卡、完整特效及便携包仍待后续验收。

## 2.5D 场景编辑

打开 `scenes/match/match_view.tscn` 调整对局叠加界面；`match_hud.tscn` 的锚点控制详情、日志、手牌和操作区。`battlefield.tscn` 的 Zones 子节点直接调整双方区域的位置与尺寸，`table_3d.tscn` 调整牌桌，`card_3d.tscn` 调整卡面和状态标记。区域移动只影响展示，不修改 C++ 区域枚举、槽位或规则。场景中保留实际桌面、灯光、摄像机和区域；卡牌实例由运行时真实快照创建。

正式对局会话仍在 WizardClient，通过信号接收场景输入。WizardMatchView 不持有 C++ 会话，不允许演出回调提交规则。MatchPresentation 的 cues 与 sounds 从成功提交结果消费一次。自动对局测试需显式结束表现等待，然后经正常控制入口继续。

新增场景输入测试：`ctest --preset godot-release -R godot_match_scene`。GPU 取图脚本为 `tests/capture_match_25d.gd`，需指定 `--assets`、隔离 `--user-data`、`--capture true`、CLI 生成的 `--reference` 和 `--output`。

## 非建模美术批次

2026-10-07完成A古籍金饰材质、区域、HUD/交互/连锁及15类特效资源，见[资源状态清单](../docs/art-resource-status-v1.5.md)和[画廊](../assets/art/design/nonmodel-v15/gallery.html)。主牌面保留无光照以避免受光过曝，纸边和牌背受光；原卡图/宝石保持原件。独立预览场景为`scenes/art/nonmodel_gallery.tscn`，动作装配样例为`scenes/art/card_motion_sample.tscn`。循环/离场等部分效果提供资源但未全部自动接入对局生命周期；不宣称模型、完整环境或真人输入验收完成。

`ctest --preset godot-release -R godot_art_nonmodel`验证特效加载、暂停、精简、结束、清理和20动作轨道；CMake隔离工程同时复制godot/art中的材质/纹理/着色器。新增组件ID使用`nm_`前缀，不覆盖旧UI资源。当前状态另存于`snapshot/v1.5-25d-art`；ZIP是本地可重建交付物，不提交到源码仓库。
