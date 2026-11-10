# Alpha v2.6.3 更新需求

更新日期：2026-11-10。本文为需求与验收基线。本次版本已于2026-11-10确认更正为2.6.3；原始现状比较中的2.6.2保留为历史基线。

## 一、目标与资料依据

本次更新包括巫师指南、五章新手教程、内置规则书、对局界面与响应流程、默认联机配置，以及最新卡池和风味文本同步。

| 资料 | 用途 |
| --- | --- |
| [近期.md](近期.md)及其中7张截图 | 教程章节、响应窗口、专注询问、选择窗口与手牌操作 |
| [对局新界面参考.png](对局新界面参考.png) | 新对局界面的区域布局 |
| [WizardCard玩家规则书.md](WizardCard玩家规则书.md) | 内置规则书和教学依据；现有矛盾见第七节 |
| [法术卡列表-72张卡.xlsx](<Alpha 现有卡池/法术卡列表-72张卡.xlsx>)：“卡表” | 法术属性、效果和风味文本 |
| [符文卡列表-共3张卡.xlsx](<Alpha 现有卡池/符文卡列表-共3张卡.xlsx>)：“符文卡” | 符文费用及附着效果 |
| [阵法卡列表-共17张卡.xlsx](<Alpha 现有卡池/阵法卡列表-共17张卡.xlsx>)：“阵法卡” | 最新阵法名单、数值、配方和效果 |

## 二、巫师指南与教学内容

### 2.1 一级入口（GUIDE-01）

1. 主菜单新增“巫师指南”，替换“卡牌图鉴”入口；`近期.md`中的“指南”统一使用此名称。
2. 内部分为“新手教程”“规则书”“卡牌图鉴”三个分页。
3. 移除单人游戏中的教程选项，统一从巫师指南进入。
4. 图鉴保留现有查阅能力，与对局、构筑使用同一版本卡池。

**验收：**三个分页均可打开、返回；单人游戏无重复教程入口；迁移后图鉴详情仍可正常查看。

### 2.2 五章可操作教程（GUIDE-02）

前提：先完成阵法卡更新，再制作教学关。教程使用新版配方和真实对局操作，不能仅靠点击“下一步”替代玩家应完成的动作。

| 章节 | 教学流程 | 完成判定 |
| --- | --- | --- |
| 第一章：状态栏与区域 | 认识生命、临时生命、魔素、荷载/上限；定位手牌、牌库、副卡组、阵法槽、解析区、施法区、灰烬区；辨认阶段和回合玩家 | 按引导定位区域，完成资源与区域的交互教学 |
| 第二章：设置阵法 | 从副卡组选择阵法，查看等级、阵体、环位、收入、荷载上限和配方；区分初始阵法与对局中付费设置 | 完成合法设置，副卡组、材料、魔素与场地正确变化；不把初始免费设置理解为所有设置均免费 |
| 第三章：解析法术——火球术 | 选位设置→等待解析完成→准备施法→在合法阶段选择目标发动→结算 | 完整操作并造成7点火焰伤害；未解析完成不能准备，设置/发动费用分别正确支付 |
| 第四章：言灵法术——火花术 | 选择合法环位，设置后立即自动准备并发动，选择目标 | 造成3点火焰伤害，无额外等待解析或手动准备步骤 |
| 第五章：咒符法术——护盾术响应魔法飞弹 | 先盖伏并经过合法等待时段；对方发动魔法飞弹，玩家通过响应流程反转护盾术 | 护盾术先结算；魔法飞弹单发4点力场伤害被6点减伤降至0，不产生负伤害或回血 |

教学用卡按当前表格取值：

| 卡名 | 类型 | 阶级 | 设置/发动费用 | 解析时间 | 速度 |
| --- | --- | ---: | --- | ---: | ---: |
| 火球术 | 解析 | 3 | 3 / 1 | 1 | 1 |
| 火花术 | 言灵 | 1 | 2 / 0 | 0 | 2 |
| 护盾术 | 咒符 | 4 | 0 / 3 | 0 | 2 |
| 魔法飞弹 | 解析 | 2 | 2 / 2 | 1 | 2 |

补充要求与验收：

