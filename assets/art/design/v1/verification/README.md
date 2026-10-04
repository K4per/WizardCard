# 插画实机抽查

2026-10-04，使用现有 `build/windows/Release/wizard_client.exe`，加载工作区 `assets`：

```text
wizard_client.exe --assets E:\Project\WizardCard\assets --showcase --inspect-card recall --screenshot E:\Project\WizardCard\assets\art\design\v1\verification\runtime_illustrations.png
```

进程退出码 0。截图中可见新插画：火花术、奥术回想、蓄能阵、导流阵、结构瓦解；奥术回想在手牌与右侧详情中同时显示。基础均衡继续使用原有插画。

这是运行映射的实机抽查，不覆盖全部13张图的每种显示状态，也不是新UI皮肤验收。全13张文件本身的解码、尺寸和来源哈希由 `../validation.json` 记录。设计预览与本目录实机截图分别保存。
