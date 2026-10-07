# A 古籍金饰 · 美术资源清单

用户已选 A 版；本批重制卡牌组件并细化其余 UI。未重排战场，也未接入游戏。

共 **314 个独立 PNG**，其中十二张原始生成主素材；另附 30 卡排版预览、四张总览、可编辑 SVG、参数清单、提示词和生成脚本。

|分类|数量|
|---|---:|
|icons|122|
|cards|58|
|interaction|64|
|board|37|
|rarity|15|
|materials|1|
|ornaments|5|
|painted|12|

## 本轮内容

- 五类独立主卡框：行动闪电与箭形镶边、解析法术卷册徽章、言灵银羽角饰、阵法六芒星徽章、符文圆环链扣。所有主框均无烘焙文字、数值和插画。
- 25 个可编辑固定尺寸卡框：五种类型各有手牌、短条、场内小卡、详情及完整模板；配有独立类型纹章、属性槽、条件标签和六类交互叠加。
- 五档宝石稀有度：银灰菱晶、翠绿菱晶、蓝色水滴、紫色星芒、金黄色宝石冠饰；镶座与名称容器分别导出，1—5 刻度独立，不绑定卡牌类型。
- 一张原始绘制通用卡背及三个可编辑缩略卡背。卡背不携带类型或稀有度等私有信息。
- 其余组件重绘金属分层、皮革缝线、古金卷饰和切面宝石：区域框／标题、按钮四状态、HUD、资源条、弹窗、反馈、阶段、滚动、步骤和图标。
- 施法区、言灵区、解析区、行动区、灰烬区均有独立框体和标题，双方可复用；另含阵法、手牌、牌库。
- 补充速度、临时／绑定／独立荷载、仅响应、伏击、专注和八个学派图标。
- 保留上一批可选羊皮纸底图；它是唯一直接复用的材料资源。既有卡牌插画未重画。

## 使用与限制

原始绘制主框为 1024×1536，窗口为 1536×1024。生成 PNG 完整原样保存，不做裁切、改色或像素加工。透明通道保留原始边缘覆盖率；代码原生组件为硬边像素。原始绘制图的细节较多，需用实际尺寸确认小屏阅读效果。
主框和卡背固定比例缩放，禁止九宫格拉伸；可编辑小卡框也固定尺寸。按钮和窗口等代码原生组件按 manifest 的 stretch/insets 使用。生成主窗口同样固定比例，不宣称可任意九宫格拉伸。
五张主框内容安全区按输出保守设置，见各 master_* 的 layout。卡名、费用、规则、插画、稀有度需要程序单独叠加；预览 PNG 只是信息组合展示，不应作运行时卡面。
属性字段以 assets/cards.json 为准。解析回合缺省 1，显示解析费用与施法费用分别支付；规则文字全部保留；不存在的属性不造数值。详情可用滚动区域显示风味文字和动态实例信息。
所有 30 张卡已做标题、正文与属性数检查。透明窗采样、源图 SHA256、SVG 链接与独立组件数校验见 validation.json。没有进行游戏实机验收，因为本批未接入。

## 原始绘制提示词

内置 imagegen 制作了五张主框、通用卡背、主窗口与五档精绘稀有度徽章；完整提示词存放于 prompts/，来源存放于 generated-sources.json 与 generated-supplement.json。其余组件通过当前项目可编辑绘制系统制作。

## 逐项清单

路径相对于 assets/art。