- 每章有当前目标、操作提示、完成反馈、重试和返回章节列表；错误点击不推进步骤，也不造成死锁。
- 固定初始局面须提供足够的资源、合法阵法及空余容量。第三章不能只提供等级1阵法却要求设置阶级3火球术。
- 第五章需满足护盾术反转条件及3点发动费用。护盾术效果为本回合每次伤害-3、力场伤害改为-6，并非无效魔法飞弹整张卡。
- 魔法飞弹还有“之后可以销毁对方解析区1张盖伏卡”，应单独处理该可选效果，不能因伤害为0就假定后续效果消失。
- 教程提示区分“准备施法”动作与“准备阶段”；费用、速度、合法阶段和连锁逆序结算均与实际规则一致。
- 章节是否顺序解锁、进度是否保存和具体初始牌表待确认。

### 2.3 内置规则书与图鉴（GUIDE-03）

1. 规则书支持分章节阅读和滚动，标明版本；规则书、图鉴及教程均可离线打开。
2. 内置规则书采用本目录玩家规则书，但须先解决第七节的矛盾，补全“略”等未完成内容。
3. 图鉴和对局详情完整显示属性、效果、阵法配方与风味文本。效果和风味文本分区，避免混淆。

**验收：**不依赖外部源文件或网络访问；长文本完整可读；教程、规则书、图鉴及实际结算一致。本次需求整理不直接修改原规则书文件。

## 三、对局界面与操作

### 3.1 新场景（UI-01）

以[对局新界面参考.png](对局新界面参考.png)为布局依据；`近期.md`中的外部游戏截图仅用作交互参考，仍使用 WizardCard 的术语与素材。

- 左侧卡牌详情，下方为连锁信息和己方状态；对方状态位于右上区域。
- 中央区分双方场地，明确阵法、解析区、施法区及所属玩家；牌库、副卡组、灰烬区按参考布局放置，切换阶段按钮位于右侧。
- 状态栏清晰展示生命、临时生命、魔素、荷载/上限；可辨认当前阶段、回合玩家和等待哪一方操作。
- 连锁区显示C1、C2等发动顺序及当前处理链节；关键操作不能被详情和连锁展示遮挡。
- 手牌保留独立可点击区域。参考图未标注的手牌区、中央横带用途和施法槽布局待确认。

**验收：**双方视角区域可辨，满场、多手牌时无关键操作遮挡或点击穿透；项目支持的窗口尺寸下可完整查看主要信息。支持分辨率须在视觉验收前确定。

### 3.2 响应窗口与直接选卡（UI-02）

窗口写明具体时点，例如“对方发动【魔法飞弹】”“即将结束主要阶段”“对方设置了1张盖伏卡”，再询问是否响应。隐藏卡信息只显示当前玩家有权知道的内容。

| 情况 | 操作 | 结果 |
| --- | --- | --- |
| 有合法响应 | 显示时点与“是否进行连锁？”，提供“是/否” | “是”收起询问进入选卡；“否”放弃当前响应机会 |
| 全时点模式、无合法响应 | 显示时点与“此时没有可以发动的效果，是否查看场上情况？”，提供“是/否” | “是”收起窗口查看场地；“否”略过当前时点 |
| 已收起询问进行查看/选卡 | 提供“取消操作” | 未提交动作时回到同一时点的询问，不等于放弃响应 |
| 场上或手牌有合法响应卡 | 直接高亮卡牌，点击进入合法操作 | 不再弹出汇总场上与手牌响应卡的选择窗口 |
| 涉及牌库、副卡组、灰烬区选择 | 保留卡牌选择窗口，标明区域、数量和条件 | 仅显示依法可见的候选，不开放隐藏牌库 |

补充要求：

1. 高亮依据当前合法动作，考虑速度、优先权、费用、目标、盖伏等待期限及容量；仅可查看的卡不能误作可发动卡。
2. 同卡多个合法操作可显示操作选项；取消不能重复扣费、生成链节或跳过时点。
3. 时点结束即清除旧询问与高亮，迟到点击不能提交到下一时点。
4. 全时点模式无可用卡仍可查看；可用时点模式不为无合法响应的时点反复弹窗。默认模式、“忽略时点”范围待确认。
5. 保留阵法材料选择、【克敌机先】手牌费用等其他选择流程，本次不将其一并移除。

