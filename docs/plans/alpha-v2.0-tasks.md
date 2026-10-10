# Alpha v2.0 任务与依赖

GitHub父任务：[Alpha v2.0 #2](https://github.com/K4per/WizardCard/issues/2)。子任务已发布，并设置GitHub原生父子和阻塞依赖，以下清单记录任务链接以避免重复创建。标签只使用仓库默认五种分流标签；有未完成依赖的任务使用needs-triage，解除依赖且规格完整后改为ready-for-agent。

| ID | 工作线/任务 | 阶段 | 依赖 | 完成条件 | 初始标签 |
|---|---|---|---|---|---|
| [A01 #3](https://github.com/K4per/WizardCard/issues/3) | 工程：基线、模块/公共头拆分、内容/复盘独立库 | dev.1–2 | 无 | 旧测试通过，逐步摘要/哈希不变，独立构建 | ready-for-agent |
| [A02 #4](https://github.com/K4per/WizardCard/issues/4) | 工程：共同会话接口与本地/网络适配 | dev.2 | [A01 #3](https://github.com/K4per/WizardCard/issues/3) | application不公开Peer，pending/冻结/保存行为一致 | needs-triage |
| [A03 #5](https://github.com/K4per/WizardCard/issues/5) | 工程：分层CI、工具链、Godot桥接恢复 | dev.2–4 | [A01 #3](https://github.com/K4per/WizardCard/issues/3) | 无窗口与桥接分别构建；精确ID/旧回调/回放验证 | needs-triage |
| [R01 #6](https://github.com/K4per/WizardCard/issues/6) | 内容：30卡迁移、配方/等级/类型、40+10预设审定 | dev.1 | 无 | 所有定义与预设审定，特殊能力冲突有明确处理 | ready-for-human |
| [R02 #7](https://github.com/K4per/WizardCard/issues/7) | 规则：40+10、配方、类型/区域/盖伏、过载/枯竭 | dev.3 | [A01 #3](https://github.com/K4per/WizardCard/issues/3) | 使用测试夹具通过规格全部基础场景 | needs-triage |
| [R03 #8](https://github.com/K4per/WizardCard/issues/8) | 规则：全时点、阶段、连锁、持续回位/临时生命 | dev.4 | [R02 #7](https://github.com/K4per/WizardCard/issues/7) | 完整无窗口对局，暂停恢复无重复处理 | needs-triage |
| [R04 #9](https://github.com/K4per/WizardCard/issues/9) | 内容：审定卡池、预设、AI与新版教学 | dev.4–5 | R01,R03 | 正式内容校验；三档AI和教学真实运行 | needs-triage |
| [F01 #10](https://github.com/K4per/WizardCard/issues/10) | 前端：C像素主题、卡牌/环位/决策控件 | dev.2–5 | [A03 #5](https://github.com/K4per/WizardCard/issues/5) | 三种分辨率、长文本和隐藏卡显示 | needs-triage |
| [F02 #11](https://github.com/K4per/WizardCard/issues/11) | 前端：完整本地页面与持久化 | dev.5 | F01,R04,A02 | 换手/构筑/AI/教学/设置/保存失败恢复 | needs-triage |
| [F03 #12](https://github.com/K4per/WizardCard/issues/12) | 前端：Godot房间、联机与故障恢复 | dev.6 | F02,A02 | 双进程正常/投降/重连、复盘一致 | needs-triage |
| [F04 #13](https://github.com/K4per/WizardCard/issues/13) | 前端：事件动画音效、精简模式、性能 | dev.7 | F02,F03 | 不泄露私有信息、不改变摘要、不阻塞收包 | needs-triage |
| [H01 #14](https://github.com/K4per/WizardCard/issues/14) | 交付：真人IME/DPI/声音、两机与虚拟LAN | rc.1 | F03,F04 | 真实设备三场景和输入检查有记录 | ready-for-human |
| [A04 #15](https://github.com/K4per/WizardCard/issues/15) | 工程：独立导出、跨平台复盘、发布证据 | rc.1 | A03,F04,H01 | 独立包与远程CI通过，发布标识可追溯 | needs-triage |

R01与H01是计划明确的人类关卡，不以自动测试替代；R01等待期间R02/R03可用独立夹具推进。所有任务正文引用规则和架构文件，列出本行验收及依赖；依赖和标签更新前查询当前状态；不得重复创建任务。
