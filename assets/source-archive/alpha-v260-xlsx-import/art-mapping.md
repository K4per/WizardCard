# Alpha v2.6.0 卡图映射

设计目录卡图优先于历史复用。RGB 源的棋盘底由 `tools/prepare_alpha_v260_card_art.py` 扣除；RGBA 源保留原始 alpha。逐张源文件、哈希和透明处理记录见 `assets/art/alpha-v260/current-pool/manifest.json`。

| 卡名 | 类型 | 状态 | illustrationId | mapping | manifestId | 源文件 | 运行文件 | 透明处理 |
|---|---|---|---|---|---|---|---|---|
| 火球术 | analytic | matched | fireball | 历史同名卡图 | fireball | E:\Project\WizardCard\assets\art\cards\fireball.png | 历史运行资源 | 沿用历史透明资源 |
| 寒霜射线 | analytic | matched | ray-of-frost | 历史同名卡图 | ray-of-frost | E:\Project\WizardCard\assets\art\alpha-release\ray-of-frost.png | 历史运行资源 | 沿用历史透明资源 |
| 闪电束 | analytic | matched | lightning-bolt | 历史同名卡图 | lightning-bolt | E:\Project\WizardCard\assets\art\alpha-release\lightning-bolt.png | 历史运行资源 | 沿用历史透明资源 |
| 魔法飞弹 | analytic | matched | magic-missile | 历史同名卡图 | magic-missile | E:\Project\WizardCard\assets\art\alpha-v1\magic-missile.png | 历史运行资源 | 沿用历史透明资源 |
| 繁彩球 | analytic | matched | v2-chromatic-orb | 历史同类卡图 | v2-chromatic-orb | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-chromatic-orb.png | 历史运行资源 | 沿用历史透明资源 |
| 曳光弹 | analytic | matched | v2-guiding-bolt | 历史同类卡图 | v2-guiding-bolt | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-guiding-bolt.png | 历史运行资源 | 沿用历史透明资源 |
| 炽热射线 | analytic | matched | v2-scorching-ray | 历史同类卡图 | v2-scorching-ray | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-scorching-ray.png | 历史运行资源 | 沿用历史透明资源 |
| 炽燃结界 | analytic | matched | ward | 历史同名卡图 | ward | E:\Project\WizardCard\assets\art\cards\ward.png | 历史运行资源 | 沿用历史透明资源 |
| 阳炎爆 | analytic | matched | v2-sunburst | 历史同类卡图 | v2-sunburst | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-sunburst.png | 历史运行资源 | 沿用历史透明资源 |
| 燃烧之手 | analytic | matched | v260-art-analytic-0010 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0010 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\燃烧之手.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0010.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 云雾术 | analytic | matched | cloud-mist | 历史同类卡图 | cloud-mist | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261017\storm-series-atlas.png | 历史运行资源 | 沿用历史透明资源 |
| 召雷术 | analytic | matched | thunder-summon | 历史同类卡图 | thunder-summon | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261017\storm-series-atlas.png | 历史运行资源 | 沿用历史透明资源 |
| 匕首之云 | analytic | matched | v260-art-analytic-0013 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0013 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\匕首之云.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0013.png | 源图已有有效 alpha；保留透明像素 |
| 臭云术 | analytic | matched | v260-art-analytic-0014 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0014 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\臭云术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0014.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 油腻术 | analytic | matched | v260-art-analytic-0015 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0015 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\油腻术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0015.png | 源图已有有效 alpha；保留透明像素 |
| 次级复原 | analytic | matched | lesser-restoration | 历史同名卡图 | lesser-restoration | E:\Project\WizardCard\assets\art\alpha-release\lesser-restoration.png | 历史运行资源 | 沿用历史透明资源 |
| 吸收元素 | analytic | matched | v260-art-analytic-0017 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0017 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\吸收元素.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0017.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 枯萎术 | analytic | matched | v260-art-analytic-0018 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0018 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\枯萎术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0018.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 缝合生命 | analytic | matched | mend | 历史同名卡图 | mend | E:\Project\WizardCard\assets\art\cards\mend.png | 历史运行资源 | 沿用历史透明资源 |
| 虚假生命 | analytic | matched | false-life | 历史同名卡图 | false-life | E:\Project\WizardCard\assets\art\alpha-release\false-life.png | 历史运行资源 | 沿用历史透明资源 |
| 治疗之环 | analytic | matched | v2-healing-circle | 历史同类卡图 | v2-healing-circle | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-healing-circle.png | 历史运行资源 | 沿用历史透明资源 |
| 致病射线 | analytic | matched | v260-art-analytic-0022 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0022 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\致病射线.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0022.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 防死结界 | analytic | matched | v260-art-analytic-0023 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0023 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\防死结界.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0023.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 汲血之触 | analytic | matched | v260-art-analytic-0024 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0024 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\汲血之触.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0024.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 沉默术 | analytic | matched | v260-art-analytic-0025 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0025 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\沉默术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0025.png | 源图已有有效 alpha；保留透明像素 |
| 朦胧术 | analytic | matched | v260-art-analytic-0026 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0026 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\朦胧术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0026.png | 源图已有有效 alpha；保留透明像素 |
| 恐惧术 | analytic | matched | v260-art-analytic-0027 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0027 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\恐惧术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0027.png | 源图已有有效 alpha；保留透明像素 |
| 精神监狱 | analytic | matched | v260-art-analytic-0028 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0028 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\精神监狱.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0028.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 紊乱波动 | analytic | matched | messy-wave | 历史同名卡图 | messy-wave | E:\Project\WizardCard\assets\art\alpha-v1\messy-wave.png | 历史运行资源 | 沿用历史透明资源 |
| 昏昏欲睡 | analytic | matched | instantly-sleep | 用户确认沿用旧沉默术卡图 | instantly-sleep | E:\Project\WizardCard\assets\art\alpha-v1\instantly-sleep.png | 历史运行资源 | 沿用历史透明资源 |
| 疯狂冠冕 | analytic | matched | v260-art-analytic-0031 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0031 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\疯狂冠冕.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0031.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 祝福术 | analytic | matched | v260-art-analytic-0032 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0032 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\祝福术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0032.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 灾祸术 | analytic | matched | v260-art-analytic-0033 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0033 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\灾祸术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0033.png | 源图已有有效 alpha；保留透明像素 |
| 心灵尖刺 | analytic | matched | v260-art-analytic-0034 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0034 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\心灵尖刺.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0034.png | 源图已有有效 alpha；保留透明像素 |
| 地震术 | analytic | matched | earthquake | 历史同名卡图 | earthquake | E:\Project\WizardCard\assets\art\alpha-v254-20261024\source\卡图替换\地震术.png | 历史运行资源 | 沿用历史透明资源 |
| 碎环术 | analytic | matched | unravel | 历史同名卡图 | unravel | E:\Project\WizardCard\assets\art\cards\unravel.png | 历史运行资源 | 沿用历史透明资源 |
| 土石喷发 | analytic | matched | v260-art-analytic-0037 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0037 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\土石喷发.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0037.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 弹射术 | analytic | matched | v260-art-analytic-0038 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0038 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\弹射术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0038.png | 源图已有有效 alpha；保留透明像素 |
| 加速术 | analytic | matched | v260-art-analytic-0039 | 用户指定 Alpha 现有卡池卡图 | v260-art-analytic-0039 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\加速术.png | assets/art/alpha-v260/current-pool/v260-art-analytic-0039.png | 源图已有有效 alpha；保留透明像素 |
| 火花术 | word | matched | spark | 历史同名卡图 | spark | E:\Project\WizardCard\assets\art\cards\spark.png | 历史运行资源 | 沿用历史透明资源 |
| 魔能爆 | word | matched | v260-art-word-0002 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0002 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\魔能爆.png | assets/art/alpha-v260/current-pool/v260-art-word-0002.png | 源图已有有效 alpha；保留透明像素 |
| 魔力扰动 | word | matched | disrupt | 历史同名卡图 | disrupt | E:\Project\WizardCard\assets\art\cards\disrupt.png | 历史运行资源 | 沿用历史透明资源 |
| 法师之手 | word | matched | v260-art-word-0004 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0004 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\法师之手.png | assets/art/alpha-v260/current-pool/v260-art-word-0004.png | 源图已有有效 alpha；保留透明像素 |
| 瞬发回路过载 | word | matched | instant-circuit-overload | 历史同名卡图 | instant-circuit-overload | E:\Project\WizardCard\assets\art\alpha-v1\instant-circuit-overload.png | 历史运行资源 | 沿用历史透明资源 |
| 奥术复苏 | word | matched | arcane-recovery | 历史同名卡图 | arcane-recovery | E:\Project\WizardCard\assets\art\alpha-release\arcane-recovery.png | 历史运行资源 | 沿用历史透明资源 |
| 支援术 | word | matched | aid | 历史同名卡图 | aid | E:\Project\WizardCard\assets\art\alpha-v1\aid.png | 历史运行资源 | 沿用历史透明资源 |
| 心如止水 | word | matched | clarity | 历史同名卡图 | clarity | E:\Project\WizardCard\assets\art\cards\clarity.png | 历史运行资源 | 沿用历史透明资源 |
| 扰神曲 | word | matched | v260-art-word-0009 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0009 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\扰神曲.png | assets/art/alpha-v260/current-pool/v260-art-word-0009.png | 源图已有有效 alpha；保留透明像素 |
| 降咒 | word | matched | v260-art-word-0010 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0010 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\降咒.png | assets/art/alpha-v260/current-pool/v260-art-word-0010.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 七彩喷射 | word | matched | v260-art-word-0011 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0011 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\七彩喷射.png | assets/art/alpha-v260/current-pool/v260-art-word-0011.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 幽影刃 | word | matched | v260-art-word-0012 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0012 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\幽影刃.png | assets/art/alpha-v260/current-pool/v260-art-word-0012.png | 源图已有有效 alpha；保留透明像素 |
| 不谐低语 | word | matched | v260-art-word-0013 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0013 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\不谐低语.png | assets/art/alpha-v260/current-pool/v260-art-word-0013.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 脆弱诅咒 | word | matched | v260-art-word-0014 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0014 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\脆弱诅咒.png | assets/art/alpha-v260/current-pool/v260-art-word-0014.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 命令术 | word | matched | v260-art-word-0015 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0015 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\命令术.png | assets/art/alpha-v260/current-pool/v260-art-word-0015.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 恶言相加 | word | matched | v260-art-word-0016 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0016 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\恶言相加.png | assets/art/alpha-v260/current-pool/v260-art-word-0016.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 克敌机先 | word | matched | true-strike | 历史同名卡图 | true-strike | E:\Project\WizardCard\assets\art\alpha-v1\true-strike.png | 历史运行资源 | 沿用历史透明资源 |
| 奥术回想 | word | matched | recall | 历史同名卡图 | recall | E:\Project\WizardCard\assets\art\cards\recall.png | 历史运行资源 | 沿用历史透明资源 |
| 异界探知 | word | matched | v260-art-word-0019 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0019 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\异界探知.png | assets/art/alpha-v260/current-pool/v260-art-word-0019.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 卜筮术 | word | matched | v260-art-word-0020 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0020 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\卜筮术.png | assets/art/alpha-v260/current-pool/v260-art-word-0020.png | 源图已有有效 alpha；保留透明像素 |
| 强化属性 | word | matched | v260-art-word-0021 | 用户指定 Alpha 现有卡池卡图 | v260-art-word-0021 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\强化属性.png | assets/art/alpha-v260/current-pool/v260-art-word-0021.png | 源图已有有效 alpha；保留透明像素 |
| 炼狱叱喝 | talisman | matched | v260-art-talisman-0001 | 用户指定 Alpha 现有卡池卡图 | v260-art-talisman-0001 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\炼狱叱喝.png | assets/art/alpha-v260/current-pool/v260-art-talisman-0001.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 捕获打击 | talisman | matched | v260-art-talisman-0002 | 用户指定 Alpha 现有卡池卡图 | v260-art-talisman-0002 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\捕获打击.png | assets/art/alpha-v260/current-pool/v260-art-talisman-0002.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 护盾术 | talisman | matched | shield | 历史同名卡图 | shield | E:\Project\WizardCard\assets\art\alpha-release\shield.png | 历史运行资源 | 沿用历史透明资源 |
| 法术反制 | talisman | matched | counter-spell | 历史同名卡图 | counter-spell | E:\Project\WizardCard\assets\art\alpha-v1\counter-spell.png | 历史运行资源 | 沿用历史透明资源 |
| 避火咒 | talisman | matched | protective-flame | 历史同名卡图 | protective-flame | E:\Project\WizardCard\assets\art\alpha-v1\protective-flame.png | 历史运行资源 | 沿用历史透明资源 |
| 解除魔法 | talisman | matched | v260-art-talisman-0006 | 用户指定 Alpha 现有卡池卡图 | v260-art-talisman-0006 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\解除魔法.png | assets/art/alpha-v260/current-pool/v260-art-talisman-0006.png | 源图已有有效 alpha；保留透明像素 |
| 死佑护甲 | talisman | matched | v260-art-talisman-0007 | 用户指定 Alpha 现有卡池卡图 | v260-art-talisman-0007 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\死佑护甲.png | assets/art/alpha-v260/current-pool/v260-art-talisman-0007.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 步履维艰 | talisman | matched | v260-art-talisman-0008 | 用户指定 Alpha 现有卡池卡图 | v260-art-talisman-0008 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\步履维艰.png | assets/art/alpha-v260/current-pool/v260-art-talisman-0008.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 银光锐语 | talisman | matched | barbs | 历史同名卡图 | barbs | E:\Project\WizardCard\assets\art\cards\barbs.png | 历史运行资源 | 沿用历史透明资源 |
| 滑稽魔盒 | talisman | matched | v2-funny-box | 历史同名卡图 | v2-funny-box | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-funny-box.png | 历史运行资源 | 沿用历史透明资源 |
| 侦测善恶 | talisman | matched | v260-art-talisman-0011 | 用户指定 Alpha 现有卡池卡图 | v260-art-talisman-0011 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\侦测善恶.png | assets/art/alpha-v260/current-pool/v260-art-talisman-0011.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 荆棘丛生 | talisman | matched | v260-art-talisman-0012 | 用户指定 Alpha 现有卡池卡图 | v260-art-talisman-0012 | E:\Project\WizardCard-Design\Alpha 现有卡池\法术卡卡图\荆棘丛生.png | assets/art/alpha-v260/current-pool/v260-art-talisman-0012.png | 源图已有有效 alpha；保留透明像素 |
| 逆三角星符 | seal | matched | conduit | 历史同名卡图 | conduit | E:\Project\WizardCard\assets\art\cards\conduit.png | 历史运行资源 | 沿用历史透明资源 |
| 奥术圆环 | seal | matched | ring | 历史同名卡图 | ring | E:\Project\WizardCard\assets\art\cards\ring.png | 历史运行资源 | 沿用历史透明资源 |
| 昂扬 | seal | matched | uplift | 历史同名卡图 | uplift | E:\Project\WizardCard\assets\art\alpha-v1\uplift.png | 历史运行资源 | 沿用历史透明资源 |
| 正十字 | formation | matched | v2-upright-cross | 历史对应阵法卡图 | v2-upright-cross | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-upright-cross.png | 历史运行资源 | 沿用历史透明资源 |
| 逆十字 | formation | matched | v2-inverted-cross | 历史对应阵法卡图 | v2-inverted-cross | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-inverted-cross.png | 历史运行资源 | 沿用历史透明资源 |
| 安卡十字 | formation | matched | v2-ankh | 历史对应阵法卡图 | v2-ankh | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-ankh.png | 历史运行资源 | 沿用历史透明资源 |
| 恩格亚斯之星 | formation | matched | engeas-four-point-star | 历史同名卡图 | engeas-four-point-star | E:\Project\WizardCard\assets\art\alpha-v1\engeas-four-point-star.png | 历史运行资源 | 沿用历史透明资源 |
| 元素之环 | formation | matched | v2-elemental-ring | 历史同名卡图 | v2-elemental-ring | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-elemental-ring.png | 历史运行资源 | 沿用历史透明资源 |
| 混沌之环 | formation | matched | v2-chaos-ring | 历史同名卡图 | v2-chaos-ring | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-chaos-ring.png | 历史运行资源 | 沿用历史透明资源 |
| 阿克乌姆之弧 | formation | matched | reservoir | 历史同名卡图 | reservoir | E:\Project\WizardCard\assets\art\cards\reservoir.png | 历史运行资源 | 沿用历史透明资源 |
| 硫磺 | formation | matched | v2-sulfur | 历史同名卡图 | v2-sulfur | E:\Project\WizardCard\assets\art\godot-2d-v2\card-illustrations-v2-20261011\v2-sulfur.png | 历史运行资源 | 沿用历史透明资源 |
| 基础法阵 | formation | matched | v260-art-formation-0009 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0009 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\基础.png | assets/art/alpha-v260/current-pool/v260-art-formation-0009.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 标准法阵 | formation | matched | v260-art-formation-0010 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0010 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\标准.png | assets/art/alpha-v260/current-pool/v260-art-formation-0010.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 强化法阵 | formation | matched | v260-art-formation-0011 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0011 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\强化.png | assets/art/alpha-v260/current-pool/v260-art-formation-0011.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 复合法阵 | formation | matched | v260-art-formation-0012 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0012 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\复合.png | assets/art/alpha-v260/current-pool/v260-art-formation-0012.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 增幅法阵 | formation | matched | v260-art-formation-0013 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0013 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\增幅.png | assets/art/alpha-v260/current-pool/v260-art-formation-0013.png | 源图已有有效 alpha；保留透明像素 |
| 守护法阵 | formation | matched | v260-art-formation-0014 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0014 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\守护.png | assets/art/alpha-v260/current-pool/v260-art-formation-0014.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 幻象法阵 | formation | matched | v260-art-formation-0015 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0015 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\幻象.png | assets/art/alpha-v260/current-pool/v260-art-formation-0015.png | 源图已有有效 alpha；保留透明像素 |
| 高阶法阵 | formation | matched | v260-art-formation-0016 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0016 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\高阶.png | assets/art/alpha-v260/current-pool/v260-art-formation-0016.png | 脚本移除边缘连通及大面积浅色棋盘背景 |
| 完美法阵 | formation | matched | v260-art-formation-0017 | 用户指定 Alpha 现有卡池卡图 | v260-art-formation-0017 | E:\Project\WizardCard-Design\Alpha 现有卡池\阵法卡卡图\完美.png | assets/art/alpha-v260/current-pool/v260-art-formation-0017.png | 源图已有有效 alpha；保留透明像素 |
