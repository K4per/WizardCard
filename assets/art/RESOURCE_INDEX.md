# 美术资源索引

运行时状态以 `godot/art/assets.json` 与 `assets/art/runtime.json` 为准。前者记录 Godot 页面素材和对应源文件，后者记录稳定卡牌 ID 到卡牌插画的映射。已发布 UI、品牌图和内容图像不等同于可编辑源稿。

## 当前资源源目录

| 目录 | 当前用途 | 保留原因 |
|---|---|---|
| `alpha-release`、`alpha-v1`、`cards` | 传统卡牌插画 | 被 `runtime.json` 或 Godot 卡图映射使用 |
| `alpha-v252-20261021`、`alpha-v254-20261024`、`sique-20261020` | 当前卡池系列插画 | 被 Godot 运行清单引用 |
| `godot-2d-v1`、`godot-2d-v2` | 页面、卡框、主菜单和构筑素材源 | 被 Godot 运行清单引用 |
| `fonts` | 字体与许可文件 | 被 Godot 字体清单引用 |
| `licenses` | 字体及美术许可说明 | 随发布资源分发 |

## 验证

`ART_INVENTORY.json` 按文件记录源素材哈希、尺寸、类别和运行时映射。执行 `python tools/check_art_assets.py --check` 检查盘点与内容；执行 `python tools/prepare_godot_frontend_assets.py --check` 检查已提交的 Godot 资源副本。新资源应先更新对应运行清单，再刷新盘点。