**验收：**覆盖有/无响应、是/否/取消、场上/手牌/灰烬区候选；收起后可查看，取消后返回原询问；连续不同事件的提示不混淆。

### 3.3 强制专注操作（UI-03）

1. 准备阶段有待处理专注时，提示“此时有必须进行的操作，请完成。”，仅提供“是”。
2. 点击后收起窗口，禁用切换阶段按钮，高亮当前待处理的专注卡；点击卡牌选择维持或放弃。
3. 魔素不足或规则禁止维持时，不允许提交维持，显示原因并允许完成合法处理。
4. 所有待处理操作完成后才能推进；关闭详情或取消浏览不跳过强制选择。

**验收：**足够魔素时仅扣费一次；不足时能放弃；【扰神曲】禁止维持时正确处理；未处理完毕时按钮和快捷方式均不能切换阶段。

### 3.4 详情与选择窗口（UI-04）

1. 修复选择窗口内悬停无法查看详情的问题，覆盖材料、费用、检索、灰烬区等选择。
2. 对方无关操作、日志或普通同步不能关闭详情/选择窗，不能清空仍合法的选择或重置浏览位置。
3. 候选失效、目标离场或时点结束时须刷新并提示，不能继续提交过期动作；保留的离场卡详情须标明状态。
4. 悬停与点击选择区分；不泄露对方手牌、盖伏卡和无权查看的牌库内容。

**验收：**窗口内可看完整详情；对方无关动作不打断；候选真正失效时更新并阻止提交。

### 3.5 设置、盖伏与手牌选中（UI-05）

1. 选择手牌法术及设置/盖伏动作后，先高亮合法阵法和环位供选位；选定位置并完成必要选择后才提交。
2. 不自动放入默认位置；非法位置不能提交，无位置时说明原因。
3. 取消未提交操作不移动卡、不扣费、不增加荷载或占用环位。
4. 选中的手牌保持突出，不随其他手牌收缩；取消、改选或成功后解除固定。
5. 设置与盖伏分别按规则校验，不把表侧设置的阶级限制套到盖伏；响应期间的手牌设置同样经过选位。

**验收：**覆盖多个合法位置、无位置、等级不足、取消重选及联机状态变化；落位与点击一致，动作及费用只结算一次。

### 3.6 施法槽（UI-06）

`近期.md`仅有“施法槽”标题，以下为现有规则可支持的要求，未明确细节仍待确认：

- 展示已占用与可用施法槽；默认槽位数按己方场上环位总数计算，并反映适用效果修正。
- 明确区分解析环位与施法槽；准备施法前能判断容量是否允许。
- 【土石喷发】施法槽减半，以及阵法、符文引起的容量变化，界面和动作合法性一致。
- 减半取整、容量下降后已有占位超限的处理、是否手动选择施法槽待确认。

**验收：**基础及修正容量正确，无容量时不出现幽灵卡或重叠落位；待确认规则落实后补齐相应预期结果。

## 四、默认联机配置（NET-01）

1. 内置默认服务器域名、端口和客户端所需受信任证书材料，普通玩家无需输入地址、选择证书或传入启动参数。
2. 保留服务器身份及证书校验；客户端不打包服务器私钥。
3. 失败时提供原因及重试，至少区分服务器不可达和证书校验失败。
4. 正式域名、端口、证书来源和更新方式待确认；是否保留高级手动配置入口待确认，不以临时测试地址代替正式配置。

**验收：**干净用户环境使用发布包直接连接指定服务器，双端完成对局；错误或过期证书不静默放行，失败后可重试。此项不扩展为新增服务器架构需求。

## 五、卡池同步

### 5.1 同步口径（CARD-01）

1. 逐行核对卡名、类别、派系、阶级/等级、费用、解析时间、速度、阵体、环位、收入、荷载上限、配方、效果和风味文本。
2. 保持既有卡牌ID稳定，不按重启或跳号的表内序号重新覆盖ID。卡图按身份匹配，避免阵法删行后错位。
3. 图鉴、构筑、教程、对局详情和实际结算必须一致，不能只改显示文本而不核对行为。
4. 当前表未列出的旧卡单列差异；退池确认前不自动删除、不静默改写旧卡组。确认后同步默认卡组、副卡组及教程引用。
5. 导入记录有效卡数、字段缺失和差异，不从“过时”目录补齐文件名张数。

