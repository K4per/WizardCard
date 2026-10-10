class_name CardPresentation
extends Resource

@export var instance_id: String = ""
@export var definition_id: String = ""
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

static func from_definition(data: Dictionary) -> CardPresentation:
	var card: CardPresentation = CardPresentation.new()
	card.definition_id = str(data.get("id", ""))
	card.instance_id = card.definition_id
	card.title = str(data.get("name", ""))
	card.text = str(data.get("text", ""))
	card.rarity = str(data.get("rarity", "common"))
	card.speed = int(data.get("speed", 1))
	# Legacy action cards keep their actual label; this is not a migration decision.
	var type_index: int = int(data.get("type", 1))
	card.type_key = ["incantation", "analysis", "incantation", "formation", "rune"][clampi(type_index, 0, 4)]
	card.type_label = str(data.get("typeLabel", ["行动（旧版）", "解析", "言灵", "阵法", "符文"][clampi(type_index, 0, 4)]))
	card.primary_value = str(data.get("cost", 0))
	card.secondary_value = str(data.get("rank", 0))
	return card

static func anonymous(id: String = "") -> CardPresentation:
	var card: CardPresentation = CardPresentation.new()
	card.instance_id = id
	card.title = "盖伏卡"
	card.type_key = ""
	card.hidden = true
	return card

static func from_projection(data: Dictionary) -> CardPresentation:
	# Sanitize before any renderer, tooltip, art lookup or drag preview sees data.
	if bool(data.get("hidden", false)):
		return anonymous(str(data.get("id", "")))
	var card: CardPresentation = from_definition(data.get("definition", {}))
	card.instance_id = str(data.get("id", ""))
	card.status = str(data.get("statusLabel", ""))
	return card
