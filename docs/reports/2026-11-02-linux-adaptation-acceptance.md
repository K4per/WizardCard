# Linux 桌面端适配验收

日期：2026-11-02。基线：`ad817fa`，分支：`dev`。实施保持为未提交的工作树变更。

结论：六项任务的实现和可自动执行的验收已完成；Ubuntu 24.04 构建的最终便携包已通过 headless、X11 和原生 Wayland 启动。物理音频、输入法、多屏和跨机器局域网仍需人工验收；Hyprland 平铺窗口的模式切换限制见下文，不能将本报告解释为这些项目全部通过。

## 交付范围

| 计划任务 | 交付及核对结果 |
|---|---|
| 1：审计和设计 | 设计、审计文档包含模块边界、271 项原始基线、静态 C++ 桥接崩溃与动态运行库对照、Ubuntu 发布基线和依赖政策。 |
| 2：构建和运行库 | Linux Debug/Release Ninja 预设；强制动态 C++ 运行库；Linux 安装目标和 TGZ 命名；同 commit 开发引擎校验；原生与实际前端 XDG/HOME/显式覆盖回归。 |
| 3：导出和启动 | Linux Desktop x86_64 外置 PCK；可执行 Bash 开发入口；冷导入和仓库外工作目录启动；Windows 预设及 PowerShell 参数保留。 |
| 4：便携包 | Windows 默认平台兼容；Linux 平台规格、ELF 检查、运行库清单、Unix 权限、解压隔离启动、字体/音频/许可证/正式内容、SHA256 和 tar.gz。 |
| 5：CI 和说明 | 新增 Ubuntu 24.04 linux-godot job，官方编辑器/模板 SHA512 校验，Debug/Release、网络、Xvfb 真实键盘、Weston 原生 Wayland、解压产物和证据上传；保留 linux-core 与 Windows jobs。 |
| 6：最终矩阵 | 自动检查、完整原生/Godot CTest、网络/replay、桌面保存和最终归档检查通过；下述人工项目明确保留。 |

规则实现、协议、正式内容及 `godot/toolchain-lock.json` 无变更。前端继续调用原有 C++17 GDExtension，不增加第二套 UI 或平台抽象。

## 最终产物

```text
dist/WizardCard-2.6.0-hotfix.1-linux-x86_64.tar.gz
dist/WizardCard-2.6.0-hotfix.1-linux-x86_64.tar.gz.sha256
dist/WizardCard-2.6.0-hotfix.1-linux-x86_64/RELEASE.json
dist/WizardCard-2.6.0-hotfix.1-linux-x86_64/SHA256.json
```

归档 SHA256：

```text
102d38cc07abb647f28d442e2da59a0c32063f060f1125f4541f939c534a77fc
```

最终归档包含 256 个清单文件和 `SHA256.json` 自身，共 257 个普通文件；逐文件哈希全部匹配。`WizardCard.x86_64`、`run.sh`、`bin/libwizard_godot_bridge.release.so` 均为 `0755`。包含 Godot、godot-cpp、中文字体及音频许可证；正式内容为 92 张卡、mechanicsRevision 10。

发布构建来自 Ubuntu 24.04 x86_64 容器，使用官方 `4.7.2.stable.official.ed1daf0bf` 编辑器和匹配的官方 Linux 模板。桥接使用系统 `libstdc++.so.6`/`libgcc_s.so.1`，最高要求 `GLIBC_2.38`、`GLIBCXX_3.4.29`；Ubuntu 24.04 满足这些符号版本。没有捆绑这些标准库。

归档不含 Windows DLL/EXE、SFML、debug 桥接或源码目录。最终归档是在写完清单后生成并解压验证的同一个文件；解压检查日志放在归档外的 `dist/WizardCard-2.6.0-hotfix.1-linux-x86_64-archive-checks/`，不会再改变被验证的归档。

## 自动验收证据

