# Alpha v2.5.3 卡池逐卡差异与来源记录

来源快照位于本目录 `sources/`，其原始文件路径与 SHA-256 在 `source-manifest.json`。工具 `tools/import_alpha_v253_content.py` 从快照读取，只有显式传入 `--apply-reviewed-fields` 才修改卡牌 JSON；该导入器不从效果文案推导可执行效果。

本次源文件标题数：102；正式卡牌：99；待定草稿：3。源文件计数：spells 49；sique 19；storm 15；formations 16；seals 3。

对比基线为更新前已提交的规则/卡池 2.5.2/2.5.2，其 102 张定义保存在 `previous-catalog.json`。新增：无；更新：53 张；退池：ankh、inverted-cross、upright-cross（三张待定草稿）；正式重命名：无；重复来源 ID：无。

## 待确认与阻断

- 正十字、逆十字、安卡十字：源表标为待定且效果栏为空，不进入正式卡池。
- 恒风·风暴纹章：源字段问号已由用户确认沿用现行档位；初始0指示物，0–2为等级1/环1/载5/收1，3–4为等级2/环2/载8/收1，5以上为等级3/环3/载10/收2。
- 奥数圆环/奥术圆环、恩格亚斯之星/恩格亚斯四角星：来源与现行名称不同，稳定 ID 保留，等待设计方确认名称。
- 神恩炖肉：用户确认仅替代魔素，按1:1在宣告时支付对方普通生命且至少保留1生命；不替代符文、生命等额外成本。倒吊人：用户确认按普通生命向上取整一半并在宣告时支付；交换仅影响非阵法卡，沿用现行洗牌。

- 糖浆煎饼按已确认通用结算：每个普通伤害事件先处理免疫、抗性及减伤，再限制到最多4点，最后临时生命吸收、普通生命扣减；多段伤害分别处理，过载和枯竭真实伤害不受本上限影响。下个己方准备阶段开始时失效。
- 飘香诱惑按已确认通用结算：双方手牌分别返回并洗牌，再分别选择是否抽取各自实际返回数量；每次抽牌统一经过手牌上限、抽牌触发及卡组不足时的枯竭规则。完整发布口径见 `docs/card-migration-2.5.3.md`。

## 逐卡记录

