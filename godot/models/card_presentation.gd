class_name CardPresentation
extends Resource

const TYPE_KEYS: Array[String] = [
	"terms.card_type.legacy_action", "terms.card_type.analysis", "terms.card_type.word",
	"terms.card_type.formation", "terms.card_type.rune", "terms.card_type.talisman",
]

const RARITY_KEYS: Dictionary[String, String] = {
	"common": "terms.rarity.common",
	"uncommon": "terms.rarity.uncommon",
	"rare": "terms.rarity.rare",
	"epic": "terms.rarity.epic",
	"legendary": "terms.rarity.legendary",
}

@export var instance_id: String = ""
@export var definition_id: String = ""
@export var number: String = ""
@export var title: String = ""
@export var type_key: String = "analysis"
@export var type_label: String = ""
@export var text: String = ""
@export var flavor: String = ""
@export var rarity: String = "common"
@export var primary_value: String = ""
@export var secondary_value: String = ""
@export var speed: int = 1
@export var status: String = ""
@export var hidden: bool = false
@export var face_down: bool = false
@export var setting_cost: int = 0
@export var cast_cost: int = 0
@export var dynamic_cost: bool = false
@export var illustration_id: String = ""
@export var metadata_text: String = ""
@export var preview: bool = false
@export var storm_points: int = 0
@export var wind_contract: bool = false

func token_label() -> String:
	if wind_contract:
		return GameText.label("ui.card.wind_contract_tokens", {"count": storm_points})
	if storm_points != 0:
		return GameText.label("ui.card.storm_points", {"points": storm_points})
	return ""

static func from_definition(data: Dictionary) -> CardPresentation:
	var card: CardPresentation = CardPresentation.new()
	card.definition_id = str(data.get("id", ""))
	card.number = str(data.get("number", ""))
	card.illustration_id = str(data.get("illustrationId", ""))
	card.instance_id = card.definition_id
	card.title = str(data.get("name", ""))
	card.text = str(data.get("text", ""))
	card.flavor = str(data.get("flavor", ""))
	card.rarity = str(data.get("rarity", "common"))
	card.speed = int(data.get("speed", 1))
	var type_index: int = int(data.get("type", 1))
	card.type_key = ["incantation", "analysis", "incantation", "formation", "rune", "talisman"][clampi(type_index, 0, 5)]
	card.type_label = str(data["typeLabel"]) if data.has("typeLabel") else GameText.label(TYPE_KEYS[clampi(type_index, 0, 5)])
	card.setting_cost = int(data.get("cost", 0)) if type_index != 5 else 0
	card.cast_cost = int(data.get("castCost", 0))
	card.dynamic_cost = bool(data.get("dynamicCost", false))
	card._update_cost_label()
	card.secondary_value = str(data.get("level", data.get("rank", 0)))
	var lines: Array[String] = []
	lines.append(GameText.label("ui.card.level", {"level": int(data.get("level", data.get("rank", 0)))}))
	if type_index == 3:
		lines.append(GameText.label("ui.card.body_and_rings", {"body": int(data.get("body", 0)), "rings": int(data.get("rings", 0))}))
		lines.append(GameText.label("ui.card.capacity_and_income", {"capacity": int(data.get("capacity", 0)), "income": int(data.get("income", 0))}))
		lines.append(GameText.label("ui.card.setting_condition", {"condition": formation_condition(data)}))
	else:
		var school: String = str(data.get("school", ""))
		if not school.is_empty():
			lines.append(GameText.label("ui.card.faction", {"school": (GameText.label("terms.school_short." + school) if GameText.shared().has_key("terms.school_short." + school) else school)}))
		lines.append(card.cost_detail())
		lines.append(GameText.label("ui.card.analysis_and_speed", {"turns": int(data.get("analysisTurns", 0)), "speed": card.speed}))
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
	var lines: Array[String] = [GameText.label("ui.card.level", {"level": int(data["level"])})]
	if card.type_key == "formation":
		lines.append(GameText.label("ui.card.preview_body_and_rings", {"body": int(data["body"]), "rings": int(data["rings"])}))
		lines.append(GameText.label("ui.card.preview_capacity_and_income", {"capacity": int(data["capacity"]), "income": int(data["income"])}))
		lines.append(GameText.label("ui.card.summon_condition", {"condition": str(data["summonCondition"])}))
	else:
		var costs: Array = data["costParts"]
		var cost_text: String = str(data["costText"])
		card.primary_value = cost_text if cost_text == "X" else str(int(costs[0]))
		lines.append(GameText.label("ui.card.school", {"school": str(data["school"])}))
		lines.append(GameText.label("ui.card.cost", {"cost": cost_text}))
		lines.append(GameText.label("ui.card.preview_analysis_and_speed", {"turns": int(data["analysisTurns"]), "speed": card.speed}))
	card.metadata_text = "\n".join(lines)
	return card

