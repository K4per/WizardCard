# Linux 桌面端设计

状态：已确认，2026-11-02。沿用 ADR 0001/0002 的 C++17 权威规则内核、共享会话/网络接口和 Godot 单前端，不改变规则、内容、复盘或联机协议。

## 支持范围

- Ubuntu 24.04+ 和 Arch Linux，均为 x86_64。
- X11 和原生 Wayland；使用项目现有 GL Compatibility 渲染器。
- 官方 Godot `4.7.2.stable.official.ed1daf0bf`、API `4.7` 和 godot-cpp `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5`。
- 本地开发接受同版本、同 commit 的发行版 Godot，例如 `4.7.2.stable.arch_linux.ed1daf0bf`；打包必须使用官方引擎和匹配的官方导出模板。
- 发布版本 `2.6.0-hotfix.1`，正式内容固定为 `assets/release-v260`（92 张卡，mechanicsRevision 10）。

不包含 ARM64、AppImage、deb/rpm/pacman 包、自动更新、规则或协议迁移。Windows 构建、PowerShell 入口和默认 ZIP 打包接口继续兼容。

## 构建和运行库

发行构建唯一基线为 Ubuntu 24.04 x86_64，使用 CMake 3.28+、Ninja、C++17 和 Python 3.12+。Arch 产物用于开发和实机验收，不能标为 Ubuntu 发行产物：新系统生成的 glibc/GLIBCXX 符号可能无法在 Ubuntu 24.04 装载。

Linux Debug/Release 分别使用 `linux-godot-debug` / `linux-godot-release` Ninja 预设及独立构建目录，godot-cpp target 分别为 `template_debug` / `template_release`。`GODOT_BIN` 是机器相关 cache 输入，不写入共享预设。桥接输出固定为：

```text
godot/bin/libwizard_godot_bridge.debug.so
godot/bin/libwizard_godot_bridge.release.so
```

Linux 必须强制 `GODOTCPP_USE_STATIC_CPP=OFF`。审计复现了静态 C++ 运行库桥接在桌面 Godot 的 `fingerprint()` → `std::ostringstream`/locale 路径崩溃；动态运行库对照通过官方 Godot headless 和 X11。发行包依赖系统 `libstdc++.so.6`、`libgcc_s.so.1`、`libc.so.6` 和 `libm.so.6`，不复制这些库或 Windows CRT。

桌面运行还需要 OpenGL 3.3 驱动（Mesa 或厂商驱动）、X11 或 Wayland/xkbcommon、fontconfig，以及可用的 PulseAudio/PipeWire 或 ALSA 音频环境。中文字体随前端发布。Ubuntu CI 安装图形库、Xvfb 和 Weston；`readelf`/`ldd` 必须证明发行可执行文件和桥接无缺失依赖。

## 导出、数据和启动

新增 `Linux Desktop`（Godot 平台名 `Linux/X11`）导出预设，固定 x86_64、GL Compatibility 和外置 PCK；此平台同时支持 `--display-driver x11` 和 `--display-driver wayland`。`.gdextension` 继续使用 `res://bin/libwizard_godot_bridge.release.so`，导出器和便携目录保持相同布局。

开发启动入口 `tools/run_godot_frontend.sh` 接受 `--editor-path`、`--assets`、`--user-data`，校验引擎、冷导入并使用绝对路径启动。发行包启动器从自己的物理目录定位可执行文件，不依赖调用者 cwd。前端既有的同级 `assets` 探测继续使用。

Linux 默认原生持久化目录为 `$XDG_DATA_HOME/WizardCard`；变量缺失或为空时回退到 `$HOME/.local/share/WizardCard`。`--user-data` 覆盖该路径。设置、卡组和复盘使用原生应用层现有原子保存机制。Windows 默认持久化行为保持兼容。

## 便携包契约

唯一发行文件名为 `WizardCard-2.6.0-hotfix.1-linux-x86_64.tar.gz`。目录至少包含：

```text
WizardCard.x86_64
WizardCard.pck
run.sh
bin/libwizard_godot_bridge.release.so
assets/                       # release-v260 内容及音频
licenses/                     # 包含字体、音频和 Godot 等许可证
docs/releases/2.6.0-hotfix.1-linux.md
RELEASE.json
SHA256.json
```

前端字体在 PCK 中，许可证随包保留。CLI 和网络探针用于打包验证，不作为第二套产品 UI。允许离线卡牌目录所需的精确插画子集，禁止源代码、测试/fixture、SFML、Windows DLL/EXE 和 debug 桥接混入 Linux 包。

可执行文件、`run.sh` 和桥接库保留执行权限。打包器检查 ELF 架构及动态依赖，将 `ldd` 证据和系统要求写入 `RELEASE.json`，生成逐文件 SHA256、tar.gz 和归档 SHA256。验证必须解压归档，从包外 cwd 启动真正的发行可执行文件，不能使用源码树的资源或桥接。

## 验收与边界

保留原 271 项核心验收基线；新增 Linux 用户目录测试后数量相应增加。运行内容/模块/前端资源检查、核心 CTest、网络双进程 smoke、CLI smoke/replay、桥接 boundary/完整 replay、frontend/match/deck/network/typography/art residency smoke。

Ubuntu 24.04 CI 构建 Debug/Release、导出并验证归档，Xvfb 验证 X11，Weston headless compositor 验证原生 Wayland。桌面 smoke 记录选中的显示驱动、窗口、中文字体、设置/卡组/复盘保存和输入；真实输入使用 X11 事件注入，Wayland 使用前端输入 smoke。日志、报告和 CTest 失败现场随 CI 上传。

Arch 实机验收覆盖硬件/软件渲染、中文字体、音频、XDG 目录和局域网双进程连接。合成 compositor 验证不替代真实桌面的 IME、多屏缩放、音频听感或物理网络验收；无法执行的检查必须明确记录。