### 5.2 风味文本和空字段（CARD-02）

- 72张法术均有风味文本，完整同步并保留原文；不自行润色卡牌文案。
- 当前3张符文、15张阵法风味文本为空，三份表稀有度列均为空。空白不等于“普通”或“无效果”，不擅自生成。
- 稀有度空白是否保留现有值待确认；空风味文本不显示“None/null”等技术占位词。
- 数值0必须保留。【法术反制】发动费用为X，显示X及计算说明，实际费用按被连锁法术的实际设置与发动费用之和计算，不能将X误读为免费。

**验收：**72条风味文本逐条匹配，长文本可完整阅读；空字段无技术占位词；法术反制显示与实际扣费分别检查。

### 5.3 阵法目标清单（CARD-03）

下表来自当前阵法表15条有效记录。配方材料的具体处理沿用已确认规则，歧义另列。

| 卡名 | 等级 | 阵体 | 环位 | 荷载上限 | 收入 | 配方 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 正十字 | 3 | 2 | 2 | 5 | 2 | 解析区死灵派系法术×1＋魔素×1 |
| 逆十字 | 3 | 2 | 2 | 5 | 2 | 解析区死灵派系法术×1＋魔素×1 |
| 安卡十字 | 3 | 2 | 1 | 5 | 1 | 解析区死灵派系法术×1 |
| 恩格亚斯之星 | 2 | 1 | 1 | 2 | 2 | 解析完成的法术×1＋魔素×3 |
| 元素之环 | 3 | 2 | 2 | 8 | 1 | 解析完成的塑能派系法术×1＋魔素×1 |
| 混沌之环 | 3 | 2 | 5 | 12 | 1 | 解析完成的不同派系法术×2 |
| 阿克乌姆之弧 | 0 | 1 | 0 | 15 | 0 | 手卡×2 |
| 硫磺 | 6 | 2 | 6 | 6 | 3 | 解析完成的火焰伤害法术×2 |
| 基础法阵 | 1 | 1 | 1 | 5 | 1 | 手卡×1 |
| 标准法阵 | 2 | 2 | 2 | 8 | 2 | 基础法阵＋魔素×2 |
| 强化法阵 | 3 | 2 | 3 | 10 | 3 | 标准法阵＋盖伏卡×1 |
| 复合法阵 | 4 | 2 | 3 | 10 | 4 | 强化法阵＋基础法阵 |
| 增幅法阵 | 5 | 3 | 5 | 15 | 5 | 复合法阵＋解析完成的法术×1＋魔素×2 |
| 高阶法阵 | 6 | 3 | 6 | 18 | 6 | 增幅法阵＋解析完成的法术×1＋魔素×4 |
| 完美法阵 | 7 | 4 | 7 | 21 | 7 | 高阶法阵＋解析完成的法术×2＋魔素×6 |

| 有效果阵法 | 效果与重点验收 |
| --- | --- |
| 正十字 | 己方结束阶段开始时恢复1点生命，不能在其他时点额外触发 |
| 逆十字 | 己方结束阶段开始时失去1点生命、获得3点临时生命，分别处理两类生命 |
| 安卡十字 | 离场时可发动，将自身抹除，之后可将己方灰烬区1张阵法返回副卡组；核对自身状态与返回候选，不能返回手牌 |
| 恩格亚斯之星 | 1回合1次，其上设置的解析法术解析时间-1；核对次数限制，具体作用时机待确认 |
| 元素之环 | 其上设置的塑能法术伤害+1；核对进入施法区后的来源关联与多段伤害 |
| 混沌之环 | 不可同时设置相同派系法术；同派系已有卡时正确限制选位，盖伏是否受限待确认 |
| 阿克乌姆之弧 | 己方场上阵法被销毁时，可将此阵法送去灰烬区替代；不能简化成无条件免疫销毁 |
| 硫磺 | 仅可设置火焰伤害法术；己方每有1张法术从施法区进入灰烬区，对对方造成2点火焰伤害；覆盖多张离场，非施法区离场不误触发 |

