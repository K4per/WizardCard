# WizardCard C 风格美术资源清单

已完成独立素材绘制；未接入游戏，也未重排战场。

共 **232 个 PNG 资产**；另附 30 张真实卡牌排版预览、2 张资源总览、分层 SVG 源文件、生成脚本和 JSON 清单。

|分类|PNG 数量|
|---|---:|
|icons|92|
|cards|39|
|interaction|58|
|board|37|
|rarity|5|
|materials|1|

## 卡牌模板与信息设计

五类均有手牌、条目、小立绘、详情和完整模板，共 25 套。完整模板 360×560；透明插画窗 322×192。
行动：折角齿形侧边与闪电纹章；解析法术：卷册双层横边与书页纹章；言灵：流线卷饰与羽笔纹章；阵法：角部晶格与六芒星纹章；符文：双线镶边与菱环纹章。

卡名独立酒红标题栏，学派／基础属性副标题，插画窗，下方两行共六个属性格，规则文字区，底部类型与独立稀有度槽。
属性以当前 cards.json 为准：行动费用与速度；解析费用／施法费用／环阶／解析回合／速度／持续；言灵费用／环阶／速度／持续；阵法体量／负载上限／魔力产出／环位／最高环阶／速度；符文费用／体量／负载／速度／启动费用（字段存在才展示）。
完整卡规则区支持当前卡池全部文本；风味文字属于可选扩展详情，不烘焙进卡框。缩略模板用于简略信息；完整正文应在完整模板或滚动详情中展示。

## 稀有度

普通：银灰方章；罕见：青绿菱晶；稀有：蓝色六边宝石；史诗：紫色星芒；传说：古金王冠。每档附 1—5 枚刻度，并保留程序绘制名称的位置。颜色、轮廓、刻度共同区分，稀有度与类型无绑定。每种类型可组合任意稀有度。

## 交付与使用

PNG 均为独立组件，不包含卡名／规则／数值文字；大卡框插画窗透明。预览文件是文字和现有插画的示范组合，不能用作动态卡牌底板。
像素组件使用最近邻缩放；卡框保持固定比例，其他组件按 manifest 的九宫格／横向拉伸／裁剪参数使用。尺寸只是素材默认规格，不规定最终战场布局。
施法区、言灵区、解析区、行动区、灰烬区均提供独立标题与区域框体，双方可复用。另含阵法、手牌、牌库、HUD、弹窗、交互状态、资源条、图标与卡背。
材料底图为本次 imagegen 原始生成输出；其余 UI 为可编辑代码原生像素图形。既有 30 张插画仅在预览中引用，未重绘或改动。

## 逐项清单

下表路径相对于 assets/art。

