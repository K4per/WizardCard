# 卡池设计源快照归档

`card-pool-snapshots.zip` 保存 Alpha v2.5.2、2.5.3 和 2.5.4 的原始卡牌来源、旧卡池基线、草稿卡与差异报告。原 `assets/design/` 目录已移除，当前内容导入脚本从该 ZIP 按需解压到系统临时目录。

ZIP 成员路径以版本目录开头，例如 `alpha-v254/sources/requirements.md`。需要恢复仓库内旧布局时，先创建 `assets/design/`，再将 ZIP 内容解压到该目录。此归档不属于运行时资源，不会安装或打入 Godot 发布包。

- SHA-256: `ea7ac772fc554ed608b3d366170c75ee842235ce4147a800afc3c9ffbed9faec`