其余七张法阵均为“无效果”。初始阵法按等级1条件筛选：当前表中基础法阵符合，等级0的阿克乌姆之弧不能仅因等级低就入选。

#### 相对现有发布数据的变化

对比对象为核对当日仓库 `assets/release-v260/catalog.json` 及其逐卡定义（92张，元数据标记2.6.2）。以下为静态数据差异，不代表运行行为已验证。

| 项目 | 现有发布数据 | 当前表格 |
| --- | --- | --- |
| 强化法阵 | 标准法阵＋解析区法术×1 | 标准法阵＋盖伏卡×1 |
| 复合法阵 | 基础法阵＋标准法阵 | 强化法阵＋基础法阵 |
| 增幅法阵 | 复合法阵＋解析区塑能/咒法法术×1 | 复合法阵＋解析完成的任意派系法术×1＋魔素×2 |
| 高阶法阵 | 增幅/守护/幻象三选一＋解析完成法术×1＋魔素×3 | 增幅法阵＋解析完成法术×1＋魔素×4 |
| 完美法阵 | 高阶法阵＋解析完成法术×2＋魔素×5 | 高阶法阵＋解析完成法术×2＋魔素×6 |
| 守护法阵、幻象法阵 | 发布数据中存在 | 当前表未列出，去留待确认 |

**验收：**正确材料可设置，旧配方被拒绝；覆盖魔素不足、未解析完成、阵体空间不足、取消不消耗材料。不能只改配方文字而仍接受旧条件。

### 5.4 符文核对（CARD-04）

| 卡名 | 附着费用 | 附着于阵法 | 附着于法术 |
| --- | ---: | --- | --- |
| 逆三角星符 | 3 | 魔素收入+1 | 无效果 |
| 奥术圆环 | 2 | 环位+2 | 发动时获得其实际设置费用的魔素 |
| 昂扬 | 2 | 阵法等级+1、环位+1、魔素收入-1 | 发动费用+1，额外造成5点力场伤害 |

**验收：**分别验证两种宿主；奥术圆环取实际设置费用；昂扬的额外力场伤害不替换原伤害。

### 5.5 法术交互专项核对（CARD-05）

72张法术均需字段核对，以下为与本次需求直接相关的重点，不替代其余卡牌检查：

| 卡牌 | 验收重点 |
| --- | --- |
| 火球术、火花术、护盾术、魔法飞弹 | 教程、选位、响应与逆序结算 |
| 克敌机先 | 保留手牌费用选择，再从牌库选择同派系法术，两步不混淆 |
| 碎环术、滑稽魔盒、安卡十字 | 副卡组/灰烬区候选、约束与悬停详情 |
| 炽燃结界、匕首之云、避火咒、扰神曲 | 专注询问、费用、禁止维持与放弃 |
| 油腻术、土石喷发 | 环位、施法槽与合法位置变化 |
| 解除魔法、银光锐语、法术反制 | 分别对应设置、准备施法、发动时点，提示不能混用 |
| 步履维艰 | 新表写“此回合，对方设置的全部解析法术卡的解析时间+1”；发布文本另有限定“此卡发动后”“从手卡设置”，范围差异待确认 |

## 六、依赖与发布验收

建议顺序：确认卡池差异及规则歧义→同步卡牌数据与规则行为→更新对局交互和场景→基于最终流程制作教程及规则书→发布包双端联机验收。默认联机配置可独立准备。

