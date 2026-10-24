# Alpha v2.5.4 逐卡差异报告

来源快照、原始路径及 SHA-256 见 `source-manifest.json`；更新前正式卡池见 `previous-catalog.json`。稳定 ID 和已有编号全部保留。七星星帜的二级标题按卡牌解析；七星·星移统一引用七星星移，早成修正为造成。

源条目 138；正式卡 135；新增 36；待定不发布 3。所有正式卡均有唯一 ID、唯一编号与有效插图映射。

本报告中的结构化定义状态不等同于完整实装或测试通过；规则裁定、行为验证及已知限制以本次交付报告为准。三张十字阵法保留待定草稿。

| 卡名 | 稳定 ID | 状态 | 字段变化 | 效果状态 | 图像 | 对应测试文件 |
|---|---|---|---|---|---|---|
| 火球术 | `fireball` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/fireball.png | tests/ai.cpp, tests/alpha_cards.cpp, tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_debug.cpp, tests/alpha_v253_rules.cpp, tests/alpha_v254_existing.cpp, tests/bridge.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/decks.cpp, tests/rc_content.cpp, tests/rules.cpp, tests/sique.cpp, tests/storm_ice.cpp, tests/storm_missing_effects.cpp |
| 寒霜射线 | `ray-of-frost` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/ray-of-frost.png | tests/alpha_cards.cpp, tests/alpha_v253_rules.cpp, tests/rc_content.cpp, tests/sique.cpp |
| 闪电束 | `lightning-bolt` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/lightning-bolt.png | tests/alpha_cards.cpp, tests/rc_content.cpp |
| 魔法飞弹 | `magic-missile` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/magic-missile.png | tests/alpha_cards.cpp, tests/alpha_v254_existing.cpp, tests/rc_content.cpp, tests/storm_cloud_mist.cpp, tests/storm_ice.cpp, tests/storm_missing_effects.cpp |
| 繁彩球 | `chromatic-orb` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-chromatic-orb.png | tests/alpha_v252_pool.cpp, tests/alpha_v253_rules.cpp, tests/rc_content.cpp |
| 曳光弹 | `guiding-bolt` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-guiding-bolt.png | tests/rc_content.cpp |
| 缝合生命 | `mend` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/mend.png | tests/ai.cpp, tests/alpha_cards.cpp, tests/alpha_v252_content.cpp, tests/alpha_v253_rules.cpp, tests/card_updates.cpp, tests/rc_content.cpp, tests/sique.cpp |
| 虚假生命 | `false-life` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/false-life.png | tests/alpha_cards.cpp, tests/alpha_v252_pool.cpp |
| 炽热射线 | `scorching-ray` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-scorching-ray.png | tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v253_rules.cpp, tests/rc_content.cpp |
| 炽燃结界 | `ward` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/ward.png | tests/ai.cpp, tests/alpha_cards.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/rc_content.cpp, tests/rules.cpp, tests/sique.cpp, tests/storm_modifiers.cpp |
| 阳炎爆 | `sunburst` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-sunburst.png | tests/alpha_v253_rules.cpp, tests/bridge.cpp, tests/rc_content.cpp |
| 治疗之环 | `healing-circle` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-healing-circle.png | tests/alpha_v252_content.cpp, tests/rc_content.cpp |
| 地震术 | `earthquake` | matched | 无 | 需规则内核实现/确认 | res://art/card/unravel.png | tests/alpha_v252_content.cpp, tests/alpha_v252_rules.cpp |
| 沉默术 | `instantly-sleep` | updated | name | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/instantly-sleep.png | tests/alpha_cards.cpp, tests/rc_content.cpp |
| 碎环术 | `unravel` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/unravel.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v254_existing.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/rules.cpp, tests/sique.cpp |
| 紊乱波动 | `messy-wave` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/messy-wave.png | tests/alpha_cards.cpp, tests/alpha_v252_rules.cpp |
| 次级复原术 | `lesser-restoration` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/lesser-restoration.png | tests/alpha_cards.cpp |
| 云雾术 | `cloud-mist` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/cloud-mist.png | tests/storm.cpp, tests/storm_cloud_mist.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 召雷术 | `thunder-summon` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/thunder-summon.png | tests/storm_cloud_mist.cpp |
| 皇后 | `arcana-empress` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-empress.png | tests/alpha_v252_acceptance.cpp, tests/alpha_v252_rules.cpp |
| 恋人 | `arcana-lovers` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-lovers.png | tests/alpha_v252_rules.cpp |
| 战车 | `arcana-chariot` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-chariot.png | tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v254_existing.cpp |
| 力量 | `arcana-strength` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-strength.png | tests/alpha_v252_rules.cpp |
| 隐士 | `arcana-hermit` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-hermit.png | tests/alpha_v252_rules.cpp |
| 倒吊人 | `arcana-hanged-man` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-hanged-man.png | tests/alpha_v252_rules.cpp, tests/sique.cpp |
| 死神 | `arcana-death` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-death.png | tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/alpha_v254_existing.cpp |
| 节制 | `arcana-temperance` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-temperance.png | tests/alpha_v252_rules.cpp |
| 恶魔 | `arcana-devil` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-devil.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp |
| 高塔 | `arcana-tower` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-tower.png | tests/alpha_v252_acceptance.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp |
| 火花术 | `spark` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/spark.png | tests/ai.cpp, tests/alpha_cards.cpp, tests/alpha_v252_acceptance.cpp, tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/alpha_v254_existing.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/rc_content.cpp, tests/rules.cpp, tests/sique.cpp |
| 魔力扰动 | `disrupt` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/disrupt.png | tests/alpha_v253_rules.cpp, tests/card_updates.cpp, tests/rules.cpp |
| 克敌机先 | `true-strike` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/true-strike.png | tests/alpha_cards.cpp, tests/alpha_v252_pool.cpp, tests/rc_content.cpp |
| 奥术回想 | `recall` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/recall.png | tests/ai.cpp, tests/alpha_cards.cpp, tests/alpha_v252_acceptance.cpp, tests/alpha_v252_content.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/rc_content.cpp, tests/rules.cpp, tests/sique.cpp |
| 奥术复苏 | `arcane-recovery` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcane-recovery.png | tests/alpha_cards.cpp |
| 瞬发回路过载 | `instant-circuit-overload` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/instant-circuit-overload.png | tests/alpha_cards.cpp, tests/alpha_v252_pool.cpp, tests/rc_content.cpp |
| 支援术 | `aid` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/aid.png | tests/alpha_cards.cpp, tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/rc_content.cpp, tests/sique.cpp, tests/storm_modifiers.cpp |
| 心如止水 | `clarity` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/clarity.png | tests/alpha_cards.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/rules.cpp |
| 愚者 | `arcana-fool` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-fool.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v254_existing.cpp |
| 魔术师 | `arcana-magician` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-magician.png | tests/alpha_v252_rules.cpp, tests/sique.cpp |
| 女祭司 | `arcana-priestess` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-priestess.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp |
| 皇帝 | `arcana-emperor` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-emperor.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp |
| 正义 | `arcana-justice` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-justice.png | tests/alpha_v252_rules.cpp |
| 世界 | `arcana-world` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-world.png | tests/alpha_v252_rules.cpp |
| 银光锐语 | `barbs` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/barbs.png | tests/ai.cpp, tests/alpha_cards.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/rc_content.cpp, tests/rules.cpp |
| 护盾术 | `shield` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/shield.png | tests/alpha_cards.cpp, tests/alpha_v25.cpp, tests/alpha_v252_rules.cpp, tests/rc_content.cpp |
| 法术反制 | `counter-spell` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/counter-spell.png | tests/alpha_cards.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/rc_content.cpp, tests/sique.cpp |
| 避火咒 | `protective-flame` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/protective-flame.png | tests/alpha_cards.cpp, tests/alpha_v252_rules.cpp, tests/rc_content.cpp |
| 滑稽魔盒 | `funny-box` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-funny-box.png | tests/alpha_v252_pool.cpp, tests/rc_content.cpp |
| 教皇 | `arcana-hierophant` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/arcana-hierophant.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp |
| 逆三角星符 | `conduit` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/conduit.png | tests/alpha_v252_rules.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/decks.cpp, tests/rc_content.cpp, tests/rules.cpp |
| 奥术圆环 | `ring` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/ring.png | tests/ai.cpp, tests/alpha_cards.cpp, tests/alpha_v25.cpp, tests/alpha_v252_acceptance.cpp, tests/alpha_v252_content.cpp, tests/alpha_v252_life.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_debug.cpp, tests/alpha_v253_rules.cpp, tests/alpha_v254_existing.cpp, tests/alpha_v2_resources.cpp, tests/application.cpp, tests/bridge.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/content.cpp, tests/decks.cpp, tests/network.cpp, tests/rc_content.cpp, tests/rules.cpp, tests/session.cpp, tests/sique.cpp, tests/storm.cpp, tests/storm_cloud_mist.cpp, tests/storm_ice.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 昂扬 | `uplift` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/uplift.png | tests/alpha_cards.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/rc_content.cpp |
| 正十字（待定） | `upright-cross` | draft | 无 | 待定，禁止构筑 | not published | 待补充 |
| 逆十字（待定） | `inverted-cross` | draft | 无 | 待定，禁止构筑 | not published | 待补充 |
| 安卡十字（待定） | `ankh` | draft | 无 | 待定，禁止构筑 | not published | 待补充 |
| 恩格亚斯之星 | `engeas-four-point-star` | updated | name, summonCondition, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/engeas-four-point-star.png | tests/alpha_cards.cpp, tests/alpha_v252_content.cpp, tests/rc_content.cpp |
| 元素之环 | `elemental-ring` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-elemental-ring.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/rc_content.cpp |
| 混沌之环 | `chaos-ring` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-chaos-ring.png | 待补充 |
| 阿克乌姆之弧 | `reservoir` | updated | summonCondition | 需规则内核实现/确认 | res://art/card/reservoir.png | tests/ai.cpp, tests/alpha_cards.cpp, tests/alpha_v252_acceptance.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/card_updates.cpp, tests/chain.cpp, tests/rc_content.cpp, tests/rules.cpp |
| 硫磺 | `sulfur` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/v2-sulfur.png | tests/rc_content.cpp |
| 四芒星 | `small-hexagram` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/alpha-v252-small-hexagram.png | tests/alpha_v25.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/alpha_v254_existing.cpp, tests/decks.cpp, tests/rc_content.cpp, tests/sique.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 六芒星 | `hexagram` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/alpha-v252-hexagram.png | tests/alpha_v25.cpp, tests/alpha_v252_acceptance.cpp, tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_debug.cpp, tests/alpha_v253_rules.cpp, tests/alpha_v254_existing.cpp, tests/decks.cpp, tests/rc_content.cpp, tests/sique.cpp, tests/storm.cpp, tests/storm_cloud_mist.cpp, tests/storm_ice.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 五芒星 | `pentagram-of-life` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/alpha-v252-pentagram-of-life.png | tests/alpha_cards.cpp, tests/rc_content.cpp |
| 八芒星 | `octagram` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/alpha-v252-octagram.png | tests/rc_content.cpp, tests/sique.cpp |
| 太阳 | `sun` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/alpha-v252-sun.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/alpha_v254_existing.cpp, tests/bridge.cpp, tests/rc_content.cpp |
| 三芒星 | `three-star` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/three-star.png | tests/alpha_v252_content.cpp |
| 二芒星 | `two-star` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/two-star.png | tests/alpha_v252_content.cpp |
| 月亮 | `moon` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/moon.png | tests/alpha_v252_content.cpp |
| 味契·柴堆 | `sique-woodpile` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-woodpile.png | tests/alpha_v252_rules.cpp, tests/sique.cpp |
| 味契·篝火 | `sique-campfire` | updated | text | 需规则内核实现/确认 | res://art/card/sique-campfire.png | tests/alpha_v252_acceptance.cpp, tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/sique.cpp |
| 味契·魔力坩埚 | `sique-cauldron` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-cauldron.png | tests/alpha_v252_acceptance.cpp, tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/sique.cpp |
| 味契·香醋 | `sique-vinegar` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-vinegar.png | tests/alpha_v252_acceptance.cpp, tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/sique.cpp |
| 味契·冰糖 | `sique-sugar` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-sugar.png | tests/alpha_v252_pool.cpp, tests/alpha_v254_existing.cpp, tests/sique.cpp |
| 味契·海盐 | `sique-salt` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-salt.png | tests/alpha_v252_pool.cpp, tests/sique.cpp |
| 味契·辣椒 | `sique-chili` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-chili.png | tests/alpha_v252_acceptance.cpp, tests/alpha_v252_pool.cpp, tests/sique.cpp |
| 味契·欧芹 | `sique-parsley` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-parsley.png | tests/sique.cpp |
| 味契·炙烤 | `sique-roast` | updated | school | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-roast.png | tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp |
| 味契·奶油蘑菇浓汤 | `sique-mushroom-soup` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-mushroom-soup.png | tests/sique.cpp |
| 味契·糖浆煎饼 | `sique-pancakes` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-pancakes.png | tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp, tests/sique.cpp |
| 味契·神恩炖肉 | `sique-divine-stew` | updated | speed, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-divine-stew.png | tests/alpha_v252_pool.cpp, tests/sique.cpp |
| 味契·原料召来 | `sique-ingredients` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-ingredients.png | tests/sique.cpp |
| 味契·风味转化 | `sique-flavor-shift` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-flavor-shift.png | tests/sique.cpp |
| 味契·烈火回炉 | `sique-refire` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-refire.png | tests/alpha_v254_existing.cpp, tests/sique.cpp |
| 味契·吹火术 | `sique-blowfire` | updated | school | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-blowfire.png | tests/alpha_v252_content.cpp, tests/alpha_v252_pool.cpp |
| 味契·回火 | `sique-backfire` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-backfire.png | tests/alpha_v252_content.cpp, tests/alpha_v253_rules.cpp, tests/sique.cpp |
| 味契·沸汤滚袭 | `sique-boiling-surge` | matched | 无 | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-boiling-surge.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/sique.cpp |
| 味契·飘香诱惑 | `sique-aroma-lure` | updated | text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/sique-aroma-lure.png | tests/sique.cpp |
| 风契·风暴纹章 | `storm-crest` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/storm-crest.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v253_rules.cpp, tests/alpha_v254_existing.cpp, tests/bridge.cpp, tests/rc_content.cpp, tests/storm.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 风契·风暴之心 | `storm-heart` | updated | name, summonCondition, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/storm-heart.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/alpha_v254_existing.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 风契·风暴烙印 | `storm-brand` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/storm-brand.png | tests/storm_modifiers.cpp |
| 风契·湮灭之纹章 | `storm-annihilation-crest` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/storm-annihilation-crest.png | tests/alpha_v253_rules.cpp, tests/storm_modifiers.cpp |
| 风契·冰风暴 | `ice-storm` | updated | name, tags | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/ice-storm.png | tests/alpha_v252_content.cpp, tests/storm_ice.cpp |
| 风契·切割之风 | `cutting-wind` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/cutting-wind.png | tests/alpha_v253_rules.cpp, tests/storm_missing_effects.cpp |
| 风契·大风暴术 | `great-storm` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/great-storm.png | tests/alpha_v252_pool.cpp, tests/alpha_v254_existing.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 风契·风语术 | `wind-whisper` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/wind-whisper.png | tests/rc_content.cpp, tests/storm.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 风契·极速之风 | `swift-wind` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/swift-wind.png | tests/rc_content.cpp |
| 风契·造风术 | `windmaking` | updated | name, tags | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/windmaking.png | tests/storm_missing_effects.cpp |
| 风契·风暴导向 | `storm-guidance` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/storm-guidance.png | tests/alpha_v252_rules.cpp, tests/alpha_v254_existing.cpp, tests/rc_content.cpp, tests/storm_missing_effects.cpp |
| 风契·雪雨暴 | `snow-rain-storm` | updated | name, tags | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/snow-rain-storm.png | tests/alpha_v252_content.cpp, tests/alpha_v252_rules.cpp, tests/storm_modifiers.cpp |
| 风契·风暴气旋 | `storm-vortex` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/storm-vortex.png | tests/alpha_v252_rules.cpp, tests/rc_content.cpp, tests/storm.cpp, tests/storm_missing_effects.cpp, tests/storm_modifiers.cpp |
| 风契·风击术 | `wind-strike` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/wind-strike.png | tests/alpha_v252_pool.cpp, tests/alpha_v252_rules.cpp, tests/rc_content.cpp, tests/storm.cpp, tests/storm_missing_effects.cpp |
| 风契·大旋风术 | `great-cyclone` | updated | name, tags, text | 结构化效果/机制已定义，行为验收见测试报告 | res://art/card/great-cyclone.png | tests/rc_content.cpp, tests/storm_missing_effects.cpp |
| 七星·天枢 | `qixing-tianshu` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, type | 需规则内核实现/确认 | res://art/cards/qixing-tianshu.jpg | 待补充 |
| 七星·天璇 | `qixing-tianxuan` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-tianxuan.jpg | 待补充 |
| 七星·天玑 | `qixing-tianji` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-tianji.jpg | 待补充 |
| 七星·天权 | `qixing-tianquan` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-tianquan.jpg | 待补充 |
| 七星·玉衡 | `qixing-yuheng` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-yuheng.jpg | 待补充 |
| 七星·开阳 | `qixing-kaiyang` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-kaiyang.jpg | 待补充 |
| 七星·瑶光 | `qixing-yaoguang` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-yaoguang.jpg | 待补充 |
| 七星星帜 | `qixing-banner` | new | castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-banner.jpg | 待补充 |
| 七星灯盏 | `qixing-lamp` | new | analysisTurns, castCost, cost, duration, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-lamp.jpg | 待补充 |
| 七星归墟 | `qixing-ruin` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-ruin.jpg | 待补充 |
| 七星启曜 | `qixing-awaken` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-awaken.jpg | 待补充 |
| 七星遥鉴 | `qixing-divination` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-divination.jpg | 待补充 |
| 七星星移 | `qixing-shift` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-shift.jpg | 待补充 |
| 七星的注视 | `qixing-gaze` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, responseOnly, responses, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-gaze.jpg | 待补充 |
| 七星惩戒 | `qixing-punishment` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-punishment.jpg | 待补充 |
| 七星归迹 | `qixing-return` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/qixing-return.jpg | 待补充 |
| 血契·祭坛 | `blood-altar` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-altar.png | 待补充 |
| 血契·输血管 | `blood-transfusion` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-transfusion.png | 待补充 |
| 血契·祭品台 | `blood-sacrificial-table` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-sacrificial-table.png | 待补充 |
| 血契·血池 | `blood-pool` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-pool.png | 待补充 |
| 血契·心脏 | `blood-heart` | new | baseEligible, body, capacity, castCost, cost, effects, id, illustrationId, income, level, maxRank, name, number, rank, rarity, recipe, rings, speed, summonCondition, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-heart.png | 待补充 |
| 血契·血印 | `blood-mark` | new | castCost, cost, effects, id, illustrationId, incomeBonus, level, name, number, rank, rarity, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-mark.png | 待补充 |
| 血契·血脉 | `blood-vein` | new | castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-vein.png | 待补充 |
| 血契·血咒 | `blood-curse` | new | castCost, cost, effects, id, illustrationId, level, name, number, rank, rankBonus, rarity, ringBonus, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-curse.png | 待补充 |
| 血契·心印 | `blood-heartmark` | new | castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-heartmark.png | 待补充 |
| 血契·血刃 | `blood-blade` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-blade.png | 待补充 |
| 血契·血债 | `blood-debt` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-debt.png | 待补充 |
| 血契·续命 | `blood-renewal` | new | analysisTurns, castCost, cost, duration, effects, id, illustrationId, level, name, number, onEnd, rank, rarity, school, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-renewal.png | 待补充 |
| 血契·血偿 | `blood-repayment` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-repayment.png | 待补充 |
| 血契·放血 | `blood-letting` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-letting.png | 待补充 |
| 血契·割腕 | `blood-wrist` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-wrist.png | 待补充 |
| 血契·祭品 | `blood-offering` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-offering.png | 待补充 |
| 血契·背水 | `blood-last-stand` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-last-stand.png | 待补充 |
| 血契·凝血 | `blood-coagulation` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, responseOnly, responses, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-coagulation.png | 待补充 |
| 血契·血雨 | `blood-rain` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, rank, rarity, school, speed, tags, target, text, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-rain.png | 待补充 |
| 血契·同源 | `blood-same-origin` | new | analysisTurns, castCost, cost, effects, id, illustrationId, level, name, number, perpetual, rank, rarity, school, speed, tags, target, text, traits, type | 结构化效果/机制已定义，行为验收见测试报告 | res://art/cards/blood-same-origin.png | 待补充 |

## 来源计数

| 来源 | 张数 |
|---|---:|
| spells | 49 |
| seals | 3 |
| formations | 16 |
| sique | 19 |
| storm | 15 |
| qixing | 16 |
| blood | 20 |
