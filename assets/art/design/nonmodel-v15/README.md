# A 古籍金饰 · v1.5 非建模资源

依据 [Godot最新清单](../../../../docs/art-resource-requirements-godot-v1.5.md)。原卡框与30插画不覆盖，传说保持金黄色宝石。本批使用项目原生数学纹理/SVG程序，可编辑和重建。

[画廊](gallery.html) · [文件清单](ASSET_LIST.md) · [状态表](../../../../docs/art-resource-status-v1.5.md) · [验证](validation.json) · [增补包](dist/WizardCard-A-v15-nonmodel.zip)。ZIP依赖现有WizardCard工程、A版资源/卡图/字体，是增补源码包，不是独立游戏。

## 通道与导入

- 暗木、酒红皮革、古金、环境石材：4套2K，每套基础色RGB/sRGB、法线RGB/线性/OpenGL +Y、粗糙度L/线性/R、金属度L/线性/R。纸边同规格512。无打包通道，无AO或位移依赖。相关通道来自同一周期高度源，不宣称扫描或高模烘焙。
- 原型PlaneMesh/BoxMesh现有UV平铺3×2，模型精修后需核对UV和木纹方向。StandardMaterial3D线性mipmap、法线强度0.35。环境石材等待模型；卡面无光照，背/边受光。原插画保持比例和nearest。
- 七类区域/空槽/关联512×256，透明无字；五区/空槽使用现有平面与Compatibility着色器，透明度0.32/0.19。动态Label3D约14屏幕像素，不使用Decal。
- 15类特效遮罩256×256，横排正常8帧/精简4帧。帧表是动画预览/替代采样资源，不与实时Tween严格逐帧一致。实时场景共享平面/Shader：事件0.4～0.5秒、精简≤0.16秒，循环环境/解析显式停止，精简关闭循环。暂停停止Tween与循环phase，无全屏后处理/Glow依赖。
- 7种交互图形用轮廓、虚线、角标、刻度和结点区分；4连锁图标、5九宫格HUD、3无字终局纹章。HUD20px安全边饰，阶段/资源6px切片，文字18/14px独立；SVG与PNG一一对应。
- 动作库10类动作和10个精简版本，路径为 `Visual:position/rotation/scale`，只移动展示子节点，避免改变命中根节点。库是线性采样轨道，运行Tween按规范用cubic-out；局部位移仅为装配样例，正式消费者使用已提交锚点。轻镜头反馈默认关闭，胜/负/平搭配独立纹章和动态文字。

## 预览与复建

Godot4.7.2 Compatibility打开 `scenes/art/nonmodel_gallery.tscn` 可播放、暂停、清空及切换精简模式，覆盖15特效，不依赖规则事件。`card_motion_sample.tscn`提供Visual/AnimationPlayer样例。当前对局已接材质、灯光、区域、HUD/交互/连锁与已有cues短特效；解析开始/循环、附着/阵毁/普通离场及完整开终局动作尚未全部自动接生命周期，详见状态表。

`previews/engine/empty-*` 是隐藏卡牌、区域计数置零的美术空场fixture，顶部资源仍为真实快照，权威对局不变；`busy-hud-*` 为真实Alpha AI复盘107步、查看者0，含左右HUD和手牌，三档分辨率。没有制作外围模型，仍可见空白外围，本批不是完整精修场景验收。

复建：`build_assets.py` → Godot导入 → 两个smoke脚本/GPU capture → `organize_assets.py`。Python依赖NumPy/Pillow与项目Noto字体。原始新PNG在assets/art/nonmodel，Godot镜像相同SHA；不把总览/说明截图用作贴图。
