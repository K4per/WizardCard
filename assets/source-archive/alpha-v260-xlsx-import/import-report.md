# Alpha v2.6.0 XLSX 导入报告

状态：**blocked**

源目录：`E:\Project\WizardCard-Design\Alpha 现有卡池`
staging 内容 hash：`a77282b8a5b1934610f02e978b90c0042e7ac157d79bca3965bad220123828a7`

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

法术工作表明细为 72 条（解析/言灵/咒符 39/21/12）。工作簿说明页明确统计表是生成时静态结果、改动卡表后需手动同步，因此统计页的 71 与 30/24/17 已判定为陈旧统计，不再要求外部确认；原值仍保留在源快照。

## 实装状态

精确结构化效果候选：40；效果仍阻断：52；效果和卡图均已映射的候选：19；完整映射仍阻断：73。结构化仅表示可映射到既有原语，尚不代表 v2.6.0 逐卡行为已通过。

## 逐行与资源证据

- `card-rows.csv`：92 行字段映射和原始工作表坐标。
- `art-mapping.md`：49 张匹配、42 张缺失、1 张身份冲突；源目录共 40 张法术图和 9 张阵法通用图。
- `staging/test-decks.json`：只用于已映射候选的 40+10 结构校验，不进入正式构筑器。
- `staging/loader-validation.json`、`deck.json`、`side-deck.json`：全 92 定义的 C++ 内容加载器 schema 验证夹具；验证时仅临时覆写版本元数据以越过 2.6.0 发布门，不证明规则兼容或逐卡效果正确。

## 阻断项

- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:2` **火球术 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:3` **寒霜射线 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:4` **闪电束 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:5` **魔法飞弹 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:6` **繁彩球 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:7` **曳光弹 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:8` **炽热射线 / 效果**：伤害后的条件、时点或区域操作尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:8` **炽热射线 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:9` **炽燃结界 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:10` **阳炎爆 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:11` **燃烧之手 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:12` **云雾术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:12` **云雾术 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:13` **召雷术 / 效果**：对方独立荷载的结束阶段清除时点尚未映射到现有触发字段
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:13` **召雷术 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:15` **臭云术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:16` **油腻术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:17` **次级复原 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:17` **次级复原 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:18` **吸收元素 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:20` **缝合生命 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:21` **虚假生命 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:22` **治疗之环 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:22` **治疗之环 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:23` **致病射线 / 效果**：伤害后的条件、时点或区域操作尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:24` **防死结界 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:27` **朦胧术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:28` **恐惧术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:29` **精神监狱 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:30` **紊乱波动 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:30` **紊乱波动 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:31` **昏昏欲睡 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:31` **昏昏欲睡 / 卡图**：目录只有沉默术.png，不能确认它是否对应昏昏欲睡
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:32` **疯狂冠冕 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:33` **祝福术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:34` **灾祸术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:36` **地震术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:36` **地震术 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:37` **碎环术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:37` **碎环术 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:38` **土石喷发 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:39` **弹射术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:40` **加速术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:42` **火花术 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:44` **魔力扰动 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:45` **法师之手 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:46` **瞬发回路过载 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:46` **瞬发回路过载 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:47` **奥术复苏 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:48` **支援术 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:49` **心如止水 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:50` **扰神曲 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:51` **降咒 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:52` **七彩喷射 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:55` **脆弱诅咒 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:56` **命令术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:58` **克敌机先 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:58` **克敌机先 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:59` **奥术回想 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:60` **异界探知 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:61` **卜筮术 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:64` **炼狱叱喝 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:65` **捕获打击 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:66` **护盾术 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:67` **法术反制 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:67` **法术反制 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:68` **避火咒 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:69` **解除魔法 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:70` **死佑护甲 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:71` **步履维艰 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:72` **银光锐语 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:72` **银光锐语 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:73` **滑稽魔盒 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:73` **滑稽魔盒 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:74` **侦测善恶 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡列表.xlsx::卡表:75` **荆棘丛生 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:2` **逆三角星符 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:3` **奥术圆环 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\符文卡列表-共3张卡.xlsx::符文卡:4` **昂扬 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:3` **正十字 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:3` **正十字 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:4` **逆十字 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:4` **逆十字 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:5` **安卡十字 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:5` **安卡十字 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:6` **恩格亚斯之星 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:6` **恩格亚斯之星 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:7` **元素之环 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:7` **元素之环 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:8` **混沌之环 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:8` **混沌之环 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:9` **阿克乌姆之弧 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:9` **阿克乌姆之弧 / 卡图**：正式卡图未匹配
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:10` **硫磺 / 效果**：效果包含条件、时点、持续或未确认的组合语义，尚无可验证的一一对应结构化定义
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:10` **硫磺 / 效果**：硫磺离场触发伤害从现有能力固定值1改为源数据2，尚未有可参数化字段
- `E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡列表-共17张卡.xlsx::阵法卡:10` **硫磺 / 卡图**：正式卡图未匹配

## 输出

- `staging/`：92 张卡的可追溯 staging 定义，不会被生产加载器自动读取。
- `backup/runtime-inputs/`：导入前的正式卡池、卡组、预组和分卡 JSON 备份。
- `backup-manifest.json`：备份文件哈希。
- `source-manifest.json`：三份正式 XLSX 快照哈希。

生产覆盖当前不可用：除清零导入阻断外，还需完成 staging schema 评审、v2.6.0 规则裁定和迁移审批。生产内核仍严格拒绝 2.6.0/mechanicsRevision 10。