|分类 / ID|尺寸|文件|用途|
|---|---|---|---|
|icons / type_action|32×32|`ui/a-gilded-v2/icons/type_action.png`|行动|
|icons / type_action_64|64×64|`ui/a-gilded-v2/icons/type_action_64.png`|行动 2×|
|icons / type_analytic|32×32|`ui/a-gilded-v2/icons/type_analytic.png`|解析法术|
|icons / type_analytic_64|64×64|`ui/a-gilded-v2/icons/type_analytic_64.png`|解析法术 2×|
|icons / type_word|32×32|`ui/a-gilded-v2/icons/type_word.png`|言灵|
|icons / type_word_64|64×64|`ui/a-gilded-v2/icons/type_word_64.png`|言灵 2×|
|icons / type_formation|32×32|`ui/a-gilded-v2/icons/type_formation.png`|阵法|
|icons / type_formation_64|64×64|`ui/a-gilded-v2/icons/type_formation_64.png`|阵法 2×|
|icons / type_seal|32×32|`ui/a-gilded-v2/icons/type_seal.png`|符文|
|icons / type_seal_64|64×64|`ui/a-gilded-v2/icons/type_seal_64.png`|符文 2×|
|icons / rank|32×32|`ui/a-gilded-v2/icons/rank.png`|位阶|
|icons / rank_64|64×64|`ui/a-gilded-v2/icons/rank_64.png`|位阶 2×|
|icons / analysis_cost|32×32|`ui/a-gilded-v2/icons/analysis_cost.png`|解析费用|
|icons / analysis_cost_64|64×64|`ui/a-gilded-v2/icons/analysis_cost_64.png`|解析费用 2×|
|icons / cast_cost|32×32|`ui/a-gilded-v2/icons/cast_cost.png`|施法费用|
|icons / cast_cost_64|64×64|`ui/a-gilded-v2/icons/cast_cost_64.png`|施法费用 2×|
|icons / body|32×32|`ui/a-gilded-v2/icons/body.png`|阵体|
|icons / body_64|64×64|`ui/a-gilded-v2/icons/body_64.png`|阵体 2×|
|icons / ring_slot|32×32|`ui/a-gilded-v2/icons/ring_slot.png`|环位|
|icons / ring_slot_64|64×64|`ui/a-gilded-v2/icons/ring_slot_64.png`|环位 2×|
|icons / mana_income|32×32|`ui/a-gilded-v2/icons/mana_income.png`|魔素收入|
|icons / mana_income_64|64×64|`ui/a-gilded-v2/icons/mana_income_64.png`|魔素收入 2×|
|icons / duration|32×32|`ui/a-gilded-v2/icons/duration.png`|持续计数|
|icons / duration_64|64×64|`ui/a-gilded-v2/icons/duration_64.png`|持续计数 2×|
|icons / state_analyzing|32×32|`ui/a-gilded-v2/icons/state_analyzing.png`|解析中|
|icons / state_analyzing_64|64×64|`ui/a-gilded-v2/icons/state_analyzing_64.png`|解析中 2×|
|icons / state_analyzed|32×32|`ui/a-gilded-v2/icons/state_analyzed.png`|解析完成|
|icons / state_analyzed_64|64×64|`ui/a-gilded-v2/icons/state_analyzed_64.png`|解析完成 2×|
|icons / state_prepared|32×32|`ui/a-gilded-v2/icons/state_prepared.png`|待施放|
|icons / state_prepared_64|64×64|`ui/a-gilded-v2/icons/state_prepared_64.png`|待施放 2×|
|icons / state_active|32×32|`ui/a-gilded-v2/icons/state_active.png`|持续生效|
|icons / state_active_64|64×64|`ui/a-gilded-v2/icons/state_active_64.png`|持续生效 2×|
|icons / state_cancelled|32×32|`ui/a-gilded-v2/icons/state_cancelled.png`|准备被取消|
|icons / state_cancelled_64|64×64|`ui/a-gilded-v2/icons/state_cancelled_64.png`|准备被取消 2×|
|icons / state_protected|32×32|`ui/a-gilded-v2/icons/state_protected.png`|基础阵法保护|
|icons / state_protected_64|64×64|`ui/a-gilded-v2/icons/state_protected_64.png`|基础阵法保护 2×|
|icons / state_attached|32×32|`ui/a-gilded-v2/icons/state_attached.png`|符文附着|
|icons / state_attached_64|64×64|`ui/a-gilded-v2/icons/state_attached_64.png`|符文附着 2×|
|icons / life|32×32|`ui/a-gilded-v2/icons/life.png`|生命|
|icons / life_64|64×64|`ui/a-gilded-v2/icons/life_64.png`|生命 2×|
|icons / mana|32×32|`ui/a-gilded-v2/icons/mana.png`|魔素|
|icons / mana_64|64×64|`ui/a-gilded-v2/icons/mana_64.png`|魔素 2×|
|icons / load|32×32|`ui/a-gilded-v2/icons/load.png`|当前荷载|
|icons / load_64|64×64|`ui/a-gilded-v2/icons/load_64.png`|当前荷载 2×|
|icons / capacity|32×32|`ui/a-gilded-v2/icons/capacity.png`|荷载容量|
|icons / capacity_64|64×64|`ui/a-gilded-v2/icons/capacity_64.png`|荷载容量 2×|
|icons / zone_formation|32×32|`ui/a-gilded-v2/icons/zone_formation.png`|阵法|
|icons / zone_formation_64|64×64|`ui/a-gilded-v2/icons/zone_formation_64.png`|阵法 2×|
|icons / zone_analysis|32×32|`ui/a-gilded-v2/icons/zone_analysis.png`|解析|
|icons / zone_analysis_64|64×64|`ui/a-gilded-v2/icons/zone_analysis_64.png`|解析 2×|
|icons / zone_casting|32×32|`ui/a-gilded-v2/icons/zone_casting.png`|施法|
|icons / zone_casting_64|64×64|`ui/a-gilded-v2/icons/zone_casting_64.png`|施法 2×|
|icons / zone_words|32×32|`ui/a-gilded-v2/icons/zone_words.png`|言灵|
|icons / zone_words_64|64×64|`ui/a-gilded-v2/icons/zone_words_64.png`|言灵 2×|
|icons / zone_action|32×32|`ui/a-gilded-v2/icons/zone_action.png`|行动|
|icons / zone_action_64|64×64|`ui/a-gilded-v2/icons/zone_action_64.png`|行动 2×|
|icons / zone_ash|32×32|`ui/a-gilded-v2/icons/zone_ash.png`|灰烬|
|icons / zone_ash_64|64×64|`ui/a-gilded-v2/icons/zone_ash_64.png`|灰烬 2×|
|icons / zone_hand|32×32|`ui/a-gilded-v2/icons/zone_hand.png`|手牌|
|icons / zone_hand_64|64×64|`ui/a-gilded-v2/icons/zone_hand_64.png`|手牌 2×|
|icons / zone_deck|32×32|`ui/a-gilded-v2/icons/zone_deck.png`|牌库|
|icons / zone_deck_64|64×64|`ui/a-gilded-v2/icons/zone_deck_64.png`|牌库 2×|
|icons / close|32×32|`ui/a-gilded-v2/icons/close.png`|关闭|
|icons / close_64|64×64|`ui/a-gilded-v2/icons/close_64.png`|关闭 2×|
|icons / confirm|32×32|`ui/a-gilded-v2/icons/confirm.png`|确认|
|icons / confirm_64|64×64|`ui/a-gilded-v2/icons/confirm_64.png`|确认 2×|
|icons / cancel|32×32|`ui/a-gilded-v2/icons/cancel.png`|取消|
|icons / cancel_64|64×64|`ui/a-gilded-v2/icons/cancel_64.png`|取消 2×|
|icons / left|32×32|`ui/a-gilded-v2/icons/left.png`|向前滚动|
|icons / left_64|64×64|`ui/a-gilded-v2/icons/left_64.png`|向前滚动 2×|
|icons / right|32×32|`ui/a-gilded-v2/icons/right.png`|向后滚动|
|icons / right_64|64×64|`ui/a-gilded-v2/icons/right_64.png`|向后滚动 2×|
|icons / up|32×32|`ui/a-gilded-v2/icons/up.png`|向上|
|icons / up_64|64×64|`ui/a-gilded-v2/icons/up_64.png`|向上 2×|
|icons / down|32×32|`ui/a-gilded-v2/icons/down.png`|向下|
|icons / down_64|64×64|`ui/a-gilded-v2/icons/down_64.png`|向下 2×|
|icons / save|32×32|`ui/a-gilded-v2/icons/save.png`|保存|
|icons / save_64|64×64|`ui/a-gilded-v2/icons/save_64.png`|保存 2×|
|icons / log|32×32|`ui/a-gilded-v2/icons/log.png`|记录|
|icons / log_64|64×64|`ui/a-gilded-v2/icons/log_64.png`|记录 2×|
|icons / surrender|32×32|`ui/a-gilded-v2/icons/surrender.png`|投降|
|icons / surrender_64|64×64|`ui/a-gilded-v2/icons/surrender_64.png`|投降 2×|
|icons / success|32×32|`ui/a-gilded-v2/icons/success.png`|成功|
|icons / success_64|64×64|`ui/a-gilded-v2/icons/success_64.png`|成功 2×|
|icons / no_action|32×32|`ui/a-gilded-v2/icons/no_action.png`|无操作|
|icons / no_action_64|64×64|`ui/a-gilded-v2/icons/no_action_64.png`|无操作 2×|
|icons / wrong_target|32×32|`ui/a-gilded-v2/icons/wrong_target.png`|选错目标|
|icons / wrong_target_64|64×64|`ui/a-gilded-v2/icons/wrong_target_64.png`|选错目标 2×|
|icons / rejected|32×32|`ui/a-gilded-v2/icons/rejected.png`|操作拒绝|
|icons / rejected_64|64×64|`ui/a-gilded-v2/icons/rejected_64.png`|操作拒绝 2×|
|icons / eye|32×32|`ui/a-gilded-v2/icons/eye.png`|查看|
|icons / eye_64|64×64|`ui/a-gilded-v2/icons/eye_64.png`|查看 2×|
|icons / speed|32×32|`ui/a-gilded-v2/icons/speed.png`|速度|
|icons / speed_64|64×64|`ui/a-gilded-v2/icons/speed_64.png`|速度 2×|
|icons / temporary_load|32×32|`ui/a-gilded-v2/icons/temporary_load.png`|临时荷载|
|icons / temporary_load_64|64×64|`ui/a-gilded-v2/icons/temporary_load_64.png`|临时荷载 2×|
|icons / bound_load|32×32|`ui/a-gilded-v2/icons/bound_load.png`|绑定荷载|
|icons / bound_load_64|64×64|`ui/a-gilded-v2/icons/bound_load_64.png`|绑定荷载 2×|
|icons / independent_load|32×32|`ui/a-gilded-v2/icons/independent_load.png`|独立荷载|
|icons / independent_load_64|64×64|`ui/a-gilded-v2/icons/independent_load_64.png`|独立荷载 2×|
|icons / response_only|32×32|`ui/a-gilded-v2/icons/response_only.png`|仅响应|
|icons / response_only_64|64×64|`ui/a-gilded-v2/icons/response_only_64.png`|仅响应 2×|
|icons / ambush|32×32|`ui/a-gilded-v2/icons/ambush.png`|伏击|
|icons / ambush_64|64×64|`ui/a-gilded-v2/icons/ambush_64.png`|伏击 2×|
|icons / concentration|32×32|`ui/a-gilded-v2/icons/concentration.png`|专注|
|icons / concentration_64|64×64|`ui/a-gilded-v2/icons/concentration_64.png`|专注 2×|
|icons / school_evocation|32×32|`ui/a-gilded-v2/icons/school_evocation.png`|塑能学派|
|icons / school_evocation_64|64×64|`ui/a-gilded-v2/icons/school_evocation_64.png`|塑能学派 2×|
|icons / school_abjuration|32×32|`ui/a-gilded-v2/icons/school_abjuration.png`|防护学派|
|icons / school_abjuration_64|64×64|`ui/a-gilded-v2/icons/school_abjuration_64.png`|防护学派 2×|
|icons / school_necromancy|32×32|`ui/a-gilded-v2/icons/school_necromancy.png`|死灵学派|
|icons / school_necromancy_64|64×64|`ui/a-gilded-v2/icons/school_necromancy_64.png`|死灵学派 2×|
|icons / school_transmutation|32×32|`ui/a-gilded-v2/icons/school_transmutation.png`|变化学派|
|icons / school_transmutation_64|64×64|`ui/a-gilded-v2/icons/school_transmutation_64.png`|变化学派 2×|
|icons / school_enchantment|32×32|`ui/a-gilded-v2/icons/school_enchantment.png`|惑控学派|
|icons / school_enchantment_64|64×64|`ui/a-gilded-v2/icons/school_enchantment_64.png`|惑控学派 2×|
|icons / school_divination|32×32|`ui/a-gilded-v2/icons/school_divination.png`|预言学派|
|icons / school_divination_64|64×64|`ui/a-gilded-v2/icons/school_divination_64.png`|预言学派 2×|
|icons / school_illusion|32×32|`ui/a-gilded-v2/icons/school_illusion.png`|幻术学派|
|icons / school_illusion_64|64×64|`ui/a-gilded-v2/icons/school_illusion_64.png`|幻术学派 2×|
|icons / school_conjuration|32×32|`ui/a-gilded-v2/icons/school_conjuration.png`|咒法学派|
|icons / school_conjuration_64|64×64|`ui/a-gilded-v2/icons/school_conjuration_64.png`|咒法学派 2×|
|cards / frame_action_hand|101×118|`ui/a-gilded-v2/cards/frame_action_hand.png`|行动 hand|
|cards / frame_action_strip|125×44|`ui/a-gilded-v2/cards/frame_action_strip.png`|行动 strip|
|cards / frame_action_portrait|90×112|`ui/a-gilded-v2/cards/frame_action_portrait.png`|行动 portrait|
|cards / frame_action_detail|215×354|`ui/a-gilded-v2/cards/frame_action_detail.png`|行动 detail|
|cards / frame_analytic_hand|101×118|`ui/a-gilded-v2/cards/frame_analytic_hand.png`|解析法术 hand|
|cards / frame_analytic_strip|125×44|`ui/a-gilded-v2/cards/frame_analytic_strip.png`|解析法术 strip|
|cards / frame_analytic_portrait|90×112|`ui/a-gilded-v2/cards/frame_analytic_portrait.png`|解析法术 portrait|
|cards / frame_analytic_detail|215×354|`ui/a-gilded-v2/cards/frame_analytic_detail.png`|解析法术 detail|
|cards / frame_word_hand|101×118|`ui/a-gilded-v2/cards/frame_word_hand.png`|言灵 hand|
|cards / frame_word_strip|125×44|`ui/a-gilded-v2/cards/frame_word_strip.png`|言灵 strip|
|cards / frame_word_portrait|90×112|`ui/a-gilded-v2/cards/frame_word_portrait.png`|言灵 portrait|
|cards / frame_word_detail|215×354|`ui/a-gilded-v2/cards/frame_word_detail.png`|言灵 detail|
|cards / frame_formation_hand|101×118|`ui/a-gilded-v2/cards/frame_formation_hand.png`|阵法 hand|
|cards / frame_formation_strip|125×44|`ui/a-gilded-v2/cards/frame_formation_strip.png`|阵法 strip|
|cards / frame_formation_portrait|90×112|`ui/a-gilded-v2/cards/frame_formation_portrait.png`|阵法 portrait|
|cards / frame_formation_detail|215×354|`ui/a-gilded-v2/cards/frame_formation_detail.png`|阵法 detail|
|cards / frame_seal_hand|101×118|`ui/a-gilded-v2/cards/frame_seal_hand.png`|符文 hand|
|cards / frame_seal_strip|125×44|`ui/a-gilded-v2/cards/frame_seal_strip.png`|符文 strip|
|cards / frame_seal_portrait|90×112|`ui/a-gilded-v2/cards/frame_seal_portrait.png`|符文 portrait|
|cards / frame_seal_detail|215×354|`ui/a-gilded-v2/cards/frame_seal_detail.png`|符文 detail|
|cards / rarity_common|42×12|`ui/a-gilded-v2/cards/rarity_common.png`|稀有度独立标记；仍需动态文字|
|cards / rarity_uncommon|42×12|`ui/a-gilded-v2/cards/rarity_uncommon.png`|稀有度独立标记；仍需动态文字|
|cards / rarity_rare|42×12|`ui/a-gilded-v2/cards/rarity_rare.png`|稀有度独立标记；仍需动态文字|
|cards / rarity_epic|42×12|`ui/a-gilded-v2/cards/rarity_epic.png`|稀有度独立标记；仍需动态文字|
|cards / rarity_legendary|42×12|`ui/a-gilded-v2/cards/rarity_legendary.png`|稀有度独立标记；仍需动态文字|
|cards / card_back_hand|101×118|`ui/a-gilded-v2/cards/card_back_hand.png`|对称通用卡背；不携带私有信息|
|cards / card_back_opponent|39×41|`ui/a-gilded-v2/cards/card_back_opponent.png`|对称通用卡背；不携带私有信息|
|cards / card_back_deck|100×132|`ui/a-gilded-v2/cards/card_back_deck.png`|对称通用卡背；不携带私有信息|
|cards / missing_art|64×64|`ui/a-gilded-v2/cards/missing_art.png`|缺图统一回退纹章|
|interaction / button_primary_normal|160×36|`ui/a-gilded-v2/interaction/button_primary_normal.png`|primary / normal|
|interaction / button_primary_hover|160×36|`ui/a-gilded-v2/interaction/button_primary_hover.png`|primary / hover|
|interaction / button_primary_pressed|160×36|`ui/a-gilded-v2/interaction/button_primary_pressed.png`|primary / pressed|
|interaction / button_primary_disabled|160×36|`ui/a-gilded-v2/interaction/button_primary_disabled.png`|primary / disabled|
|interaction / button_secondary_normal|160×36|`ui/a-gilded-v2/interaction/button_secondary_normal.png`|secondary / normal|
|interaction / button_secondary_hover|160×36|`ui/a-gilded-v2/interaction/button_secondary_hover.png`|secondary / hover|
|interaction / button_secondary_pressed|160×36|`ui/a-gilded-v2/interaction/button_secondary_pressed.png`|secondary / pressed|
|interaction / button_secondary_disabled|160×36|`ui/a-gilded-v2/interaction/button_secondary_disabled.png`|secondary / disabled|
|interaction / button_danger_normal|160×36|`ui/a-gilded-v2/interaction/button_danger_normal.png`|danger / normal|
|interaction / button_danger_hover|160×36|`ui/a-gilded-v2/interaction/button_danger_hover.png`|danger / hover|
|interaction / button_danger_pressed|160×36|`ui/a-gilded-v2/interaction/button_danger_pressed.png`|danger / pressed|
|interaction / button_danger_disabled|160×36|`ui/a-gilded-v2/interaction/button_danger_disabled.png`|danger / disabled|
|interaction / overlay_hand_hover|101×118|`ui/a-gilded-v2/interaction/overlay_hand_hover.png`|hover|
|interaction / overlay_hand_selected|101×118|`ui/a-gilded-v2/interaction/overlay_hand_selected.png`|selected|
|interaction / overlay_hand_target_candidate|101×118|`ui/a-gilded-v2/interaction/overlay_hand_target_candidate.png`|target_candidate|
|interaction / overlay_hand_target_selected|101×118|`ui/a-gilded-v2/interaction/overlay_hand_target_selected.png`|target_selected|
|interaction / overlay_hand_cost_candidate|101×118|`ui/a-gilded-v2/interaction/overlay_hand_cost_candidate.png`|cost_candidate|
|interaction / overlay_hand_cost_selected|101×118|`ui/a-gilded-v2/interaction/overlay_hand_cost_selected.png`|cost_selected|
|interaction / overlay_strip_hover|125×44|`ui/a-gilded-v2/interaction/overlay_strip_hover.png`|hover|
|interaction / overlay_strip_selected|125×44|`ui/a-gilded-v2/interaction/overlay_strip_selected.png`|selected|
|interaction / overlay_strip_target_candidate|125×44|`ui/a-gilded-v2/interaction/overlay_strip_target_candidate.png`|target_candidate|
|interaction / overlay_strip_target_selected|125×44|`ui/a-gilded-v2/interaction/overlay_strip_target_selected.png`|target_selected|
|interaction / overlay_strip_cost_candidate|125×44|`ui/a-gilded-v2/interaction/overlay_strip_cost_candidate.png`|cost_candidate|
|interaction / overlay_strip_cost_selected|125×44|`ui/a-gilded-v2/interaction/overlay_strip_cost_selected.png`|cost_selected|
|interaction / overlay_portrait_hover|90×112|`ui/a-gilded-v2/interaction/overlay_portrait_hover.png`|hover|
|interaction / overlay_portrait_selected|90×112|`ui/a-gilded-v2/interaction/overlay_portrait_selected.png`|selected|
|interaction / overlay_portrait_target_candidate|90×112|`ui/a-gilded-v2/interaction/overlay_portrait_target_candidate.png`|target_candidate|
|interaction / overlay_portrait_target_selected|90×112|`ui/a-gilded-v2/interaction/overlay_portrait_target_selected.png`|target_selected|
|interaction / overlay_portrait_cost_candidate|90×112|`ui/a-gilded-v2/interaction/overlay_portrait_cost_candidate.png`|cost_candidate|
|interaction / overlay_portrait_cost_selected|90×112|`ui/a-gilded-v2/interaction/overlay_portrait_cost_selected.png`|cost_selected|
|board / region_formation|106×52|`ui/a-gilded-v2/board/region_formation.png`|区域框体：formation|
|board / region_analysis|427×52|`ui/a-gilded-v2/board/region_analysis.png`|区域框体：analysis|
|board / region_casting|1080×65|`ui/a-gilded-v2/board/region_casting.png`|区域框体：casting|
|board / region_words|531×106|`ui/a-gilded-v2/board/region_words.png`|区域框体：words|
|board / region_action|531×49|`ui/a-gilded-v2/board/region_action.png`|区域框体：action|
|board / region_ash|531×101|`ui/a-gilded-v2/board/region_ash.png`|区域框体：ash|
|board / region_hand|1080×134|`ui/a-gilded-v2/board/region_hand.png`|区域框体：hand|
|board / region_deck|100×132|`ui/a-gilded-v2/board/region_deck.png`|区域框体：deck|
|board / slot_empty|106×52|`ui/a-gilded-v2/board/slot_empty.png`|阵法槽状态|
|board / slot_occupied|106×52|`ui/a-gilded-v2/board/slot_occupied.png`|阵法槽状态|
|board / slot_span|106×107|`ui/a-gilded-v2/board/slot_span.png`|阵法槽状态|
|board / host_connector|12×52|`ui/a-gilded-v2/board/host_connector.png`|宿主到解析行关联|
|board / hud|215×190|`ui/a-gilded-v2/board/hud.png`|hud|
|board / phase_bar|700×42|`ui/a-gilded-v2/board/phase_bar.png`|phase_bar|
|interaction / task_hint|800×36|`ui/a-gilded-v2/interaction/task_hint.png`|task_hint|
|interaction / action_bar|672×44|`ui/a-gilded-v2/interaction/action_bar.png`|action_bar|
|interaction / summary|215×88|`ui/a-gilded-v2/interaction/summary.png`|summary|
|interaction / decision|540×310|`ui/a-gilded-v2/interaction/decision.png`|decision|
|interaction / log_window|620×420|`ui/a-gilded-v2/interaction/log_window.png`|log_window|
|interaction / result_window|520×340|`ui/a-gilded-v2/interaction/result_window.png`|result_window|
|interaction / handoff|1080×134|`ui/a-gilded-v2/interaction/handoff.png`|handoff|
|board / phase_inactive|22×22|`ui/a-gilded-v2/board/phase_inactive.png`|阶段指示 inactive|
|board / phase_current|22×22|`ui/a-gilded-v2/board/phase_current.png`|阶段指示 current|
|board / phase_complete|22×22|`ui/a-gilded-v2/board/phase_complete.png`|阶段指示 complete|
|board / scroll_track|80×6|`ui/a-gilded-v2/board/scroll_track.png`|滚动位置提示|
|board / scroll_thumb|80×6|`ui/a-gilded-v2/board/scroll_thumb.png`|滚动位置提示|
|interaction / result_victory|96×64|`ui/a-gilded-v2/interaction/result_victory.png`|victory结果标识|
|interaction / result_defeat|96×64|`ui/a-gilded-v2/interaction/result_defeat.png`|defeat结果标识|
|interaction / result_draw|96×64|`ui/a-gilded-v2/interaction/result_draw.png`|draw结果标识|
|interaction / feedback_success|360×48|`ui/a-gilded-v2/interaction/feedback_success.png`|success反馈|
|interaction / feedback_no_action|360×48|`ui/a-gilded-v2/interaction/feedback_no_action.png`|no_action反馈|
|interaction / feedback_wrong_target|360×48|`ui/a-gilded-v2/interaction/feedback_wrong_target.png`|wrong_target反馈|
|interaction / feedback_rejected|360×48|`ui/a-gilded-v2/interaction/feedback_rejected.png`|rejected反馈|
|interaction / stepper_0|480×32|`ui/a-gilded-v2/interaction/stepper_0.png`|查看/目标/额外成本/确认步骤底板；文字动态绘制|
|interaction / stepper_1|480×32|`ui/a-gilded-v2/interaction/stepper_1.png`|查看/目标/额外成本/确认步骤底板；文字动态绘制|
|interaction / stepper_2|480×32|`ui/a-gilded-v2/interaction/stepper_2.png`|查看/目标/额外成本/确认步骤底板；文字动态绘制|
|interaction / stepper_3|480×32|`ui/a-gilded-v2/interaction/stepper_3.png`|查看/目标/额外成本/确认步骤底板；文字动态绘制|
|board / match_background|1600×1000|`ui/a-gilded-v2/board/match_background.png`|低对比无文字场地底图|
|board / board_decoration|1600×1000|`ui/a-gilded-v2/board/board_decoration.png`|独立中央装饰层；不定义槽位|
|rarity / rarity_badge_common|64×40|`ui/a-gilded-v2/rarity/rarity_badge_common.png`|普通：独立轮廓＋颜色＋1枚刻度|
|rarity / rarity_badge_uncommon|64×40|`ui/a-gilded-v2/rarity/rarity_badge_uncommon.png`|罕见：独立轮廓＋颜色＋2枚刻度|
|rarity / rarity_badge_rare|64×40|`ui/a-gilded-v2/rarity/rarity_badge_rare.png`|稀有：独立轮廓＋颜色＋3枚刻度|
|rarity / rarity_badge_epic|64×40|`ui/a-gilded-v2/rarity/rarity_badge_epic.png`|史诗：独立轮廓＋颜色＋4枚刻度|
|rarity / rarity_badge_legendary|64×40|`ui/a-gilded-v2/rarity/rarity_badge_legendary.png`|传说：独立轮廓＋颜色＋5枚刻度|
|cards / frame_action_full|360×560|`ui/a-gilded-v2/cards/frame_action_full.png`|行动完整模板|
|cards / frame_analytic_full|360×560|`ui/a-gilded-v2/cards/frame_analytic_full.png`|解析法术完整模板|
|cards / frame_word_full|360×560|`ui/a-gilded-v2/cards/frame_word_full.png`|言灵完整模板|
|cards / frame_formation_full|360×560|`ui/a-gilded-v2/cards/frame_formation_full.png`|阵法完整模板|
|cards / frame_seal_full|360×560|`ui/a-gilded-v2/cards/frame_seal_full.png`|符文完整模板|
|cards / type_header_action|180×34|`ui/a-gilded-v2/cards/type_header_action.png`|类型标题条；文字动态叠加|
|cards / type_header_analytic|180×34|`ui/a-gilded-v2/cards/type_header_analytic.png`|类型标题条；文字动态叠加|
|cards / type_header_word|180×34|`ui/a-gilded-v2/cards/type_header_word.png`|类型标题条；文字动态叠加|
|cards / type_header_formation|180×34|`ui/a-gilded-v2/cards/type_header_formation.png`|类型标题条；文字动态叠加|
|cards / type_header_seal|180×34|`ui/a-gilded-v2/cards/type_header_seal.png`|类型标题条；文字动态叠加|
|board / zone_header_casting|224×40|`ui/a-gilded-v2/board/zone_header_casting.png`|区域独立标题条：casting|
|board / zone_header_words|224×40|`ui/a-gilded-v2/board/zone_header_words.png`|区域独立标题条：words|
|board / zone_header_analysis|224×40|`ui/a-gilded-v2/board/zone_header_analysis.png`|区域独立标题条：analysis|
|board / zone_header_action|224×40|`ui/a-gilded-v2/board/zone_header_action.png`|区域独立标题条：action|
|board / zone_header_ash|224×40|`ui/a-gilded-v2/board/zone_header_ash.png`|区域独立标题条：ash|
|board / zone_header_formation|224×40|`ui/a-gilded-v2/board/zone_header_formation.png`|区域独立标题条：formation|
|board / zone_header_hand|224×40|`ui/a-gilded-v2/board/zone_header_hand.png`|区域独立标题条：hand|
|board / zone_header_deck|224×40|`ui/a-gilded-v2/board/zone_header_deck.png`|区域独立标题条：deck|
|board / meter_life_track|192×18|`ui/a-gilded-v2/board/meter_life_track.png`|life数值条底框|
|board / meter_life_fill|176×8|`ui/a-gilded-v2/board/meter_life_fill.png`|life数值条填充；按比例裁剪|
|board / meter_mana_track|192×18|`ui/a-gilded-v2/board/meter_mana_track.png`|mana数值条底框|
|board / meter_mana_fill|176×8|`ui/a-gilded-v2/board/meter_mana_fill.png`|mana数值条填充；按比例裁剪|
|board / meter_load_track|192×18|`ui/a-gilded-v2/board/meter_load_track.png`|load数值条底框|
|board / meter_load_fill|176×8|`ui/a-gilded-v2/board/meter_load_fill.png`|load数值条填充；按比例裁剪|
|board / meter_capacity_track|192×18|`ui/a-gilded-v2/board/meter_capacity_track.png`|capacity数值条底框|
|board / meter_capacity_fill|176×8|`ui/a-gilded-v2/board/meter_capacity_fill.png`|capacity数值条填充；按比例裁剪|
|interaction / overlay_full_hover|360×560|`ui/a-gilded-v2/interaction/overlay_full_hover.png`|完整卡牌 hover|
|interaction / overlay_full_selected|360×560|`ui/a-gilded-v2/interaction/overlay_full_selected.png`|完整卡牌 selected|
|interaction / overlay_full_target_candidate|360×560|`ui/a-gilded-v2/interaction/overlay_full_target_candidate.png`|完整卡牌 target_candidate|
|interaction / overlay_full_target_selected|360×560|`ui/a-gilded-v2/interaction/overlay_full_target_selected.png`|完整卡牌 target_selected|
|interaction / overlay_full_cost_candidate|360×560|`ui/a-gilded-v2/interaction/overlay_full_cost_candidate.png`|完整卡牌 cost_candidate|
|interaction / overlay_full_cost_selected|360×560|`ui/a-gilded-v2/interaction/overlay_full_cost_selected.png`|完整卡牌 cost_selected|
|interaction / card_detail|440×600|`ui/a-gilded-v2/interaction/card_detail.png`|通用 card_detail窗体|
|interaction / stack|440×260|`ui/a-gilded-v2/interaction/stack.png`|通用 stack窗体|
|interaction / settings|440×260|`ui/a-gilded-v2/interaction/settings.png`|通用 settings窗体|
|interaction / tooltip|440×260|`ui/a-gilded-v2/interaction/tooltip.png`|通用 tooltip窗体|
|materials / parchment-backing|1672×941|`ui/a-gilded-v2/materials/parchment-backing.png`|AI 生成的 C 风格羊皮纸底图；可选背景，不规定布局|
|cards / crest_action_64|64×64|`ui/a-gilded-v2/cards/crest_action_64.png`|独立古金类型纹章 行动|
|cards / crest_action_128|128×128|`ui/a-gilded-v2/cards/crest_action_128.png`|独立古金类型纹章 行动|
|cards / crest_analytic_64|64×64|`ui/a-gilded-v2/cards/crest_analytic_64.png`|独立古金类型纹章 解析法术|
|cards / crest_analytic_128|128×128|`ui/a-gilded-v2/cards/crest_analytic_128.png`|独立古金类型纹章 解析法术|
|cards / crest_word_64|64×64|`ui/a-gilded-v2/cards/crest_word_64.png`|独立古金类型纹章 言灵|
|cards / crest_word_128|128×128|`ui/a-gilded-v2/cards/crest_word_128.png`|独立古金类型纹章 言灵|
|cards / crest_formation_64|64×64|`ui/a-gilded-v2/cards/crest_formation_64.png`|独立古金类型纹章 阵法|
|cards / crest_formation_128|128×128|`ui/a-gilded-v2/cards/crest_formation_128.png`|独立古金类型纹章 阵法|
|cards / crest_seal_64|64×64|`ui/a-gilded-v2/cards/crest_seal_64.png`|独立古金类型纹章 符文|
|cards / crest_seal_128|128×128|`ui/a-gilded-v2/cards/crest_seal_128.png`|独立古金类型纹章 符文|
|rarity / rarity_badge_common_128|128×80|`ui/a-gilded-v2/rarity/rarity_badge_common_128.png`|普通大型镶座与刻度|
|rarity / rarity_cartouche_common|148×48|`ui/a-gilded-v2/rarity/rarity_cartouche_common.png`|普通名称与宝石组件|
|rarity / rarity_badge_uncommon_128|128×80|`ui/a-gilded-v2/rarity/rarity_badge_uncommon_128.png`|罕见大型镶座与刻度|
|rarity / rarity_cartouche_uncommon|148×48|`ui/a-gilded-v2/rarity/rarity_cartouche_uncommon.png`|罕见名称与宝石组件|
|rarity / rarity_badge_rare_128|128×80|`ui/a-gilded-v2/rarity/rarity_badge_rare_128.png`|稀有大型镶座与刻度|
|rarity / rarity_cartouche_rare|148×48|`ui/a-gilded-v2/rarity/rarity_cartouche_rare.png`|稀有名称与宝石组件|
|rarity / rarity_badge_epic_128|128×80|`ui/a-gilded-v2/rarity/rarity_badge_epic_128.png`|史诗大型镶座与刻度|
|rarity / rarity_cartouche_epic|148×48|`ui/a-gilded-v2/rarity/rarity_cartouche_epic.png`|史诗名称与宝石组件|
|rarity / rarity_badge_legendary_128|128×80|`ui/a-gilded-v2/rarity/rarity_badge_legendary_128.png`|传说大型镶座与刻度|
|rarity / rarity_cartouche_legendary|148×48|`ui/a-gilded-v2/rarity/rarity_cartouche_legendary.png`|传说名称与宝石组件|
|cards / stat_receptacle_normal|112×36|`ui/a-gilded-v2/cards/stat_receptacle_normal.png`|可选属性槽 normal|
|cards / stat_receptacle_hover|112×36|`ui/a-gilded-v2/cards/stat_receptacle_hover.png`|可选属性槽 hover|
|cards / stat_receptacle_disabled|112×36|`ui/a-gilded-v2/cards/stat_receptacle_disabled.png`|可选属性槽 disabled|
|cards / tag_response_only|132×28|`ui/a-gilded-v2/cards/tag_response_only.png`|条件/状态独立标签 response_only|
|cards / tag_ambush|132×28|`ui/a-gilded-v2/cards/tag_ambush.png`|条件/状态独立标签 ambush|
|cards / tag_concentration|132×28|`ui/a-gilded-v2/cards/tag_concentration.png`|条件/状态独立标签 concentration|
|cards / tag_base_protected|132×28|`ui/a-gilded-v2/cards/tag_base_protected.png`|条件/状态独立标签 base_protected|
|cards / tag_attached|132×28|`ui/a-gilded-v2/cards/tag_attached.png`|条件/状态独立标签 attached|
|cards / tag_duration|132×28|`ui/a-gilded-v2/cards/tag_duration.png`|条件/状态独立标签 duration|
|ornaments / corner_clasp_0|64×64|`ui/a-gilded-v2/ornaments/corner_clasp_0.png`|可组合卷饰角扣|
|ornaments / corner_clasp_1|64×64|`ui/a-gilded-v2/ornaments/corner_clasp_1.png`|可组合卷饰角扣|
|ornaments / corner_clasp_2|64×64|`ui/a-gilded-v2/ornaments/corner_clasp_2.png`|可组合卷饰角扣|
|ornaments / corner_clasp_3|64×64|`ui/a-gilded-v2/ornaments/corner_clasp_3.png`|可组合卷饰角扣|
|ornaments / gold_divider|320×24|`ui/a-gilded-v2/ornaments/gold_divider.png`|独立横向分隔卷饰|
|interaction / overlay_master_hover|1024×1536|`ui/a-gilded-v2/interaction/overlay_master_hover.png`|精绘主卡框 hover|
|interaction / overlay_master_selected|1024×1536|`ui/a-gilded-v2/interaction/overlay_master_selected.png`|精绘主卡框 selected|
|interaction / overlay_master_target_candidate|1024×1536|`ui/a-gilded-v2/interaction/overlay_master_target_candidate.png`|精绘主卡框 target_candidate|
|interaction / overlay_master_target_selected|1024×1536|`ui/a-gilded-v2/interaction/overlay_master_target_selected.png`|精绘主卡框 target_selected|
|interaction / overlay_master_cost_candidate|1024×1536|`ui/a-gilded-v2/interaction/overlay_master_cost_candidate.png`|精绘主卡框 cost_candidate|
|interaction / overlay_master_cost_selected|1024×1536|`ui/a-gilded-v2/interaction/overlay_master_cost_selected.png`|精绘主卡框 cost_selected|
|painted / master_action|1024×1536|`ui/a-gilded-v2/painted/action.png`|A 古籍金饰原始绘制：action|
|painted / master_analytic|1024×1536|`ui/a-gilded-v2/painted/analytic.png`|A 古籍金饰原始绘制：analytic|
|painted / master_word|1024×1536|`ui/a-gilded-v2/painted/word.png`|A 古籍金饰原始绘制：word|
|painted / master_formation|1024×1536|`ui/a-gilded-v2/painted/formation.png`|A 古籍金饰原始绘制：formation|
|painted / master_seal|1024×1536|`ui/a-gilded-v2/painted/seal.png`|A 古籍金饰原始绘制：seal|
|painted / master_back|1024×1536|`ui/a-gilded-v2/painted/back.png`|A 古籍金饰原始绘制：back|
|painted / master_window|1536×1024|`ui/a-gilded-v2/painted/window.png`|A 古籍金饰原始绘制：window|
|painted / master_gem_common|1254×1254|`ui/a-gilded-v2/painted/gem_common.png`|A 古籍金饰原始绘制：gem_common|
|painted / master_gem_uncommon|1254×1254|`ui/a-gilded-v2/painted/gem_uncommon.png`|A 古籍金饰原始绘制：gem_uncommon|
|painted / master_gem_rare|1254×1254|`ui/a-gilded-v2/painted/gem_rare.png`|A 古籍金饰原始绘制：gem_rare|
|painted / master_gem_epic|1254×1254|`ui/a-gilded-v2/painted/gem_epic.png`|A 古籍金饰原始绘制：gem_epic|
|painted / master_gem_legendary|1254×1254|`ui/a-gilded-v2/painted/gem_legendary.png`|A 古籍金饰原始绘制：gem_legendary|
