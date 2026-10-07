# v1.5 美术需求核对与非建模交付

日期2026-10-07。依据 [最新Godot清单](art-resource-requirements-godot-v1.5.md)，沿用A古籍金饰、金黄色传说宝石。完成本批非建模资源制作，资源、接入、正式验收分别记录。保留30透明卡图和314张A版PNG；新增91 PNG、41 SVG源稿及引擎配置。

[总索引](../assets/art/README.md) · [画廊](../assets/art/design/nonmodel-v15/gallery.html) · [逐文件清单](../assets/art/design/nonmodel-v15/ASSET_LIST.md) · [ZIP](../assets/art/design/nonmodel-v15/dist/WizardCard-A-v15-nonmodel.zip) · [接入记录](../godot/art-integration.json)。索引分类历史C版、早期190组件与布局稿，不搬迁运行路径。

| 编号 | 已完成资源 | 接入/剩余状态 |
|---|---|---|
| VIS-01 | 当前最终镜头空场/复杂场面样稿，各720p、16:10、1080p | 实机灰模材质版；空场是隐藏卡牌fixture，复杂场面为真实复盘107步。完整环境构图待模型阶段 |
| VIS-02 | 顶视尺寸图、材质/装配拆解图 | 从当前坐标生成，标注选区/环境边界/灯光，不改布局 |
| MAT-01 | 4套2K PBR×4通道：暗木、酒红皮革、古金、石材 | 前三套接桌体/桌面/沿，石材等待环境模型UV |
| MAT-02 | 五区、阵法空槽、宿主关联，7透明纹样 | 五区/空槽接既有平面，宿主关系也有交互框；区域名独立Label3D约14屏幕像素 |
| MAT-03 | 正/背/边材质、纸边512×4通道 | 原卡框/插画合成，纸色填窗、nearest且保比例；受光对比后保留无光照牌面 |
| LGT-01 | Environment.tres、暖主光、冷补光、镜头JSON | Compatibility单主光阴影、补光无阴影，镜头/选区不变；外围仍为原型 |
| UI-01 | 五无字九宫格HUD+SVG | 接阶段/资源/左详情/右连锁日志/独立操作，保留已有前景手牌，未重排页面 |
| UI-02 | 悬停/选中/候选/目标/成本/宿主/符文7状态 | 手牌悬停，公共牌其他状态用形状差分；命中/确认继续走既有权威接口 |
| VFX-01 | 尘点/外围符纹2场景+遮罩/帧表 | 限桌沿、可停止，精简关闭，无全屏密集粒子 |
| VFX-02 | 解析开始/循环/完成3场景+遮罩/帧表 | 完成接Ready cue；开始/循环可在画廊播放，自动生命周期待程序接线 |
| VFX-03 | 准备/释放/反制3场景+遮罩/帧表 | 接Prepare/Release/Cancel；响应仅用宣告样式/文字，不提前播放成功释放 |
| VFX-04 | 伤害/治疗/魔素/荷载4场景+遮罩/帧表 | 数值来自cue；形状及升降/收缩区分，短特效上限12 |
| VFX-05 | 附着/阵毁/普通离场3场景+遮罩/帧表 | 独立资源已验证暂停/结束/清理/精简；缺少明确cue映射，未猜测事件，待接线 |
| ANI-01 | 抽牌/落位/翻面/抬起/回位规范和10正常/精简轨道 | 保留现有移动/翻面/扇形手牌；库只作用Visual子节点，不提交规则 |
| ANI-02 | 开局/胜负平/轻反馈规范，10轨道及3终局纹章 | 库/装配样例可载入，完整消费者待接线；轻镜头反馈默认关闭 |
| UI-03 | 链节进入/优先权/逆序结算/取消4图标 | 接右侧列表及提示；来源/目标/速度可查，长连锁阅读待真人验收 |

“待程序接线”无需新模型，也不等于素材缺失；本轮完成素材、配置和可播放样例，未把尚未接入的对局生命周期虚报成完成。

## 待建模和暂缓

MOD-01主桌倒角/包角/雕花及定制UV；MOD-02周边3～5模块；MOD-03通用薄片倒角厚度；MOD-04牌库堆叠代理；PRP-01外围3种摆件。本批没有.blend/.glb，不以法线贴图冒充模型。P2多主题/角色/长过场/天气等保持暂缓。声音沿用17文件/18事件。

## 验证与实机证据

- [原资源校验](../assets/art/design/nonmodel-v15/existing-assets-validation.json)：314 PNG、30插画、13原件SHA与音效映射通过，AI原件未改。
- [新资源校验](../assets/art/design/nonmodel-v15/validation.json)：91 PNG尺寸/透明/切片/帧表/镜像SHA一致，配置可导入。
- Godot4.7.2 Compatibility：特效/动作smoke **47项通过**；既有场景headless输入smoke **33项通过**，覆盖命中、确认前不支付、取消拖放、暂停、换手清空和匿名牌隐私。退出时偶发2个ObjectDB警告，已定位为MP3流/播放引用；增加停止/释放引用后仍有偶发诊断，待音频生命周期专项定位，不归类为材质或特效通过证据。
- 整个当前项目Release构建成功，隔离工程全量 **211/211回归通过**（含新增美术资源测试）。日志为build/art-snapshot-configure.log、art-snapshot-build.log、art-snapshot-tests.log；当前源文件随 `snapshot/v1.5-25d-art` 保留，非仅提交新增贴图。
- 实机：[1080p复杂场面](../assets/art/design/nonmodel-v15/previews/engine/busy-hud-1920x1080.png)、[720p](../assets/art/design/nonmodel-v15/previews/engine/busy-hud-1280x720.png)、[16:10](../assets/art/design/nonmodel-v15/previews/engine/busy-hud-1600x1000.png)、[空场](../assets/art/design/nonmodel-v15/previews/engine/empty-1920x1080.png)、[正常特效](../assets/art/design/nonmodel-v15/previews/engine/effects-normal.png)/[精简](../assets/art/design/nonmodel-v15/previews/engine/effects-reduced.png)。均实际GPU渲染，特效画廊是资产样例，不冒充真实卡牌结算。
- 桌面GPU窗口模拟拖放两次未通过“drop signal”断言，headless同测试通过；尚未定位为资源或桌面事件因素，不能宣称GPU真人输入已验收。诊断在build/art-nonmodel-smoke*.log。截图/资源验证不替代连续拖放、跨屏DPI或真人操作。
- [GPU静态采样](../assets/art/design/nonmodel-v15/previews/engine/performance.json)仅30帧，不证明整局1080p/60fps。模型、正式环境、长期性能及真人交互验收仍未完成。

独立预览：`godot/scenes/art/nonmodel_gallery.tscn`；动作样例：`godot/scenes/art/card_motion_sample.tscn`。
