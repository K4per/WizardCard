# Godot开发环境

2026-10-07本机检查：编辑器在`E:/Project/Godot/`，控制台程序执行`--version`返回`4.7.2.stable.official.ed1daf0bf`。仓库现有`godot/project.godot`、桥接实验台和可选GDExtension；正式界面与独立导出仍待后续阶段。实施与证据见[阶段0/1报告](reports/v1.5-stage0-1.md)，历史Alpha的SFML发行包保持原样。

## 存放约定

Godot编辑器留在仓库外，现有位置可直接使用。仓库保存`project.godot`、场景、脚本、原始素材、扩展配置、桥接源码和构建脚本；`godot-cpp`用固定提交的依赖方式管理。编辑器EXE、导出模板和生成DLL不作为源码直接提交。

开发者可以自行选择工具目录；构建工具通过`GODOT_BIN`或CMake缓存定位，CI按锁定版本取得编辑器，不能硬编码本机E盘路径。导出模板版本和下载地址已锁定，下载与导出验证待阶段7。目前可在PowerShell执行：

```powershell
$env:GODOT_BIN = 'E:/Project/Godot/Godot_v4.7.2-stable_win64_console.exe'
& $env:GODOT_BIN --version
```

这只设置当前终端环境，不修改全局PATH。`godot/toolchain-lock.json`固定Godot 4.7.2、API 4.7及godot-cpp 10.0.0-stable提交507ed9d840c01a3c5b2a39af8bb4000bfac30bf5，参考[官方扩展示例](https://docs.godotengine.org/en/stable/tutorials/scripting/cpp/gdextension_cpp_example.html)。运行步骤见[桥接说明](../godot/README.md)。独立游戏发行包包含导出后的游戏、扩展及运行资源，玩家不需要安装编辑器。

本轮验证使用`.cache/godot-portable/`中的编辑器副本及`_sc_`标记，把编辑器偏好留在可再生缓存内；未修改原安装目录。该[官方自包含模式](https://docs.godotengine.org/en/latest/classes/class_editorpaths.html)不改变游戏用户数据约定。C++仍使用`%LOCALAPPDATA%/WizardCard`；冒烟通过`--user-data`传入独立构建目录。编辑器自己的`user://`也需可写，首次受限导入曾发生退出异常，正常权限下导入和完整验证通过。

## 缓存与清理

Godot工程生成的`.godot/`已加入仓库忽略规则；场景/脚本/资源的`.uid`文件保留并提交，不能按缓存一并删掉，参见[官方版本控制说明](https://docs.godotengine.org/en/stable/tutorials/best_practices/version_control_systems.html)。未来仅在编辑器和游戏关闭后清理导入缓存，重开时重新导入。

现有C++的`build/`和CPack临时目录可重新生成，清理编译中间文件后下次构建需要重新编译。`.cache/tools/`内的CMake和Ninja是实际构建工具，不整体删除；依赖源目录仍可能被本机CMake配置引用。源码、Git历史、原始素材、用户数据、正式发行ZIP与验收日志/截图/复盘应保留。
