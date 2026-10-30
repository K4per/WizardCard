# Alpha v2.6.0 XLSX 导入报告

状态：**blocked**

源目录：`E:\Project\WizardCard-Design\Alpha 现有卡池`
staging 内容 hash：`cc287c6ce51eecc77aebf994f1caf72895d87bb105a5986dff3238f160434860`

## 数量

| 类别 | 数量 |
|---|---:|
| 法术明细 | 72 |
| 解析法术 | 39 |
| 言灵法术 | 21 |
| 咒符法术 | 12 |
| 符文 | 3 |
| 阵法 | 17 |
| 合计 | 92 |

法术工作表明细为 72 条；统计工作表仍写 71，统计页的 30/24/17 也与明细类型 39/21/12 不一致。导入按明细逐行读取，差异保留为阻断项。

## 实装状态

结构化候选效果：13；阻断卡牌：79。候选效果仍需 C++ 权威内核逐卡回归后才能进入正式卡池。

## 逐行与资源证据

- `card-rows.csv`：92 行字段映射和原始工作表坐标。
- `art-mapping.md`：每张卡的卡图匹配状态；源目录共 40 张法术图和 9 张阵法通用图。
- `staging/test-decks.json`：只用于校验 40+10 结构，不进入正式构筑器。

## 阻断项

- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:2` **火球术 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:3` **寒霜射线 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:3` **寒霜射线 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:4` **闪电束 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:4` **闪电束 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:5` **魔法飞弹 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:5` **魔法飞弹 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:6` **繁彩球 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:6` **繁彩球 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:7` **曳光弹 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:7` **曳光弹 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:8` **炽热射线 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:8` **炽热射线 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:9` **炽燃结界 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:9` **炽燃结界 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:10` **阳炎爆 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:10` **阳炎爆 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:11` **燃烧之手 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:12` **云雾术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:12` **云雾术 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:13` **召雷术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:13` **召雷术 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:14` **匕首之云 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:15` **臭云术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:16` **油腻术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:17` **次级复原 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:17` **次级复原 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:18` **吸收元素 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:20` **缝合生命 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:21` **虚假生命 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:22` **治疗之环 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:22` **治疗之环 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:23` **致病射线 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:24` **防死结界 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:25` **汲血之触 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:26` **沉默术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:27` **朦胧术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:28` **恐惧术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:29` **精神监狱 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:30` **紊乱波动 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:30` **紊乱波动 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:31` **昏昏欲睡 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:31` **昏昏欲睡 / 效果/卡图**：目录只有沉默术.png，不能确认它是否对应昏昏欲睡
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:32` **疯狂冠冕 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:33` **祝福术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:34` **灾祸术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:35` **心灵尖刺 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:36` **地震术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:36` **地震术 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:37` **碎环术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:37` **碎环术 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:38` **土石喷发 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:39` **弹射术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:40` **加速术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:42` **火花术 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:43` **魔能爆 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:44` **魔力扰动 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:44` **魔力扰动 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:45` **法师之手 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:46` **瞬发回路过载 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:46` **瞬发回路过载 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:47` **奥术复苏 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:47` **奥术复苏 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:48` **支援术 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:49` **心如止水 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:49` **心如止水 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:50` **扰神曲 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:51` **降咒 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:52` **七彩喷射 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:54` **不谐低语 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:55` **脆弱诅咒 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:56` **命令术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:58` **克敌机先 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:58` **克敌机先 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:59` **奥术回想 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:59` **奥术回想 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:60` **异界探知 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:61` **卜筮术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:64` **炼狱叱喝 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:65` **捕获打击 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:66` **护盾术 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:66` **护盾术 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:67` **法术反制 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:67` **法术反制 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:68` **避火咒 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:68` **避火咒 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:69` **解除魔法 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:70` **死佑护甲 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:71` **步履维艰 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:72` **银光锐语 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:72` **银光锐语 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:73` **滑稽魔盒 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:73` **滑稽魔盒 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:74` **侦测善恶 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:75` **荆棘丛生 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:2` **逆三角星符 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:2` **逆三角星符 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:3` **奥术圆环 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:3` **奥术圆环 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:4` **昂扬 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:4` **昂扬 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:3` **正十字 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:3` **正十字 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:4` **逆十字 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:4` **逆十字 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:5` **安卡十字 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:5` **安卡十字 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:6` **恩格亚斯之星 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:6` **恩格亚斯之星 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:7` **元素之环 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:7` **元素之环 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:8` **混沌之环 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:8` **混沌之环 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:9` **阿克乌姆之弧 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:9` **阿克乌姆之弧 / 效果/卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:10` **硫磺 / 效果/卡图**：效果包含条件、时点、持续或组合语义，当前内核没有可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:10` **硫磺 / 效果/卡图**：正式卡图未匹配

## 输出

- `staging/`：92 张卡的可追溯 staging 定义，不会被生产加载器自动读取。
- `backup/runtime-inputs/`：导入前的正式卡池、卡组、预组和分卡 JSON 备份。
- `backup-manifest.json`：备份文件哈希。
- `source-manifest.json`：三份正式 XLSX 快照哈希。

`--apply` 只有在阻断项为零时才允许覆盖 `assets/`；当前报告为阻断状态，因此本次运行不会删除或替换生产卡池。