| 来源/卡名 | ID | 编号 | 状态/效果实现 | 相对2.5.2变更 | 来源尚存差异 | 图像状态 | 测试/处理记录 |
|---|---|---:|---|---|---|---|---|
| `sources/spells.md` 火球术 | `fireball` | 020001 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/fireball.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 寒霜射线 | `ray-of-frost` | 020009 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/ray-of-frost.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 闪电束 | `lightning-bolt` | 020010 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/lightning-bolt.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 魔法飞弹 | `magic-missile` | 020007 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/magic-missile.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 繁彩球 | `chromatic-orb` | 020013 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/v2-chromatic-orb.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 曳光弹 | `guiding-bolt` | 020014 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/v2-guiding-bolt.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 缝合生命 | `mend` | 020002 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/mend.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 虚假生命 | `false-life` | 020012 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/false-life.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 炽热射线 | `scorching-ray` | 020015 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/v2-scorching-ray.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 炽燃结界 | `ward` | 020004 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/ward.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 阳炎爆 | `sunburst` | 020016 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/v2-sunburst.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 治疗之环 | `healing-circle` | 020017 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/v2-healing-circle.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 地震术 | `earthquake` | 020026 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/unravel.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 昏昏入睡 | `instantly-sleep` | 020006 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/instantly-sleep.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 碎环术 | `unravel` | 020003 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/unravel.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 紊乱波动 | `messy-wave` | 020005 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/messy-wave.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 次级复原术 | `lesser-restoration` | 020011 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/lesser-restoration.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 云雾术 | `cloud-mist` | 020021 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/cloud-mist.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 召雷术 | `thunder-summon` | 020022 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/thunder-summon.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 皇后 | `arcana-empress` | 020028 | MATCHED；权威结构化效果/机制 | speed | 无 | 完整 `res://art/card/arcana-empress.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 恋人 | `arcana-lovers` | 020029 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-lovers.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 战车 | `arcana-chariot` | 020030 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-chariot.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 力量 | `arcana-strength` | 020031 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-strength.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 隐士 | `arcana-hermit` | 020032 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/arcana-hermit.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 倒吊人 | `arcana-hanged-man` | 020033 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-hanged-man.png` | tests/sique.cpp [alpha-v253] (ordinary-life cost, declaration timing, nonformation exchange, debug immunity) |
| `sources/spells.md` 死神 | `arcana-death` | 020034 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-death.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 节制 | `arcana-temperance` | 020035 | MATCHED；权威结构化效果/机制 | speed | 无 | 完整 `res://art/card/arcana-temperance.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 恶魔 | `arcana-devil` | 020036 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-devil.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 高塔 | `arcana-tower` | 020037 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/arcana-tower.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 火花术 | `spark` | 030002 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/spark.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 魔力扰动 | `disrupt` | 030003 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/disrupt.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 克敌机先 | `true-strike` | 030005 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/true-strike.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 奥术回想 | `recall` | 030001 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/recall.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 奥术复苏 | `arcane-recovery` | 030008 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/arcane-recovery.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 瞬发回路过载 | `instant-circuit-overload` | 030007 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/instant-circuit-overload.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 支援术 | `aid` | 030017 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/aid.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 心如止水 | `clarity` | 030004 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/clarity.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 愚者 | `arcana-fool` | 030019 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-fool.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 魔术师 | `arcana-magician` | 030020 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-magician.png` | tests/sique.cpp [alpha-v253] (opponent chooses among three distinct revealed names, rejects forged choices) |
| `sources/spells.md` 女祭司 | `arcana-priestess` | 030021 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-priestess.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 皇帝 | `arcana-emperor` | 030022 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/arcana-emperor.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 正义 | `arcana-justice` | 030023 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/arcana-justice.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 世界 | `arcana-world` | 030024 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-world.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 银光锐语 | `barbs` | 040001 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/barbs.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 护盾术 | `shield` | 040003 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/shield.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 法术反制 | `counter-spell` | 040002 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/counter-spell.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 避火咒 | `protective-flame` | 040005 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/protective-flame.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 滑稽魔盒 | `funny-box` | 040004 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/v2-funny-box.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/spells.md` 教皇 | `arcana-hierophant` | 040012 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/arcana-hierophant.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·柴堆 | `sique-woodpile` | 010016 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-woodpile.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·篝火 | `sique-campfire` | 010017 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-campfire.png` | tests/sique.cpp |
| `sources/sique.md` 味契·魔力坩埚 | `sique-cauldron` | 010018 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-cauldron.png` | tests/sique.cpp |
| `sources/sique.md` 味契·香醋 | `sique-vinegar` | 050006 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/sique-vinegar.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·冰糖 | `sique-sugar` | 050007 | MATCHED；权威结构化效果/机制 | onLeave, text | 无 | 完整 `res://art/card/sique-sugar.png` | tests/sique.cpp |
| `sources/sique.md` 味契·海盐 | `sique-salt` | 050008 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-salt.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·辣椒 | `sique-chili` | 050009 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/sique-chili.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·欧芹 | `sique-parsley` | 050010 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-parsley.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·炙烤 | `sique-roast` | 020027 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-roast.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·奶油蘑菇浓汤 | `sique-mushroom-soup` | 020023 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-mushroom-soup.png` | tests/sique.cpp |
| `sources/sique.md` 味契·糖浆煎饼 | `sique-pancakes` | 020024 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-pancakes.png` | tests/sique.cpp |
| `sources/sique.md` 味契·神恩炖肉 | `sique-divine-stew` | 020025 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-divine-stew.png` | tests/sique.cpp |
| `sources/sique.md` 味契·原料召来 | `sique-ingredients` | 030014 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-ingredients.png` | tests/sique.cpp |
| `sources/sique.md` 味契·风味转化 | `sique-flavor-shift` | 030015 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-flavor-shift.png` | tests/sique.cpp |
| `sources/sique.md` 味契·烈火回炉 | `sique-refire` | 030016 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-refire.png` | tests/sique.cpp |
| `sources/sique.md` 味契·吹火术 | `sique-blowfire` | 030018 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/sique-blowfire.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·回火 | `sique-backfire` | 040011 | MATCHED；权威结构化效果/机制 | text, traits | 无 | 完整 `res://art/card/sique-backfire.png` | tests/sique.cpp |
| `sources/sique.md` 味契·沸汤滚袭 | `sique-boiling-surge` | 040009 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/sique-boiling-surge.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/sique.md` 味契·飘香诱惑 | `sique-aroma-lure` | 040010 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/sique-aroma-lure.png` | tests/sique.cpp |
| `sources/storm.md` 恒风·风暴纹章 | `storm-crest` | 010014 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/storm-crest.png` | tests/storm.cpp (counter tiers, activation and search); tests/alpha_v252_pool.cpp (formal recipe) |
| `sources/storm.md` 恒风·风暴之心 | `storm-heart` | 010015 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/storm-heart.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·风暴烙印 | `storm-brand` | 050004 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/storm-brand.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·湮灭之纹章 | `storm-annihilation-crest` | 050005 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/storm-annihilation-crest.png` | tests/alpha_v253_rules.cpp (both host types restricted) |
| `sources/storm.md` 恒风·冰风暴 | `ice-storm` | 020018 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/ice-storm.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·切割之风 | `cutting-wind` | 020019 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/cutting-wind.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·大风暴术 | `great-storm` | 020020 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/great-storm.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·风语术 | `wind-whisper` | 030009 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/wind-whisper.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·极速之风 | `swift-wind` | 030010 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/swift-wind.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·造风术 | `windmaking` | 030012 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/windmaking.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·风暴导向 | `storm-guidance` | 030013 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/storm-guidance.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·雪雨暴 | `snow-rain-storm` | 040013 | MATCHED；权威结构化效果/机制 | number, type | 无 | 完整 `res://art/card/snow-rain-storm.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·风暴气旋 | `storm-vortex` | 040006 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/storm-vortex.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·风击术 | `wind-strike` | 040007 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/wind-strike.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/storm.md` 恒风·大旋风术 | `great-cyclone` | 040008 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/great-cyclone.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 正十字（待定） | `upright-cross` | 010008 | DRAFT: source explicitly marks待定; excluded from runtime pool；草稿，禁用 | 退池 | text, name | 草稿，不发布 | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 逆十字（待定） | `inverted-cross` | 010009 | DRAFT: source explicitly marks待定; excluded from runtime pool；草稿，禁用 | 退池 | text, name | 草稿，不发布 | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 安卡十字（待定） | `ankh` | 010010 | DRAFT: source explicitly marks待定; excluded from runtime pool；草稿，禁用 | 退池 | text, name | 草稿，不发布 | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 恩格亚斯之星 | `engeas-four-point-star` | 010003 | CONFIRM: source/current display-name mismatch; stable ID preserved；权威结构化效果/机制 | text | name | 完整 `res://art/card/engeas-four-point-star.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 元素之环 | `elemental-ring` | 010011 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/v2-elemental-ring.png` | tests/alpha_v253_rules.cpp (evocation damage source restriction) |
| `sources/formations.md` 混沌之环 | `chaos-ring` | 010012 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/v2-chaos-ring.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 阿克乌姆之弧 | `reservoir` | 010001 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/reservoir.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 硫磺 | `sulfur` | 010013 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/v2-sulfur.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 四芒星 | `small-hexagram` | 010004 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/alpha-v252-small-hexagram.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 六芒星 | `hexagram` | 010005 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/alpha-v252-hexagram.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 五芒星 | `pentagram-of-life` | 010002 | MATCHED；权威结构化效果/机制 | 无 | 无 | 完整 `res://art/card/alpha-v252-pentagram-of-life.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 八芒星 | `octagram` | 010006 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/alpha-v252-octagram.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 太阳 | `sun` | 010007 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/alpha-v252-sun.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 三芒星 | `three-star` | 010019 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/three-star.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 二芒星 | `two-star` | 010020 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/two-star.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/formations.md` 月亮 | `moon` | 010021 | MATCHED；权威结构化效果/机制 | capacity, text | 无 | 完整 `res://art/card/moon.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/seals.md` 逆三角星符 | `conduit` | 050002 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/conduit.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/seals.md` 奥数圆环 | `ring` | 050001 | CONFIRM: source/current display-name mismatch; stable ID preserved；权威结构化效果/机制 | text | name | 完整 `res://art/card/ring.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |
| `sources/seals.md` 昂扬 | `uplift` | 050003 | MATCHED；权威结构化效果/机制 | text | 无 | 完整 `res://art/card/uplift.png` | tests/alpha_v252_pool.cpp (normal authority execution and invalid timing/cost/ownership) |

## 计数核对

| 来源 | 标题数 |
|---|---:|
| spells | 49 |
| sique | 19 |
| storm | 15 |
| formations | 16 |
| seals | 3 |
| 去重前总条目 | 102 |
| 待定不发布 | 3 |
| 正式候选（同 ID 去重） | 99 |