func art_id() -> String:
	return illustration_id if not illustration_id.is_empty() else definition_id

static func escape_bbcode(value: String) -> String:
	return value.replace("[", "[lb]")

func detail_text(rich: bool = false) -> String:
	var result: String = (metadata_text + "\n\n" if not metadata_text.is_empty() else "") + GameText.label("ui.card.effect", {"effect": text})
	if rich:
		result = escape_bbcode(result)
	if not flavor.is_empty():
		result += "\n\n" + ("[i]" + escape_bbcode(flavor) + "[/i]" if rich else flavor)
	return result

func rarity_text() -> String:
	return GameText.label(RARITY_KEYS[rarity]) if RARITY_KEYS.has(rarity) else rarity

static func anonymous(id: String = "") -> CardPresentation:
	var card: CardPresentation = CardPresentation.new()
	card.instance_id = id
	card.title = GameText.label("ui.card.hidden")
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
	var definition_data: Dictionary = data.get("definition", {}) as Dictionary
	var card_tags: Array = definition_data.get("tags", []) as Array
	# Stable content tags used by existing card definitions.
	card.wind_contract = card_tags.has("风契")
	if card_tags.has("风暴") or card.wind_contract:
		card.storm_points = int(data.get("stormPoints", 0))
	var original_cost_detail: String = card.cost_detail()
	card.setting_cost = int(data.get("effectiveSettingCost", card.setting_cost))
	card.cast_cost = int(data.get("effectiveCastCost", card.cast_cost))
	card._update_cost_label()
	card.metadata_text = card.metadata_text.replace(original_cost_detail, card.cost_detail())
	var token_text: String = card.token_label()
	if not token_text.is_empty():
		card.metadata_text += "\n" + token_text
	card.status = str(data.get("statusLabel", ""))
	if int(data.get("zone", -1)) in [6, 10]:
		card.status = GameText.label("ui.card.departed", {"zone": GameText.term(data, "zoneName", GameText.label("ui.zone.ash"))})
	if card.status.is_empty():
		if bool(data.get("faceDown", false)):
			card.status = GameText.label("ui.card.face_down")
		elif int(data.get("spellState", 0)) == 1:
			card.status = GameText.label("ui.card.analysis_remaining", {"turns": int(data.get("turnsToReady", 0))})
		elif int(data.get("spellState", 0)) == 2:
			card.status = GameText.label("ui.card.ready")
		elif int(data.get("spellState", 0)) == 4:
			card.status = GameText.label("ui.card.ongoing")
			if int(definition_data.get("duration", 0)) > 0:
				card.status += GameText.label("ui.card.duration_remaining", {"turns": int(data.get("remaining", 0))})
		elif int(data.get("spellState", 0)) == 5:
			card.status = GameText.label("ui.card.resolved")
	for effect_status: Variant in data.get("statuses", []):
		card.status += ("\n" if not card.status.is_empty() else "") + str(effect_status)
	if not card.status.is_empty():
		card.metadata_text += GameText.label("ui.card.status", {"status": card.status})
	return card

func _update_cost_label() -> void:
	if dynamic_cost:
		primary_value = "X"
		return
	primary_value = "%d/%d" % [setting_cost, cast_cost] if type_key in ["analysis", "incantation"] else str(cast_cost if type_key == "talisman" else setting_cost)

func cost_detail() -> String:
	if type_key == "formation":
		return ""
	if type_key == "talisman":
		return GameText.label("ui.card.talisman_cost", {"cost": ("X" if dynamic_cost else str(cast_cost))})
	return GameText.label("ui.card.spell_cost", {"setting": setting_cost, "casting": cast_cost})