| 检查 | 结果及证据 |
|---|---|
| 原始 headless 基线 | 271/271；审计文档记录静态桥接失败和动态对照。 |
| 更新后 Arch headless | 272/272；新增 Linux 环境变量回归并恢复环境。 |
| Ubuntu Debug 完整 CTest | 296/296，189.82 秒；`build/linux-audit-tools/ubuntu-debug-ctest.log`。 |
| Ubuntu Release 最终完整 CTest | 296/296，84.33 秒；`build/linux-audit-tools/final-ctest.log`。 |
| Godot 覆盖 | 24 项，含 bridge boundary、完整 replay、frontend、match、deck exchange、network、typography、art residency 和 Linux XDG/HOME/覆盖测试；计入上述完整 CTest。 |
| 打包 Python 回归 | 10/10，Arch 与 Ubuntu 均通过；`build/linux-audit-tools/final-python.log`。 |
| 静态检查 | catalogue、13 target 模块依赖无环、414 个前端资源、Bash 语法、CI YAML 与 10 段 Linux shell 均通过。 |
| 运行库 | 强制传入 `GODOTCPP_USE_STATIC_CPP=ON` 仍落为 OFF；readelf/ldd 确认动态运行库与无缺失依赖；Linux 安装树包含 release 桥接。 |
| 开发版本 | 官方版本及 Arch 同 commit 版本均通过 Linux CMake 校验；非官方发行引擎仍不能打包。 |
| Ubuntu X11 | Xvfb/llvmpipe，29/29；xdotool 真实键盘事件；`build/linux-desktop-final-x11/report.json`。 |
| Ubuntu 原生 Wayland | Weston/llvmpipe，29/29；确认驱动为 Wayland；`build/linux-desktop-final-wayland/report.json`。 |
| Arch 原生 Wayland 软件渲染 | Weston/llvmpipe，收紧断言后 29/29；`build/linux-desktop-final-arch-weston/report.json`。 |
| 桌面行为 | 三种模式的实际 mode 和 borderless flag、非全屏实际 1280×720、中文字体、110% 字体缩放、设置/中文卡组/复盘保存与重读、截图及退出通过；Wayland 键盘是前端事件注入。 |
| Ubuntu 双进程网络 | normal 2241、surrender 6、reconnect 2241 条命令；主客 replay digest 一致且实际发生重连；`build/linux-network-ubuntu/report.json`。 |
| AI 和教程 | AI 1867 条命令，digest `a8f162585c423cb5`；教程 415 步，digest `d09fa9a03d32543a`；由 Arch CLI 重放 Ubuntu 记录均匹配。 |
| 重连跨系统重放 | Arch CLI 重放 Ubuntu reconnect 记录，digest `9ffb9a34686382f4`。 |
| 最终归档 headless | 真正 release 可执行文件和 run.sh 从临时 cwd 启动；分别验证显式资源和自动同级资源查找、隔离数据及保存。 |
| 最终归档 Ubuntu 桌面 | 解压后从 `/tmp` 以 X11 和原生 Wayland 启动；自动同级资源及各自 XDG 路径保存成功；`build/linux-final-extracted-evidence/`。 |
| 最终归档 Arch 桌面 | 同一 Ubuntu 归档，在 Arch AMD Radeon 780M 原生 Wayland GPU 和 Xvfb X11 llvmpipe 下启动；均从 `/tmp` 查找同级资源并保存 XDG 设置。 |
| 工作树检查 | `git diff --check` 通过；规则、协议、内容、Windows PowerShell 及锁文件无变更。 |

GitHub hosted workflow 已更新并完成语法和本地等价步骤验证，本轮没有触发远端 Actions，也没有执行 Windows 实际构建或运行。Windows/Linux 跨平台 replay 的既有 CI job 保留，不能将 Ubuntu/Arch 互放描述为本轮 Windows 实机测试。

## 最终独立审查

进行了一次全变更只读独立审查，无 Critical，发现两项 Important，均已修复：

1. 打包器把 PATH 中的 `godot` 命令解析成项目 `godot/` 目录。新增真实子进程/PATH 回归，先复现 `PermissionError`，再改为 `shutil.which()` 解析后绝对化，10 项打包回归通过。保留显式可执行路径和 Windows PATH 语义。
2. 桌面 smoke 只检查最低尺寸，且未区别窗口/无边框。现在等待并检查 `DisplayServer.window_get_size()` 的实际 1280×720 和 borderless flag，分别记录逻辑视口。受控故障注入（忽略分辨率并关闭 borderless）令新增断言失败，恢复原实现后 X11/Weston 各 29 项通过。原 `FrontendDisplay` 产品逻辑未改。

最终修复后重跑完整 Release CTest、打包回归和桌面检查；没有用第二次审查替代运行证据。

人工查看 X11 与 Arch Weston 的最终截图，中文卡名、搜索栏及列表正常显示；截图和窗口报告均保留在对应桌面证据目录。

## 现有 Python 工具测试问题