| 编号 | 场景 | 通过条件 |
| --- | --- | --- |
| A01 | 巫师指南 | 三分页可达，无旧教程入口，离线可用 |
| A02 | 五章教程 | 可完成、重试、退出，真实操作推进，数据正确 |
| A03 | 响应时点 | 事件描述及是/否/取消正确，不重复扣费或串时点 |
| A04 | 响应来源 | 场上/手牌直接高亮，其他区域按需选择，不泄露信息 |
| A05 | 强制专注 | 未处理不能切阶段；可维持、不可支付、禁止维持均能正确完成 |
| A06 | 详情窗口 | 可悬停，对方无关动作不打断，失效候选不能提交 |
| A07 | 设置与盖伏 | 先选位，取消不扣费，选中手牌不收缩，双端一致 |
| A08 | 新场景与施法槽 | 区域可辨、无关键遮挡，容量与规则一致 |
| A09 | 默认联机 | 干净环境免手填连接，证书校验正确，双端完成对局 |
| A10 | 卡池数据 | 有效行及字段匹配，72条风味文本完整，ID与卡图不串位 |
| A11 | 阵法与符文 | 全部目标卡核对，五条进阶配方通过合法及非法用例 |
| A12 | 兼容性 | 默认卡组和教程引用有效，缺失卡按确认的退池方案处理 |

逐项记录实际结果与证据，未通过或待确认项不能勾为完成。规则变动需验证本地、联机结果一致，并保持规则内核权威判定及确定性复盘；UI不另建一套规则。

## 七、待确认事项

| 编号 | 问题 | 影响 |
| --- | --- | --- |
| Q01 | 15张阵法是否代表守护法阵、幻象法阵正式退池，还是漏项；旧卡组如何迁移 | 最终90/92张口径、构筑及默认卡组；不自动补回或删除 |
| Q02 | 步履维艰是否作用于发动前已设置、非手牌来源设置的解析法术 | 文本与结算范围 |
| Q03 | 配方“盖伏卡”允许的类型、特殊来源及控制权要求 | 强化法阵材料候选 |
| Q04 | 施法槽减半取整、容量下降后超限占位处理、是否手选施法槽 | 土石喷发和施法区交互 |
| Q05 | 恩格亚斯之星主动还是自动触发；混沌之环/硫磺对盖伏、反转、自选伤害法术的限制 | 阵法效果及合法选位 |
| Q06 | 正式域名、端口、证书来源、更新方式和高级配置入口 | 联机发布验收 |
| Q07 | 教程解锁、进度保存、初始局面；新场景未标注区域布局和支持分辨率 | 教程与视觉验收 |
| Q08 | 响应模式默认值及“忽略时点”的范围 | 避免漏响应；强制操作不能被忽略 |
| Q09 | 表中空白稀有度是否保留现有值 | 卡面和图鉴显示 |

规则书还需在内置发布前校对以下问题，不视为本次已经确认的新规则：

- 解析区总述要求阵法等级不低于法术阶级，后文又明确盖伏不受限制，需区分设置与盖伏。
- 结算后“留在施法区或回到施法区”疑似笔误，需明确持续法术回位区域。
- “超过上限”和“达到上限”两种过载阈值表述需统一，明确恰好等于上限时的处理。
- 规则书写计时在准备阶段减少，项目术语表写结束阶段递减，需确认本版权威时序。
- “与卡组交互后自动洗切”与异界探知、卜筮术将其余卡放回牌库底部的处理，需明确例外及顺序。
- 主要、施法阶段仍为“略”，须补齐合法动作及阶段限制再发布完整规则书。

确认结果应回填对应条目及验收条件，不由实现方将未决问题默认为已授权的玩法变更。


## 八、2026-11-10确认结果与执行状态

以下裁定取代上文对应待确认表述。验收状态：实施中，尚未计为完成。

## 已确认裁定