|分类 / ID|尺寸|文件|用途|
|---|---|---|---|
|icons / type_action|32×32|`ui/c-theme-v1/icons/type_action.png`|行动|
|icons / type_action_64|64×64|`ui/c-theme-v1/icons/type_action_64.png`|行动 2×|
|icons / type_analytic|32×32|`ui/c-theme-v1/icons/type_analytic.png`|解析法术|
|icons / type_analytic_64|64×64|`ui/c-theme-v1/icons/type_analytic_64.png`|解析法术 2×|
|icons / type_word|32×32|`ui/c-theme-v1/icons/type_word.png`|言灵|
|icons / type_word_64|64×64|`ui/c-theme-v1/icons/type_word_64.png`|言灵 2×|
|icons / type_formation|32×32|`ui/c-theme-v1/icons/type_formation.png`|阵法|
|icons / type_formation_64|64×64|`ui/c-theme-v1/icons/type_formation_64.png`|阵法 2×|
|icons / type_seal|32×32|`ui/c-theme-v1/icons/type_seal.png`|符文|
|icons / type_seal_64|64×64|`ui/c-theme-v1/icons/type_seal_64.png`|符文 2×|
|icons / rank|32×32|`ui/c-theme-v1/icons/rank.png`|位阶|
|icons / rank_64|64×64|`ui/c-theme-v1/icons/rank_64.png`|位阶 2×|
|icons / analysis_cost|32×32|`ui/c-theme-v1/icons/analysis_cost.png`|解析费用|
|icons / analysis_cost_64|64×64|`ui/c-theme-v1/icons/analysis_cost_64.png`|解析费用 2×|
|icons / cast_cost|32×32|`ui/c-theme-v1/icons/cast_cost.png`|施法费用|
|icons / cast_cost_64|64×64|`ui/c-theme-v1/icons/cast_cost_64.png`|施法费用 2×|
|icons / body|32×32|`ui/c-theme-v1/icons/body.png`|阵体|
|icons / body_64|64×64|`ui/c-theme-v1/icons/body_64.png`|阵体 2×|
|icons / ring_slot|32×32|`ui/c-theme-v1/icons/ring_slot.png`|环位|
|icons / ring_slot_64|64×64|`ui/c-theme-v1/icons/ring_slot_64.png`|环位 2×|
|icons / mana_income|32×32|`ui/c-theme-v1/icons/mana_income.png`|魔素收入|
|icons / mana_income_64|64×64|`ui/c-theme-v1/icons/mana_income_64.png`|魔素收入 2×|
|icons / duration|32×32|`ui/c-theme-v1/icons/duration.png`|持续计数|
|icons / duration_64|64×64|`ui/c-theme-v1/icons/duration_64.png`|持续计数 2×|
|icons / state_analyzing|32×32|`ui/c-theme-v1/icons/state_analyzing.png`|解析中|
|icons / state_analyzing_64|64×64|`ui/c-theme-v1/icons/state_analyzing_64.png`|解析中 2×|
|icons / state_analyzed|32×32|`ui/c-theme-v1/icons/state_analyzed.png`|解析完成|
|icons / state_analyzed_64|64×64|`ui/c-theme-v1/icons/state_analyzed_64.png`|解析完成 2×|
|icons / state_prepared|32×32|`ui/c-theme-v1/icons/state_prepared.png`|待施放|
|icons / state_prepared_64|64×64|`ui/c-theme-v1/icons/state_prepared_64.png`|待施放 2×|
|icons / state_active|32×32|`ui/c-theme-v1/icons/state_active.png`|持续生效|
|icons / state_active_64|64×64|`ui/c-theme-v1/icons/state_active_64.png`|持续生效 2×|
|icons / state_cancelled|32×32|`ui/c-theme-v1/icons/state_cancelled.png`|准备被取消|
|icons / state_cancelled_64|64×64|`ui/c-theme-v1/icons/state_cancelled_64.png`|准备被取消 2×|
|icons / state_protected|32×32|`ui/c-theme-v1/icons/state_protected.png`|基础阵法保护|
|icons / state_protected_64|64×64|`ui/c-theme-v1/icons/state_protected_64.png`|基础阵法保护 2×|
|icons / state_attached|32×32|`ui/c-theme-v1/icons/state_attached.png`|符文附着|
|icons / state_attached_64|64×64|`ui/c-theme-v1/icons/state_attached_64.png`|符文附着 2×|
|icons / life|32×32|`ui/c-theme-v1/icons/life.png`|生命|
|icons / life_64|64×64|`ui/c-theme-v1/icons/life_64.png`|生命 2×|
|icons / mana|32×32|`ui/c-theme-v1/icons/mana.png`|魔素|
|icons / mana_64|64×64|`ui/c-theme-v1/icons/mana_64.png`|魔素 2×|
|icons / load|32×32|`ui/c-theme-v1/icons/load.png`|当前荷载|
|icons / load_64|64×64|`ui/c-theme-v1/icons/load_64.png`|当前荷载 2×|
|icons / capacity|32×32|`ui/c-theme-v1/icons/capacity.png`|荷载容量|
|icons / capacity_64|64×64|`ui/c-theme-v1/icons/capacity_64.png`|荷载容量 2×|
|icons / zone_formation|32×32|`ui/c-theme-v1/icons/zone_formation.png`|阵法|
|icons / zone_formation_64|64×64|`ui/c-theme-v1/icons/zone_formation_64.png`|阵法 2×|
|icons / zone_analysis|32×32|`ui/c-theme-v1/icons/zone_analysis.png`|解析|
|icons / zone_analysis_64|64×64|`ui/c-theme-v1/icons/zone_analysis_64.png`|解析 2×|
|icons / zone_casting|32×32|`ui/c-theme-v1/icons/zone_casting.png`|施法|
|icons / zone_casting_64|64×64|`ui/c-theme-v1/icons/zone_casting_64.png`|施法 2×|
|icons / zone_words|32×32|`ui/c-theme-v1/icons/zone_words.png`|言灵|
|icons / zone_words_64|64×64|`ui/c-theme-v1/icons/zone_words_64.png`|言灵 2×|
|icons / zone_action|32×32|`ui/c-theme-v1/icons/zone_action.png`|行动|
|icons / zone_action_64|64×64|`ui/c-theme-v1/icons/zone_action_64.png`|行动 2×|
|icons / zone_ash|32×32|`ui/c-theme-v1/icons/zone_ash.png`|灰烬|
|icons / zone_ash_64|64×64|`ui/c-theme-v1/icons/zone_ash_64.png`|灰烬 2×|
|icons / zone_hand|32×32|`ui/c-theme-v1/icons/zone_hand.png`|手牌|
|icons / zone_hand_64|64×64|`ui/c-theme-v1/icons/zone_hand_64.png`|手牌 2×|
|icons / zone_deck|32×32|`ui/c-theme-v1/icons/zone_deck.png`|牌库|
|icons / zone_deck_64|64×64|`ui/c-theme-v1/icons/zone_deck_64.png`|牌库 2×|
|icons / close|32×32|`ui/c-theme-v1/icons/close.png`|关闭|
|icons / close_64|64×64|`ui/c-theme-v1/icons/close_64.png`|关闭 2×|
|icons / confirm|32×32|`ui/c-theme-v1/icons/confirm.png`|确认|
|icons / confirm_64|64×64|`ui/c-theme-v1/icons/confirm_64.png`|确认 2×|
|icons / cancel|32×32|`ui/c-theme-v1/icons/cancel.png`|取消|
|icons / cancel_64|64×64|`ui/c-theme-v1/icons/cancel_64.png`|取消 2×|
|icons / left|32×32|`ui/c-theme-v1/icons/left.png`|向前滚动|
|icons / left_64|64×64|`ui/c-theme-v1/icons/left_64.png`|向前滚动 2×|
|icons / right|32×32|`ui/c-theme-v1/icons/right.png`|向后滚动|
|icons / right_64|64×64|`ui/c-theme-v1/icons/right_64.png`|向后滚动 2×|
|icons / up|32×32|`ui/c-theme-v1/icons/up.png`|向上|
|icons / up_64|64×64|`ui/c-theme-v1/icons/up_64.png`|向上 2×|
|icons / down|32×32|`ui/c-theme-v1/icons/down.png`|向下|
|icons / down_64|64×64|`ui/c-theme-v1/icons/down_64.png`|向下 2×|
|icons / save|32×32|`ui/c-theme-v1/icons/save.png`|保存|
|icons / save_64|64×64|`ui/c-theme-v1/icons/save_64.png`|保存 2×|
|icons / log|32×32|`ui/c-theme-v1/icons/log.png`|记录|
|icons / log_64|64×64|`ui/c-theme-v1/icons/log_64.png`|记录 2×|
|icons / surrender|32×32|`ui/c-theme-v1/icons/surrender.png`|投降|
|icons / surrender_64|64×64|`ui/c-theme-v1/icons/surrender_64.png`|投降 2×|
|icons / success|32×32|`ui/c-theme-v1/icons/success.png`|成功|
|icons / success_64|64×64|`ui/c-theme-v1/icons/success_64.png`|成功 2×|
|icons / no_action|32×32|`ui/c-theme-v1/icons/no_action.png`|无操作|
|icons / no_action_64|64×64|`ui/c-theme-v1/icons/no_action_64.png`|无操作 2×|
|icons / wrong_target|32×32|`ui/c-theme-v1/icons/wrong_target.png`|选错目标|
|icons / wrong_target_64|64×64|`ui/c-theme-v1/icons/wrong_target_64.png`|选错目标 2×|
|icons / rejected|32×32|`ui/c-theme-v1/icons/rejected.png`|操作拒绝|
|icons / rejected_64|64×64|`ui/c-theme-v1/icons/rejected_64.png`|操作拒绝 2×|
|icons / eye|32×32|`ui/c-theme-v1/icons/eye.png`|查看|
|icons / eye_64|64×64|`ui/c-theme-v1/icons/eye_64.png`|查看 2×|
|cards / frame_action_hand|101×118|`ui/c-theme-v1/cards/frame_action_hand.png`|行动 hand|
|cards / frame_action_strip|125×44|`ui/c-theme-v1/cards/frame_action_strip.png`|行动 strip|
|cards / frame_action_portrait|90×112|`ui/c-theme-v1/cards/frame_action_portrait.png`|行动 portrait|
|cards / frame_action_detail|215×354|`ui/c-theme-v1/cards/frame_action_detail.png`|行动 detail|
|cards / frame_analytic_hand|101×118|`ui/c-theme-v1/cards/frame_analytic_hand.png`|解析法术 hand|
|cards / frame_analytic_strip|125×44|`ui/c-theme-v1/cards/frame_analytic_strip.png`|解析法术 strip|
|cards / frame_analytic_portrait|90×112|`ui/c-theme-v1/cards/frame_analytic_portrait.png`|解析法术 portrait|
|cards / frame_analytic_detail|215×354|`ui/c-theme-v1/cards/frame_analytic_detail.png`|解析法术 detail|
|cards / frame_word_hand|101×118|`ui/c-theme-v1/cards/frame_word_hand.png`|言灵 hand|
|cards / frame_word_strip|125×44|`ui/c-theme-v1/cards/frame_word_strip.png`|言灵 strip|
|cards / frame_word_portrait|90×112|`ui/c-theme-v1/cards/frame_word_portrait.png`|言灵 portrait|
|cards / frame_word_detail|215×354|`ui/c-theme-v1/cards/frame_word_detail.png`|言灵 detail|
|cards / frame_formation_hand|101×118|`ui/c-theme-v1/cards/frame_formation_hand.png`|阵法 hand|
|cards / frame_formation_strip|125×44|`ui/c-theme-v1/cards/frame_formation_strip.png`|阵法 strip|
|cards / frame_formation_portrait|90×112|`ui/c-theme-v1/cards/frame_formation_portrait.png`|阵法 portrait|
|cards / frame_formation_detail|215×354|`ui/c-theme-v1/cards/frame_formation_detail.png`|阵法 detail|
|cards / frame_seal_hand|101×118|`ui/c-theme-v1/cards/frame_seal_hand.png`|符文 hand|
|cards / frame_seal_strip|125×44|`ui/c-theme-v1/cards/frame_seal_strip.png`|符文 strip|
|cards / frame_seal_portrait|90×112|`ui/c-theme-v1/cards/frame_seal_portrait.png`|符文 portrait|
|cards / frame_seal_detail|215×354|`ui/c-theme-v1/cards/frame_seal_detail.png`|符文 detail|
|cards / rarity_common|42×12|`ui/c-theme-v1/cards/rarity_common.png`|稀有度独立标记；仍需动态文字|
|cards / rarity_uncommon|42×12|`ui/c-theme-v1/cards/rarity_uncommon.png`|稀有度独立标记；仍需动态文字|
|cards / rarity_rare|42×12|`ui/c-theme-v1/cards/rarity_rare.png`|稀有度独立标记；仍需动态文字|
|cards / rarity_epic|42×12|`ui/c-theme-v1/cards/rarity_epic.png`|稀有度独立标记；仍需动态文字|
|cards / rarity_legendary|42×12|`ui/c-theme-v1/cards/rarity_legendary.png`|稀有度独立标记；仍需动态文字|
|cards / card_back_hand|101×118|`ui/c-theme-v1/cards/card_back_hand.png`|对称通用卡背；不携带私有信息|
|cards / card_back_opponent|39×41|`ui/c-theme-v1/cards/card_back_opponent.png`|对称通用卡背；不携带私有信息|
|cards / card_back_deck|100×132|`ui/c-theme-v1/cards/card_back_deck.png`|对称通用卡背；不携带私有信息|
|cards / missing_art|64×64|`ui/c-theme-v1/cards/missing_art.png`|缺图统一回退纹章|
|interaction / button_primary_normal|160×36|`ui/c-theme-v1/interaction/button_primary_normal.png`|primary / normal|
|interaction / button_primary_hover|160×36|`ui/c-theme-v1/interaction/button_primary_hover.png`|primary / hover|
|interaction / button_primary_pressed|160×36|`ui/c-theme-v1/interaction/button_primary_pressed.png`|primary / pressed|
|interaction / button_primary_disabled|160×36|`ui/c-theme-v1/interaction/button_primary_disabled.png`|primary / disabled|
|interaction / button_secondary_normal|160×36|`ui/c-theme-v1/interaction/button_secondary_normal.png`|secondary / normal|
|interaction / button_secondary_hover|160×36|`ui/c-theme-v1/interaction/button_secondary_hover.png`|secondary / hover|
|interaction / button_secondary_pressed|160×36|`ui/c-theme-v1/interaction/button_secondary_pressed.png`|secondary / pressed|
|interaction / button_secondary_disabled|160×36|`ui/c-theme-v1/interaction/button_secondary_disabled.png`|secondary / disabled|
|interaction / button_danger_normal|160×36|`ui/c-theme-v1/interaction/button_danger_normal.png`|danger / normal|
|interaction / button_danger_hover|160×36|`ui/c-theme-v1/interaction/button_danger_hover.png`|danger / hover|
|interaction / button_danger_pressed|160×36|`ui/c-theme-v1/interaction/button_danger_pressed.png`|danger / pressed|
|interaction / button_danger_disabled|160×36|`ui/c-theme-v1/interaction/button_danger_disabled.png`|danger / disabled|
|interaction / overlay_hand_hover|101×118|`ui/c-theme-v1/interaction/overlay_hand_hover.png`|hover|
|interaction / overlay_hand_selected|101×118|`ui/c-theme-v1/interaction/overlay_hand_selected.png`|selected|
|interaction / overlay_hand_target_candidate|101×118|`ui/c-theme-v1/interaction/overlay_hand_target_candidate.png`|target_candidate|
|interaction / overlay_hand_target_selected|101×118|`ui/c-theme-v1/interaction/overlay_hand_target_selected.png`|target_selected|
|interaction / overlay_hand_cost_candidate|101×118|`ui/c-theme-v1/interaction/overlay_hand_cost_candidate.png`|cost_candidate|
|interaction / overlay_hand_cost_selected|101×118|`ui/c-theme-v1/interaction/overlay_hand_cost_selected.png`|cost_selected|
|interaction / overlay_strip_hover|125×44|`ui/c-theme-v1/interaction/overlay_strip_hover.png`|hover|
|interaction / overlay_strip_selected|125×44|`ui/c-theme-v1/interaction/overlay_strip_selected.png`|selected|
|interaction / overlay_strip_target_candidate|125×44|`ui/c-theme-v1/interaction/overlay_strip_target_candidate.png`|target_candidate|
|interaction / overlay_strip_target_selected|125×44|`ui/c-theme-v1/interaction/overlay_strip_target_selected.png`|target_selected|
|interaction / overlay_strip_cost_candidate|125×44|`ui/c-theme-v1/interaction/overlay_strip_cost_candidate.png`|cost_candidate|
|interaction / overlay_strip_cost_selected|125×44|`ui/c-theme-v1/interaction/overlay_strip_cost_selected.png`|cost_selected|
|interaction / overlay_portrait_hover|90×112|`ui/c-theme-v1/interaction/overlay_portrait_hover.png`|hover|
|interaction / overlay_portrait_selected|90×112|`ui/c-theme-v1/interaction/overlay_portrait_selected.png`|selected|
|interaction / overlay_portrait_target_candidate|90×112|`ui/c-theme-v1/interaction/overlay_portrait_target_candidate.png`|target_candidate|
|interaction / overlay_portrait_target_selected|90×112|`ui/c-theme-v1/interaction/overlay_portrait_target_selected.png`|target_selected|
|interaction / overlay_portrait_cost_candidate|90×112|`ui/c-theme-v1/interaction/overlay_portrait_cost_candidate.png`|cost_candidate|
|interaction / overlay_portrait_cost_selected|90×112|`ui/c-theme-v1/interaction/overlay_portrait_cost_selected.png`|cost_selected|
|board / region_formation|106×52|`ui/c-theme-v1/board/region_formation.png`|区域框体：formation|
|board / region_analysis|427×52|`ui/c-theme-v1/board/region_analysis.png`|区域框体：analysis|
|board / region_casting|1080×65|`ui/c-theme-v1/board/region_casting.png`|区域框体：casting|
|board / region_words|531×106|`ui/c-theme-v1/board/region_words.png`|区域框体：words|
|board / region_action|531×49|`ui/c-theme-v1/board/region_action.png`|区域框体：action|
|board / region_ash|531×101|`ui/c-theme-v1/board/region_ash.png`|区域框体：ash|
|board / region_hand|1080×134|`ui/c-theme-v1/board/region_hand.png`|区域框体：hand|
|board / region_deck|100×132|`ui/c-theme-v1/board/region_deck.png`|区域框体：deck|
|board / slot_empty|106×52|`ui/c-theme-v1/board/slot_empty.png`|阵法槽状态|
|board / slot_occupied|106×52|`ui/c-theme-v1/board/slot_occupied.png`|阵法槽状态|
|board / slot_span|106×107|`ui/c-theme-v1/board/slot_span.png`|阵法槽状态|
|board / host_connector|12×52|`ui/c-theme-v1/board/host_connector.png`|宿主到解析行关联|
|board / hud|215×190|`ui/c-theme-v1/board/hud.png`|hud|
|board / phase_bar|700×42|`ui/c-theme-v1/board/phase_bar.png`|phase_bar|
|interaction / task_hint|800×36|`ui/c-theme-v1/interaction/task_hint.png`|task_hint|
|interaction / action_bar|672×44|`ui/c-theme-v1/interaction/action_bar.png`|action_bar|
|interaction / summary|215×88|`ui/c-theme-v1/interaction/summary.png`|summary|
|interaction / decision|540×310|`ui/c-theme-v1/interaction/decision.png`|decision|
|interaction / log_window|620×420|`ui/c-theme-v1/interaction/log_window.png`|log_window|
|interaction / result_window|520×340|`ui/c-theme-v1/interaction/result_window.png`|result_window|
|interaction / handoff|1080×134|`ui/c-theme-v1/interaction/handoff.png`|handoff|
|board / phase_inactive|22×22|`ui/c-theme-v1/board/phase_inactive.png`|阶段指示 inactive|
|board / phase_current|22×22|`ui/c-theme-v1/board/phase_current.png`|阶段指示 current|
|board / phase_complete|22×22|`ui/c-theme-v1/board/phase_complete.png`|阶段指示 complete|
|board / scroll_track|80×6|`ui/c-theme-v1/board/scroll_track.png`|滚动位置提示|
|board / scroll_thumb|80×6|`ui/c-theme-v1/board/scroll_thumb.png`|滚动位置提示|
|interaction / result_victory|96×64|`ui/c-theme-v1/interaction/result_victory.png`|victory结果标识|
|interaction / result_defeat|96×64|`ui/c-theme-v1/interaction/result_defeat.png`|defeat结果标识|
|interaction / result_draw|96×64|`ui/c-theme-v1/interaction/result_draw.png`|draw结果标识|
|interaction / feedback_success|360×48|`ui/c-theme-v1/interaction/feedback_success.png`|success反馈|
|interaction / feedback_no_action|360×48|`ui/c-theme-v1/interaction/feedback_no_action.png`|no_action反馈|
|interaction / feedback_wrong_target|360×48|`ui/c-theme-v1/interaction/feedback_wrong_target.png`|wrong_target反馈|
|interaction / feedback_rejected|360×48|`ui/c-theme-v1/interaction/feedback_rejected.png`|rejected反馈|
|interaction / stepper_0|480×32|`ui/c-theme-v1/interaction/stepper_0.png`|查看/目标/额外成本/确认步骤底板；文字动态绘制|
|interaction / stepper_1|480×32|`ui/c-theme-v1/interaction/stepper_1.png`|查看/目标/额外成本/确认步骤底板；文字动态绘制|
|interaction / stepper_2|480×32|`ui/c-theme-v1/interaction/stepper_2.png`|查看/目标/额外成本/确认步骤底板；文字动态绘制|
|interaction / stepper_3|480×32|`ui/c-theme-v1/interaction/stepper_3.png`|查看/目标/额外成本/确认步骤底板；文字动态绘制|
|board / match_background|1600×1000|`ui/c-theme-v1/board/match_background.png`|低对比无文字场地底图|
|board / board_decoration|1600×1000|`ui/c-theme-v1/board/board_decoration.png`|独立中央装饰层；不定义槽位|
|rarity / rarity_badge_common|64×40|`ui/c-theme-v1/rarity/rarity_badge_common.png`|普通：独立轮廓＋颜色＋1枚刻度|
|rarity / rarity_badge_uncommon|64×40|`ui/c-theme-v1/rarity/rarity_badge_uncommon.png`|罕见：独立轮廓＋颜色＋2枚刻度|
|rarity / rarity_badge_rare|64×40|`ui/c-theme-v1/rarity/rarity_badge_rare.png`|稀有：独立轮廓＋颜色＋3枚刻度|
|rarity / rarity_badge_epic|64×40|`ui/c-theme-v1/rarity/rarity_badge_epic.png`|史诗：独立轮廓＋颜色＋4枚刻度|
|rarity / rarity_badge_legendary|64×40|`ui/c-theme-v1/rarity/rarity_badge_legendary.png`|传说：独立轮廓＋颜色＋5枚刻度|
|cards / frame_action_full|360×560|`ui/c-theme-v1/cards/frame_action_full.png`|行动完整模板|
|cards / frame_analytic_full|360×560|`ui/c-theme-v1/cards/frame_analytic_full.png`|解析法术完整模板|
|cards / frame_word_full|360×560|`ui/c-theme-v1/cards/frame_word_full.png`|言灵完整模板|
|cards / frame_formation_full|360×560|`ui/c-theme-v1/cards/frame_formation_full.png`|阵法完整模板|
|cards / frame_seal_full|360×560|`ui/c-theme-v1/cards/frame_seal_full.png`|符文完整模板|
|cards / type_header_action|180×34|`ui/c-theme-v1/cards/type_header_action.png`|类型标题条；文字动态叠加|
|cards / type_header_analytic|180×34|`ui/c-theme-v1/cards/type_header_analytic.png`|类型标题条；文字动态叠加|
|cards / type_header_word|180×34|`ui/c-theme-v1/cards/type_header_word.png`|类型标题条；文字动态叠加|
|cards / type_header_formation|180×34|`ui/c-theme-v1/cards/type_header_formation.png`|类型标题条；文字动态叠加|
|cards / type_header_seal|180×34|`ui/c-theme-v1/cards/type_header_seal.png`|类型标题条；文字动态叠加|
|board / zone_header_casting|224×40|`ui/c-theme-v1/board/zone_header_casting.png`|区域独立标题条：casting|
|board / zone_header_words|224×40|`ui/c-theme-v1/board/zone_header_words.png`|区域独立标题条：words|
|board / zone_header_analysis|224×40|`ui/c-theme-v1/board/zone_header_analysis.png`|区域独立标题条：analysis|
|board / zone_header_action|224×40|`ui/c-theme-v1/board/zone_header_action.png`|区域独立标题条：action|
|board / zone_header_ash|224×40|`ui/c-theme-v1/board/zone_header_ash.png`|区域独立标题条：ash|
|board / zone_header_formation|224×40|`ui/c-theme-v1/board/zone_header_formation.png`|区域独立标题条：formation|
|board / zone_header_hand|224×40|`ui/c-theme-v1/board/zone_header_hand.png`|区域独立标题条：hand|
|board / zone_header_deck|224×40|`ui/c-theme-v1/board/zone_header_deck.png`|区域独立标题条：deck|
|board / meter_life_track|192×18|`ui/c-theme-v1/board/meter_life_track.png`|life数值条底框|
|board / meter_life_fill|176×8|`ui/c-theme-v1/board/meter_life_fill.png`|life数值条填充；按比例裁剪|
|board / meter_mana_track|192×18|`ui/c-theme-v1/board/meter_mana_track.png`|mana数值条底框|
|board / meter_mana_fill|176×8|`ui/c-theme-v1/board/meter_mana_fill.png`|mana数值条填充；按比例裁剪|
|board / meter_load_track|192×18|`ui/c-theme-v1/board/meter_load_track.png`|load数值条底框|
|board / meter_load_fill|176×8|`ui/c-theme-v1/board/meter_load_fill.png`|load数值条填充；按比例裁剪|
|board / meter_capacity_track|192×18|`ui/c-theme-v1/board/meter_capacity_track.png`|capacity数值条底框|
|board / meter_capacity_fill|176×8|`ui/c-theme-v1/board/meter_capacity_fill.png`|capacity数值条填充；按比例裁剪|
|interaction / overlay_full_hover|360×560|`ui/c-theme-v1/interaction/overlay_full_hover.png`|完整卡牌 hover|
|interaction / overlay_full_selected|360×560|`ui/c-theme-v1/interaction/overlay_full_selected.png`|完整卡牌 selected|
|interaction / overlay_full_target_candidate|360×560|`ui/c-theme-v1/interaction/overlay_full_target_candidate.png`|完整卡牌 target_candidate|
|interaction / overlay_full_target_selected|360×560|`ui/c-theme-v1/interaction/overlay_full_target_selected.png`|完整卡牌 target_selected|
|interaction / overlay_full_cost_candidate|360×560|`ui/c-theme-v1/interaction/overlay_full_cost_candidate.png`|完整卡牌 cost_candidate|
|interaction / overlay_full_cost_selected|360×560|`ui/c-theme-v1/interaction/overlay_full_cost_selected.png`|完整卡牌 cost_selected|
|interaction / card_detail|440×600|`ui/c-theme-v1/interaction/card_detail.png`|通用 card_detail窗体|
|interaction / stack|440×260|`ui/c-theme-v1/interaction/stack.png`|通用 stack窗体|
|interaction / settings|440×260|`ui/c-theme-v1/interaction/settings.png`|通用 settings窗体|
|interaction / tooltip|440×260|`ui/c-theme-v1/interaction/tooltip.png`|通用 tooltip窗体|
|materials / parchment-backing|1672×941|`ui/c-theme-v1/materials/parchment-backing.png`|AI 生成的 C 风格羊皮纸底图；可选背景，不规定布局|