全量 `python3 -m unittest discover -s tools -p 'test_*.py' -v` 运行 29 项，结果为 2 failures、6 errors、1 skipped。将同一批旧测试与工具还原到 `ad817fa`、使用相同内容重跑 19 项，得到相同的 2 failures、6 errors、1 skipped；新增 10 项全部通过。原始对照保存在 `.superpowers/sdd/wizardcard-linux-adaptation-plan/python-baseline.log`，最终运行在同目录 `python-final.log`。

这些项目没有在本次 Linux 适配中修改：

- `test_alpha_v254_content.test_complete_source_coverage_and_stable_legacy_identities` 失败；`test_every_new_card_has_structured_capabilities_and_unique_numbers`、`test_explicit_renames_and_source_corrections` 出错。旧测试要求已移除的 qixing 牌和旧卡池。
- `test_alpha_v260_import.test_workbook_detail_and_blocker_categories_are_separate`、`test_workbook_static_statistics_are_explained_as_stale` 出错。依赖仓库外不存在的 `E:/Project/WizardCard-Design/Alpha 现有卡池/法术卡列表.xlsx`。
- `test_card_pack.test_commit_failure_rolls_back_every_file`、`test_real_native_roundtrip_and_explicit_overwrite` 出错；`test_native_rejects_unknown_effect_and_invalid_number_before_writes` 失败。旧 fixture 使用 2.5.0/revision 3，当前校验器只支持 2.5.5/revision 9 和 2.6.0/revision 10，尚未进入其想验证的路径。
- `test_alpha_v260_import.test_cpp_loader_schema_probe_when_release_binary_is_available` 跳过：旧测试固定查找 Windows Release `wizard_cli.exe`。

## 环境限制与待人工验收

- Arch Hyprland 原生 Wayland 的 GPU 启动、字体和数据保存通过；平铺窗口由 compositor 报告为 MAXIMIZED，并拒绝 Godot 4.7.2 取消最大化，完整窗口模式 smoke 因此不能通过。Weston 原生 Wayland 可通过。已核对固定引擎的 `window_try_set_mode` 路径并写入发行说明；没有修改用户 compositor 配置或更换 Godot。
- Xvfb/Weston 会报告缺少部分桌面协议、输入法或 V-Sync 能力；容器无硬件 GPU 时 Mesa 回退到 llvmpipe。日志保留这些诊断，不把软件渲染通过写成 GPU 通过。
- 未人工听音、测试中文 IME、多显示器/分数缩放、物理两机局域网或 Windows 实际导出运行。网络双进程是同机 TCP 测试。
- Arch 只承担开发与实机验证；只有 Ubuntu 24.04 构建的最终归档作为发行候选。

## 实施决策与代价

- 保留现有 `dev` 工作树，不额外创建 worktree；所有变化保持未暂存、未提交、未推送。代价：合并前还需用户按仓库流程提交，Git 历史暂未保存实现，故保留本地 ledger 和验证证据。
- Linux 强制动态 C++ 运行库，并保持既有 `res://bin/libwizard_godot_bridge.{debug,release}.so` 路径和外置 PCK。代价：需要系统运行库；路径不一致会导致装载失败，故用真实导出启动覆盖。
- Linux 前端也修正默认数据目录，而非只增加原生目录测试。代价：Linux 默认保存位置有意切换到约定 XDG 路径；显式覆盖及 Windows 行为保留。
- Godot 导出使用绝对输出路径。代价：命令/文档需要在运行时解析路径；避免 Godot 以项目目录解释相对路径导致导出失败。
- 抽取明确平台规格且默认 Windows；Windows 名称、PowerShell 参数和现有 CI job 保留。代价：接口审查和 Python 契约不能替代 Windows 运行验收。
- Linux 打包器拒绝非 Ubuntu 24.04 x86_64 主机和非官方发行引擎。代价：Arch 开发者需容器或 Ubuntu CI 生成发行包，避免误发过新的 glibc/GLIBCXX ABI。
- 自动矩阵使用 Xvfb/Weston，真实硬件项目单列。代价：不能从 CI 推断所有 compositor、音频、输入法、多屏和物理网络行为。
- Hyprland 平铺状态由 compositor 管理，保留原窗口逻辑并记录浮动窗口/全屏操作说明。代价：Godot 4.7.2 在该平铺状态下不能可靠切换三种模式。
- 不修复与 Linux 适配无关的旧内容/导入工具 fixture；保留基线对照和完整失败名称。代价：全量 Python discovery 仍不是全绿，不能据此声称整个仓库所有测试通过。

独立审查没有遗留 Minor 项。
