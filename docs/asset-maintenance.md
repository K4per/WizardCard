# 美术资产维护

正式运行资源由 `godot/art/assets.json` 管理，资产 ID 是卡牌、界面和运行时纹理之间的稳定关联。源文件、候选设计、验证截图和历史方案保留在 `assets/art/`，不由 Godot 运行时直接读取；`godot/art/` 是可提交的运行时镜像。

新增或替换资源时，先在对应 `assets/art/godot-2d-v2/<batch>/` 或明确命名的独立批次目录建立源稿、导出文件、说明和 manifest，记录来源、许可、处理参数和 SHA-256。需要运行时接入时运行：

```powershell
python tools/prepare_godot_frontend_assets.py
python tools/prepare_godot_frontend_assets.py --check
```

同步脚本只复制清单选择的运行资源，不修改源稿，并保留 Storm、味契等独立登记批次。`--check` 依赖已提交的运行资源、正式卡池与预览数据，会检查稳定 ID、路径、哈希、PNG 尺寸、卡框几何、必需资源和全部正式卡牌插图映射。

发布前运行资产盘点：

```powershell
python tools/check_art_assets.py --check --output build/art-asset-report-final.json
```

该报告区分旧资源、候选资源、源稿和归档目录，记录文件数、体积、未登记文件和相同哈希文件。重复文件只有在用途或运行时路径确实不同且记录在报告/批次说明中时才能保留。

删除或归档前，检查场景、脚本、动态路径、manifest、生成脚本、打包脚本和文档引用。正式发布包只包含运行时 manifest 指向的 Godot 资源、正式卡池定义、字体、音频和许可证；源稿、预览图、设计稿、历史方案和可再生成产物不得进入包。

味契插图的原始 JPEG、处理参数、卡名映射及 SHA-256 保存在 `assets/art/sique-20261020/manifest.json`，用 `python tools/prepare_sique_art.py` 再生成。不要使用旧 `weqi_*.png` 名称推断正式身份。卡牌定义中的 `illustrationId` 必须能解析到 `godot/art/assets.json`，并通过前端资源检查后才可发布。

开发者卡牌包格式与导入方式见 `docs/card-pack.md`；自包含包保留必要插图与稳定 ID。导入开发目录后，将资源映射登记至运行时 manifest；不要将整包或创作源文件复制进 `godot/art/`。

## Alpha v2.5.1 盘点与实际检查

2026-10-20 的数量口径为 `assets/art/` 中除清单自身以外的全部文件，以及 `godot/art/assets.json` 注册的运行资源；源文件数包含说明、manifest、创作稿、候选图和历史图，不代表独立插图数量，也不等于最终便携包体积。

| 范围 | 本次接入前 | 完成味契接入后 | 变化 |
|---|---:|---:|---:|
| 源资产文件数 | 1,418 | 1,452 | +34 |
| 源资产字节数 | 622,739,815 | 671,421,141 | +48,681,326 |
| 登记运行资源数 | 283 | 299 | +16 |
| 登记运行资源字节数 | 117,707,489 | 134,034,645 | +16,327,156 |

新增源文件为味契 16 张原始 JPG、16 张透明导出 PNG、处理 manifest 与批次说明。源目录还包含本次更新的索引说明，因此字节差同时计入已有说明文件的内容变化。运行资源增加仅为味契 16 张 PNG；其余已有资源条目的文件路径、来源、处理元数据与哈希保持。旧 `weqi_*.png` 候选方案原位保留，没有替代用户提供的本批 JPG。正式 62 张旧卡的逐卡定义与美术比对见 [翻新审计](card-refresh-2.5.1.md)。

实际执行并通过：

- `python tools/prepare_godot_frontend_assets.py --check`：299 项，134,034,645 字节；正式卡池 78 张插图全部可解析。
- `python tools/check_art_assets.py --check --output build/art-asset-report-final.json`：1,452 个源文件，671,421,141 字节；清单未登记/陈旧条目和校验错误均为 0。检查源与运行文件 SHA-256，不只是文件名是否存在。
- `python tools/catalog_source.py --check`：canonical 卡池与兼容聚合一致。
- 卡牌包回归 8 项通过；78 张卡和 78 份资源导出/导入/再导出字节一致，数据见 [卡牌包说明](card-pack.md)。

源资产仍有 93 组相同哈希，涉及 279 个源文件；这是保留记录，不是“已消除重复”的结论。包括同一模板服务不同语义区域、历史方案和创作/导出副本。清单的 `duplicateHashes` 同时记录源/运行镜像的复制理由。没有因静态搜索无结果或相同哈希而删除这些文件。

以上检查证明清单、来源和运行文件一致。便携包是否排除源稿/预览/历史材料由最终打包检查确认，不能仅凭本次资源检查推定。正式资源迁移保持原路径，本批新增插图未移动旧 Godot 资源，因此没有改写已有资源 UID 或导入设置。