```json
{
  "version": "2.6.3",
  "cards": 90,
  "retire": [
    "v260-formation-0014",
    "v260-formation-0015"
  ],
  "oldDeckMigration": "backup then remove retired cards; require player to fill gaps; never auto-select replacements",
  "setupDelay": {
    "timing": "only subsequent settings until end of current turn",
    "source": "all sources; exclude relocation, return-to-ring, control transfer"
  },
  "recipeFaceDown": "any currently controlled own analysis-zone face-down spell including analytic, word, talisman, special source, borrowed ownership, no flip-wait requirement",
  "castingSlots": {
    "rounding": "floor",
    "overflow": "controller chooses exact excess cards to ash after whole current chain resolves based on final capacity; block actions/progression until complete",
    "placement": "automatic",
    "stacking": "each active effect halves sequentially with floor after each; independent expiration"
  },
  "engeas": "first face-up setting or flip of analytic each global turn automatically reduces remaining analysis by1 clamped at0 and consumes once-per-formation opportunity; remove legacy extra load",
  "hostRestrictions": "face-down bypasses; check face-up setting/flip; chaos counts other face-up cards only",
  "fireEligibility": "must include fixed fire damage in card effects; selectable damage not sufficient; same predicate for sulfur hosting and recipe",
  "server": {
    "host": "aq.rainplay.cn",
    "port": 23513,
    "deploy": "upgrade same production server, backup and no live matches before switch, public two-client acceptance",
    "trust": "embed existing public cert; updates in release; hostname/expiry/chain checks; no private key",
    "advanced": "collapsed custom host/port/certificate plus reset to official defaults"
  },
  "tutorial": {
    "access": "all five available",
    "progress": "local chapter completion only; replay from chapter start; no midchapter save",
    "initial": "purpose-built fixed scenarios, sufficient resources, legal formations; chapter2 separates free initial selection and paid recipe; use real authoritative rules"
  },
  "layout": {
    "center": "two casting strips, one per player",
    "hand": "own hand independent bottom; opponent backs/count top",
    "baseline": "1920x1080",
    "minimum": "1280x720",
    "acceptance": [
      "1280x720",
      "1600x900",
      "1920x1080"
    ],
    "other": "scale to higher resolutions; retain key info in non16:9"
  },
  "response": {
    "default": "AUTO renamed 可用时点",
    "ignore": "until player manually changes it within current match; persistent visible state; never skip mandatory decisions"
  },
  "rarity": "preserve existing values for blank sheet cells",
  "rank": "face-up setting and flip require host level >= spell rank; ambush placement bypasses",
  "rules": {
    "rank": "face-up setting and flip require host level >= spell rank; ambush placement bypasses",
    "persistent": "after chain persistent spells return to a legal analysis ring; no legal ring => ash; ordinary resolved spells remain casting until end cleanup",
    "overload": "strictly greater than cap; 2x newly excessive load; equality no damage",
    "timers": "after successful activation, decrement once at owner's End after phase triggers, not opponent End",
    "deck": "draw/inspect no shuffle; search/shuffle-in per rules; explicit top/bottom overrides shuffle; keep original relative order for remaining Otherworldly Perception/Augury cards",
    "mainCastDocs": "complete legal action and speed/response restrictions from kernel and confirmed decisions",
    "baseLoadCapacity": 2,
    "opening": "RPS winner goes first; ties repeat; simultaneous hidden submission until both confirmed; then shuffle and draw5 each; then private initial formation selections revealed together; all normal AI/hotseat/central modes; remove normal-room fixed-first override; tutorial chapter2 includes true opening",
    "recipeReady": "retain current Ready or Active eligibility for completed-analysis recipe material; do not conflate with analysis-time modification exclusions",
    "rpsClock": "No match clock deduction during any RPS round; resume existing clock behavior at initial formation selection; network freeze/reconnect/abort unchanged"
  },
  "executionDefaults": {
    "branch": "codex/alpha-v263 in isolated workspace based on latest origin/dev; integrate and push origin/dev without force",
    "implementation": "native execution in current chat with independent review",
    "mechanicsRevision": 12,
    "protocol": 5,
    "stateFormat": "wizardcard-state-v8",
    "replayFormat": 6,
    "digestSchema": 4,
    "overflowOrder": "finish chain and derived effects, perform existing persistent-spell returns, then stabilize mandatory capacity cleanup; recheck after resulting effects",
    "sourceDocuments": "rename requirements to Alpha v2.6.3 更新需求.md and backfill decisions; store versioned offline rulebook in repo",
    "openingInterface": "add authoritative opening stage/round/private gesture state and SubmitOpeningGesture command; update legal query, viewer filtering, AI/hotseat privacy, codec/network/replay; normal clients may not override RPS winner",
    "acceptance": "A01-A12 plus A13 RPS opening, privacy, ties, reconnection, no clock charge, draw-before-formation order"
  }
}
```


新增A13：猜拳开局，验证隐私、平局重试、双方抽5张后选择初始阵法、重连、猜拳不扣时。
