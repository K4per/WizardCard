# Alpha v2.6.0 XLSX 导入报告

状态：**ready-to-apply**

源目录：`E:\Project\WizardCard-Design\Alpha 现有卡池`
staging 内容 hash：`545172a8f0743514e366c64e94c4b2605029367b5b0ec7beb9c5da1786210392`

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

结构化效果：92；效果阻断：0；卡图和效果均已映射：92；完整映射阻断：0。v2.6 staging 的未知效果使用 `V260Structured` 保留源语义；现有原语可直接执行的投影照常结算，其余语义写入确定性的 `v260_staging_adapter` 事件，禁止静默丢失。

## 逐行与资源证据

- `card-rows.csv`：92 行字段映射和原始工作表坐标。
- `art-mapping.md`：92 张匹配、0 张缺失、0 张身份冲突；49 张卡使用设计目录对应卡图，其中 29 张 RGB 已由脚本扣除浅色棋盘底、20 张 RGBA 保留源 alpha。源目录共 40 张法术图和 9 张阵法图，逐卡映射及源哈希见 `assets/art/alpha-v260/current-pool/manifest.json`。
- `staging/test-decks.json`：40+10 结构校验夹具，状态为 ready，不进入正式构筑器。
- `staging/loader-validation.json`、`deck.json`、`side-deck.json`：C++ 内容加载器直接按 `2.6.0/revision 10` 验证全 92 定义和 40+10 牌组；这项证据覆盖 schema 和数据完整性，不替代正式规则发布审批。

## 阻断项

- 无。

## 输出

- `staging/`：92 张卡的可追溯 staging 定义，不会被生产加载器自动读取。
- `backup/runtime-inputs/`：导入前的正式卡池、卡组、预组和分卡 JSON 备份。
- `backup-manifest.json`：备份文件哈希。
- `source-manifest.json`：三份正式 XLSX 快照哈希。

本次 staging 阻断已清零；正式覆盖仍需单独完成 v2.6.0 规则批准、联机/回放协议升级和迁移审批。生产 GameEngine 继续严格拒绝 2.6.0/mechanicsRevision 10，以保护当前 2.5.5 运行路径。
