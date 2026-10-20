# 卡牌包与玩家牌组交换

## 开发者卡牌包

`tools/card_pack.py` 使用 `wizardcard.card-pack` 格式 1。它与玩家牌组文件独立：只含 `pool` 卡池元数据、按稳定 ID 排序的 `cards` 定义和按资源 ID 排序的 `resources`，不含玩家资料、牌组、存档或对局状态。

卡牌包是 UTF-8 JSON 单文件；必要插图以 Base64 内嵌，并记录逻辑资源 ID、导入相对路径和 SHA-256。此版本不接受目录包、ZIP、绝对路径、反斜杠、盘符、空路径段、`.` / `..` 或越过目标目录的符号链接。资源文件只能进入 `card-pack-resources/`，最大包大小为 256 MiB。引用必须完整且无额外资源；导入后 `card-pack-resources.json` 管理资源映射。

```powershell
# 指定卡牌；省略 --cards 导出完整正式卡池
python tools/card_pack.py export build/sique.cardpack.json --cards sique-woodpile sique-campfire

# 导入新卡池或合并至已有 canonical assets 目录
python tools/card_pack.py import build/sique.cardpack.json --assets build/imported-assets

# 覆盖同 ID 必须明确声明；不会允许不同 ID 共享编号
python tools/card_pack.py import build/sique.cardpack.json --assets build/imported-assets --overwrite
```

导入使用 `wizard_cli validate-catalog` 验证完整候选卡池，因此必须先构建当前版本的 `wizard_cli`；非默认构建路径可使用 `--validator <exe>`。该模式直接使用正式内容解析器，能拒绝未知效果、无效枚举、越界数值、重复编号及非法规则组合，不依赖玩家牌组。导入空目录时，包本身也必须满足完整卡池约束（例如含至少一个等级 1 阵法，且配方引用全部可解析）；子集包应合并至已有卡池。包层额外拒绝未知字段、重复 JSON 键、重复 ID/资源、资源缺失、哈希不匹配和不兼容的卡池元数据。错误包含字段路径或卡牌 ID。

所有校验在目标目录之外的暂存目录完成；校验成功后才提交定义、资源、兼容聚合 `cards.json` 与 canonical `catalog.json`。提交失败会恢复已替换的文件，并删除新写入文件和空目录。已有资源和其他资产不参与替换。新增或覆盖后，应运行当前项目的内容校验与聚合一致性检查；集成 Godot 的自定义卡池还需将 `card-pack-resources.json` 的稳定 ID 映射加入运行时资源清单。

自动回归：`python -m unittest discover -s tools -p test_card_pack.py`。覆盖确定性导出、真实解析器 roundtrip、资源完整性、冲突、未知版本、路径逃逸、重复 ID/编号、未知字段/效果，以及失败时目标内容不变。

## 玩家牌组文件

公开交换格式为 `wizardcard-deck`，`version` 当前为 `1`，以 `godot/app/deck_exchange.gd` 为实现依据。玩家从“卡组构筑”的牌组列表选择“导入牌组”，在编辑页选择“导出牌组”。两者均使用独立 JSON 文件，不依赖开发仓库路径；文件最大为 1 MiB。

顶层只接受 `format`、`version`、`name`、`main`、`side` 五个字段。`name` 是去除两端空白后非空、最多 80 字符的名称。`main` 和 `side` 分别为主卡组与副卡组条目数组，每个数组最多 1,000 个条目；条目恰有 `id` 和 `count` 两个字段。`id` 是正式卡池稳定 ID，`count` 是 1–3 的整数值。导出将两个分区各自按 ID 排序。

以下是合法的 40 张主卡组 / 10 张副卡组示例：

```json
{
  "format": "wizardcard-deck",
  "version": 1,
  "name": "基础练习",
  "main": [
    {"id": "aid", "count": 1},
    {"id": "arcane-recovery", "count": 1},
    {"id": "barbs", "count": 1},
    {"id": "clarity", "count": 3},
    {"id": "conduit", "count": 3},
    {"id": "counter-spell", "count": 1},
    {"id": "disrupt", "count": 1},
    {"id": "fireball", "count": 3},
    {"id": "instant-circuit-overload", "count": 1},
    {"id": "instantly-sleep", "count": 1},
    {"id": "magic-missile", "count": 1},
    {"id": "mend", "count": 3},
    {"id": "messy-wave", "count": 1},
    {"id": "protective-flame", "count": 1},
    {"id": "ray-of-frost", "count": 1},
    {"id": "recall", "count": 3},
    {"id": "ring", "count": 3},
    {"id": "spark", "count": 3},
    {"id": "true-strike", "count": 1},
    {"id": "unravel", "count": 3},
    {"id": "uplift", "count": 1},
    {"id": "ward", "count": 3}
  ],
  "side": [
    {"id": "elemental-ring", "count": 1},
    {"id": "engeas-four-point-star", "count": 1},
    {"id": "hexagram", "count": 2},
    {"id": "octagram", "count": 1},
    {"id": "pentagram-of-life", "count": 1},
    {"id": "reservoir", "count": 1},
    {"id": "small-hexagram", "count": 3}
  ]
}
```

文件解析后仍需通过原生 `constructionProblems` 校验：主卡组恰为 40 张、副卡组恰为 10 张；阵法只能进入副卡组，其余正式卡牌进入主卡组；各分区按卡名累计最多 3 张；副卡组至少包含一张等级 1 阵法。初始阵法在开局时选择，因此交换文件没有 `initialFormation`。即使不同稳定 ID 同名，也不会绕过同名上限。

未知格式/版本、未知顶层字段、缺失卡牌、重复条目、非整数或越界数量、分区错误与张数错误都会拒绝整份文件，并显示字段/条目位置或具体卡牌原因；同 ID 重复条目不会自动相加，缺失 ID 不会静默丢弃或按卡名猜测。只有解析与构筑校验全部成功才替换当前编辑草稿，导入失败保持当前草稿。成功导入创建新的本地身份，玩家可以检查后保存。

此文件只携带名称、稳定 ID 与数量，不携带本地草稿 ID、已选初始阵法、卡牌编号、规则/效果定义、资源路径、图片、账号资料、对局状态或存档。它不会安装缺少的卡牌；客户端按当前正式卡池解析 ID，缺卡或规则不兼容时明确拒绝。开发者安装卡牌定义与图片使用上面的 `wizardcard.card-pack` 工具。它与本地 `decks.json` 外层格式 1、内部草稿格式 3，以及回放格式互相独立，不能直接互换或据此推定相同版本号。

玩家功能回归脚本为 `godot/tests/deck_exchange_smoke.gd`，覆盖有效往返、两分区内容保持、新本地身份，以及未知版本、重复条目、缺卡、非法分区、张数不符和超过数量上限；客户端测试结果由发布验收记录。

## Alpha v2.5.1 实际工具验收

2026-10-20 使用重建的 `build/alpha251-bridge/Release/wizard_cli.exe` 执行卡牌包 8 项回归，全部通过。完整正式卡池导出、导入到新目录、再次导出：78 张卡、78 份插图，所有定义按稳定 ID 逐值等价，再导出文件与首次导出字节完全相同。

自包含包大小为 77,828,208 字节，SHA-256 为 `60419d6e02ee8f9abae738fbbe27f3cb239f70aa13a8d37510e4dee42986f850`。导入后的原生卡池校验输出 `78 definitions; hash a2e380f91fc4f9cb`。导出按 ID 稳定排序，原 manifest 的历史条目顺序不要求保留；卡牌身份和字段值保持。真实运行证据保存在 `build/card-pack-final-report.json`。
