class_name CardPresentation
extends Resource

const RARITY_LABELS: Dictionary[String, String] = {
	"common": "普通",
	"uncommon": "罕见",
	"rare": "稀有",
	"epic": "史诗",
	"legendary": "传说",
}

@export var instance_id: String = ""
@export var definition_id: String = ""
@export var number: String = ""
@export var title: String = ""
@export var type_key: String = "analysis"
@export var type_label: String = ""
@export var text: String = ""
@export var rarity: String = "common"
@export var primary_value: String = ""
@export var secondary_value: String = ""
@export var speed: int = 1
@export var status: String = ""
@export var hidden: bool = false
@export var face_down: bool = false
@export var setting_cost: int = 0
@export var cast_cost: int = 0
@export var illustration_id: String = ""
@export var metadata_text: String = ""
@export var preview: bool = false

static func from_definition(data: Dictionary) -> CardPresentation:
	var card: CardPresentation = CardPresentation.new()
	card.definition_id = str(data.get("id", ""))
	card.number = str(data.get("number", ""))
	card.illustration_id = str(data.get("illustrationId", ""))
	card.instance_id = card.definition_id
	card.title = str(data.get("name", ""))
	card.text = str(data.get("text", ""))
	card.rarity = str(data.get("rarity", "common"))
	card.speed = int(data.get("speed", 1))
	var type_index: int = int(data.get("type", 1))
	card.type_key = ["incantation", "analysis", "incantation", "formation", "rune", "talisman"][clampi(type_index, 0, 5)]
	card.type_label = str(data.get("typeLabel", ["行动（旧版）", "解析", "言灵", "阵法", "符文", "咒符"][clampi(type_index, 0, 5)]))
	card.setting_cost = int(data.get("cost", 0)) if type_index != 5 else 0
	card.cast_cost = int(data.get("cost", 0)) if type_index in [2, 5] else int(data.get("castCost", 0))
	card._update_cost_label()
	card.secondary_value = str(data.get("level", data.get("rank", 0)))
	var lines: Array[String] = []
	if not card.number.is_empty():
		lines.append("编号：" + card.number)
	lines.append("等级：%d" % int(data.get("level", data.get("rank", 0))))
	if type_index == 3:
		lines.append("阵体：%d　环位：%d" % [int(data.get("body", 0)), int(data.get("rings", 0))])
		lines.append("荷载上限：%d　魔素收入：%d" % [int(data.get("capacity", 0)), int(data.get("income", 0))])
		lines.append("设置条件：" + formation_condition(data))
	else:
		var school: String = str(data.get("school", ""))
		var schools: Dictionary[String, String] = {"abjuration": "防护", "conjuration": "咒法", "divination": "预言", "enchantment": "惑控", "evocation": "塑能", "illusion": "幻术", "necromancy": "死灵", "transmutation": "变化"}
		if not school.is_empty():
			lines.append("学派：" + schools.get(school, school))
		lines.append(card.cost_detail())
		lines.append("解析时间：%d　速度：%d" % [int(data.get("analysisTurns", 0)), card.speed])
	card.metadata_text = "\n".join(lines)
	return card

static func formation_condition(data: Dictionary) -> String:
	return str(data.get("settingConditions", ""))

static func from_preview_definition(data: Dictionary) -> CardPresentation:
	var card: CardPresentation = CardPresentation.new()
	card.definition_id = str(data["id"])
	card.instance_id = card.definition_id
	card.title = str(data["name"])
	card.type_key = str(data["typeKey"])
	card.type_label = str(data["typeLabel"])
	card.text = str(data["text"])
	card.illustration_id = str(data["illustrationId"])
	card.secondary_value = str(int(data["level"]))
	card.preview = true
	# These design documents do not assign rarities or formation speeds.
	card.rarity = ""
	card.speed = int(data.get("speed", 0))
	var lines: Array[String] = ["等级：%d" % int(data["level"])]
	if card.type_key == "formation":
		lines.append("阵体大小：%d    最大解析数：%d" % [int(data["body"]), int(data["rings"])])
		lines.append("荷载上限：%d    魔素收入：%d" % [int(data["capacity"]), int(data["income"])])
		lines.append("召唤条件：" + str(data["summonCondition"]))
	else:
		var costs: Array = data["costParts"]
		card.primary_value = str(int(costs[0]))
		lines.append("学派：" + str(data["school"]))
		lines.append("费用：" + str(data["costText"]))
		lines.append("解析时间：%d    速度：%d" % [int(data["analysisTurns"]), card.speed])
	card.metadata_text = "\n".join(lines)
	return card

func art_id() -> String:
	return illustration_id if not illustration_id.is_empty() else definition_id

func detail_text() -> String:
	return text if metadata_text.is_empty() else metadata_text + "\n\n效果：\n" + text

func rarity_text() -> String:
	return RARITY_LABELS.get(rarity, rarity)

static func anonymous(id: String = "") -> CardPresentation:
	var card: CardPresentation = CardPresentation.new()
	card.instance_id = id
	card.title = "盖伏卡"
	card.type_key = ""
	card.hidden = true
	card.face_down = true
	return card

static func from_projection(data: Dictionary) -> CardPresentation:
	# Sanitize before any renderer, tooltip, art lookup or drag preview sees data.
	if bool(data.get("hidden", false)):
		return anonymous(str(data.get("id", "")))
	var card: CardPresentation = from_definition(data.get("definition", {}))
	card.instance_id = str(data.get("id", ""))
	card.face_down = bool(data.get("faceDown", false))
	var original_cost_detail: String = card.cost_detail()
	card.setting_cost = int(data.get("effectiveSettingCost", card.setting_cost))
	card.cast_cost = int(data.get("effectiveCastCost", card.cast_cost))
	card._update_cost_label()
	card.metadata_text = card.metadata_text.replace(original_cost_detail, card.cost_detail())
	card.status = str(data.get("statusLabel", ""))
	if card.status.is_empty():
		if bool(data.get("faceDown", false)):
			card.status = "盖伏"
		elif int(data.get("spellState", 0)) == 1:
			card.status = "解析剩余 %d" % int(data.get("turnsToReady", 0))
		elif int(data.get("spellState", 0)) == 2:
			card.status = "已解析"
		elif int(data.get("spellState", 0)) == 4:
			card.status = "持续生效"
		elif int(data.get("spellState", 0)) == 5:
			card.status = "已结算 · 结束阶段清理"
	if not card.status.is_empty():
		card.metadata_text += "\n状态：" + card.status
	return card

func _update_cost_label() -> void:
	primary_value = "%d/%d" % [setting_cost, cast_cost] if type_key == "analysis" else str(cast_cost if type_key in ["incantation", "talisman"] else setting_cost)

func cost_detail() -> String:
	if type_key == "talisman":
		return "施法费用：%d（盖伏免费）" % cast_cost
	if type_key == "incantation":
		return "施法费用：%d" % cast_cost
	return "设置费用：%d　施法费用：%d" % [setting_cost, cast_cost]
