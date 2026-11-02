# Linux 适配审计

审计日期：2026-11-02。代码基线 `ad817fa`（`dev`），实施前工作树干净。

## 环境和现有架构

审计机器为 Arch Linux x86_64，Linux `7.2.11-zen1-1-zen`，GCC 16.2.1、CMake 4.4.5、Ninja 1.13.2、Python 3.14.8。本地有 X11/Xvfb、原生 Wayland 桌面和 AMD Radeon 780M；官方 Godot 下载到 `build/linux-audit-tools/Godot_v4.7.2-stable_linux.x86_64`，版本为 `4.7.2.stable.official.ed1daf0bf`。

`wizard_core` 是纯标准库 C++17 规则内核；content/codec/replay/ai/application/interaction/bridge_model/network 为明确依赖的原生库。Godot 经可选 `wizard_godot_bridge` GDExtension 读取投影、提交操作；CLI 和网络探针负责诊断与确定性验证。此方案不需要更换架构或增加 SFML。

现有入口包括 `headless`/`windows` CMake 预设、`cmake/GodotBridge.cmake`、`godot/export_presets.cfg`、PowerShell 开发启动器和 Windows 专用 `tools/package_godot.py`。Linux TCP 分支、XDG 原生目录函数和 `.gdextension` 的 Linux `.so` 路径已经存在。

## 基线证据

审计阶段执行并通过：

```sh
cmake --preset headless
cmake --build --preset headless --parallel 4
ctest --preset headless --output-on-failure  # 271/271
python3 tools/catalog_source.py --check
python3 tools/check_module_dependencies.py
python3 tools/prepare_godot_frontend_assets.py --check
```

资源检查计 414 个运行时美术资源、248,982,633 字节；依赖检查为 13 个无环 target，application 与传输解耦。`assets/release-v260` 是正式 92 张卡、mechanicsRevision 10 的产品内容。Linux Debug 桥接成功链接为 `godot/bin/libwizard_godot_bridge.debug.so`。

## 已复现的桥接崩溃

godot-cpp 默认静态 C++ 运行库的 `.so` 在官方/Arch Godot 桌面中初始化内容时崩溃。官方 X11 和 Wayland 日志均有 signal 11，调用链包含：

```text
std::codecvt / std::ostream::_M_insert
wizard::fingerprint
wizard::parseCatalog / loadContent
wizard::bridge::LocalSession::initialize
WizardBridge::initialize
```

证据在忽略的本地目录 `build/linux-audit-bridge/official-x11/start.log`、`official-wayland-baseline/startup.log` 和 `gdb-startup.log`。该静态库的 `readelf -d` 仅列 libm/libc/loader，没有系统 libstdc++。

使用 `GODOTCPP_USE_STATIC_CPP=OFF` 的独立构建做对照，官方 Godot headless 和 X11 正常启动；`dynamic-tests/headless.log` / `dynamic-tests/x11.log` 无脚本/装载错误。对照库依赖系统 `libstdc++.so.6`、`libgcc_s.so.1`、`libm.so.6`、`libc.so.6`。官方前端 smoke 报告 `PASS; checks=2760`，覆盖六种尺寸和 92 张卡。

实施开始时共享 `godot/bin` 中最后留下的是静态审计库；不能把文件存在当作动态策略已落地。正式预设必须重新构建并检查 NEEDED 表。对照结果支持修正 Linux 链接策略，不支持改写 fingerprint 或规则逻辑。

## 平台耦合与发布缺口

| 区域 | 现状 | 需要实施 |
|---|---|---|
| 构建 | 没有 Linux Godot 预设；静态 C++ 默认值可触发崩溃 | 独立 Debug/Release Ninja 预设，Linux 强制动态运行库 |
| 引擎校验 | 只接受官方精确版本 | Linux 开发允许同 commit 的发行版 channel；发布仍要求官方 |
| 持久化 | 原生函数支持 XDG，但前端默认传入 Godot user:// 子目录 | Linux 前端使用原生默认目录，保留显式覆盖和 Windows 行为 |
| 导出 | 只有 Windows Desktop 预设 | Linux/X11 x86_64、外置 PCK，显式 X11/Wayland 验收 |
| 启动 | 仅 PowerShell 开发入口 | Bash 入口、绝对资源路径及包内启动器 |
| 安装 | CPack 硬编码 windows-x64/ZIP；未安装桥接 | Linux 桥接安装目标及 TGZ 平台命名 |
| 打包 | `.exe`/`.dll`、多配置目录、CRT、ZIP 全部 Windows 专用 | 平台规格、tar 权限、ELF 依赖和解压后隔离验收 |
| CI | Linux 只跑核心；Godot 在 Windows | Ubuntu 24.04 官方编辑器/模板、Xvfb、Weston、包与证据上传 |

## 发布政策和风险

发行基线固定 Ubuntu 24.04 x86_64，Arch 只用于开发与实机验收。包依赖系统 glibc、GCC C++ 运行库及桌面图形/音频环境，不捆绑标准运行库。只交付 `WizardCard-2.6.0-hotfix.1-linux-x86_64.tar.gz`；Windows 入口和默认打包行为保留。不增加 ARM64、AppImage、系统包，也不修改内容、规则或联机协议。

重点风险是静态旧库被误打包、Arch ABI 被误标为 Ubuntu 兼容、导出排除项移除必要脚本、PCK 与 `.so` 路径不一致、tar 丢失执行权限，以及只测源码目录而漏掉解压后的 cwd 隔离。实施验收必须对这些路径提供实际运行证据。
