# 运行时美术资源

当前发布资源由 `runtime.json`（传统卡牌插画）及 `godot/art/assets.json`（Godot 前端资源）登记。运行时副本位于 `godot/art/`；源文件位置、SHA-256、许可和制作来源保存在对应清单中。

源美术按用途保留在 `alpha-release`、`alpha-v1`、`alpha-v252-20261021`、`alpha-v254-20261024`、`cards`、`fonts`、`godot-2d-v1`、`godot-2d-v2`、`sique-20261020` 等目录。不要仅凭目录年代删除文件：先检查 `godot/art/assets.json` 的 `source` / `authoringSource` 映射，以及 `runtime.json`。旧探索稿与未使用原型已从工作区移除。

`ART_INVENTORY.json` 是源素材盘点，运行时副本单独登记。更新素材后运行 `python tools/check_art_assets.py --update-inventory`，验证时运行 `python tools/check_art_assets.py --check` 与 `python tools/prepare_godot_frontend_assets.py --check`。
