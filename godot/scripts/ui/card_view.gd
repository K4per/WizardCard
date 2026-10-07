class_name WizardCardView
extends Control

signal selected(card: Dictionary)
signal secondary(card: Dictionary)
signal reordered(source: String, before: String)

const TYPE_KEYS: Array[String] = ["action", "analytic", "word", "formation", "seal"]
const TYPE_NAMES: Array[String] = ["行动卡", "解析法术", "言灵法术", "阵法卡", "符文卡"]
const SCHOOLS: Dictionary = {"evocation": "塑能", "abjuration": "防护", "divination": "预言", "conjuration": "咒法", "transmutation": "变化", "enchantment": "惑控", "illusion": "幻术", "necromancy": "死灵"}
const RARITIES: Dictionary = {"common": "普通", "uncommon": "罕见", "rare": "稀有", "epic": "史诗", "legendary": "传说"}
var skin: WizardSkin
var card: Dictionary = {}
var definition: Dictionary = {}
var variant: String = "hand"
var highlighted: bool = false
var candidate: bool = false
var drag_enabled: bool = false
var drag_context: String = ""
var drag_generation: String = ""
var drag_revision: String = ""

func configure(resources: WizardSkin, data: Dictionary, kind: String = "hand") -> void:
	skin = resources
	card = data
	definition = data.get("definition", data) as Dictionary
	variant = kind
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	focus_mode = Control.FOCUS_ALL
	mouse_entered.connect(queue_redraw)
	mouse_exited.connect(queue_redraw)
	focus_entered.connect(queue_redraw)
	focus_exited.connect(queue_redraw)
	mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	tooltip_text = "埋伏卡 · 信息隐藏" if bool(card.get("hidden", false)) else str(definition.get("name", "未知卡牌")) + "\n" + str(definition.get("text", ""))
	if variant == "detail" and not bool(card.get("hidden", false)):
		var frame: TextureRect = TextureRect.new()
		frame.texture = skin.texture("master_" + TYPE_KEYS[clampi(int(definition.get("type", 0)), 0, 4)])
		frame.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		frame.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
		frame.mouse_filter = Control.MOUSE_FILTER_IGNORE
		frame.show_behind_parent = true
		add_child(frame)
		frame.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	queue_redraw()

func _gui_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_accept"):
		selected.emit(card)
		accept_event()
	if event is InputEventMouseButton:
		var click: InputEventMouseButton = event as InputEventMouseButton
		if not click.pressed and click.button_index == MOUSE_BUTTON_LEFT and not get_viewport().gui_is_dragging():
			selected.emit(card)
		elif click.pressed and click.button_index == MOUSE_BUTTON_RIGHT:
			if get_viewport().gui_is_dragging():
				get_viewport().gui_cancel_drag()
				accept_event()
				return
			secondary.emit(card)
			accept_event()

func _get_drag_data(_position: Vector2) -> Variant:
	if not drag_enabled or bool(card.get("hidden", false)):
		return null
	var preview: WizardCardView = WizardCardView.new()
	preview.configure(skin, card, "hand")
	preview.size = Vector2(108, 156)
	preview.mouse_filter = Control.MOUSE_FILTER_IGNORE
	preview.modulate.a = 0.85
	set_drag_preview(preview)
	return {"wizard_card": true, "context": drag_context, "id": str(card.get("id", definition.get("id", ""))), "generation": drag_generation, "revision": drag_revision}

func _can_drop_data(_position: Vector2, data: Variant) -> bool:
	return drag_context == "match" and variant == "hand" and data is Dictionary and str((data as Dictionary).get("context", "")) == "match" and bool((data as Dictionary).get("wizard_card", false))

func _drop_data(_position: Vector2, data: Variant) -> void:
	reordered.emit(str((data as Dictionary)["id"]), str(card["id"]))

func _draw() -> void:
	if skin == null or definition.is_empty():
		return
	if bool(card.get("hidden", false)):
		_fit(skin.texture("master_back" if variant == "detail" else "card_back_hand"), Rect2(Vector2.ZERO, size))
		return
	var key: String = TYPE_KEYS[clampi(int(definition.get("type", 0)), 0, 4)]
	if variant == "detail":
		_draw_detail(key)
	else:
		draw_style_box(WizardSkin.flat(skin.color("cream"), skin.color("bronze1")), Rect2(Vector2.ZERO, size))
		if variant == "strip":
			_fit(skin.illustration(str(definition.get("id", ""))), Rect2(3, 3, 26, size.y - 6))
			_text(str(definition.get("name", "")), Vector2(34, size.y / 2 + 5), WizardSkin.HELPER, size.x - 40)
		else:
			_lines(str(definition.get("name", "")), Vector2(6, 18), 14, size.x - 12, 2)
			_fit(skin.illustration(str(definition.get("id", ""))), Rect2(6, 43, size.x - 12, size.y - 89))
			var cost_y: float = size.y - 40
			_fit(skin.texture("analysis_cost" if int(definition.get("type", 0)) == 1 else "mana"), Rect2(6, cost_y, 16, 16))
			_text(str(definition.get("cost", 0)), Vector2(24, cost_y + 14), 14, 22)
			if int(definition.get("type", 0)) == 1:
				_fit(skin.texture("cast_cost"), Rect2(size.x / 2, cost_y, 16, 16))
				_text(str(definition.get("castCost", 0)), Vector2(size.x / 2 + 18, cost_y + 14), 14, 22)
			_text(_state(), Vector2(6, size.y - 8), WizardSkin.CARD_STATE, size.x - 12)
	if has_focus() or Rect2(Vector2.ZERO, size).has_point(get_local_mouse_position()):
		draw_rect(Rect2(Vector2(1, 1), size - Vector2(2, 2)), skin.color("bronze1"), false, 2)
	if highlighted or candidate:
		draw_rect(Rect2(Vector2(1, 1), size - Vector2(2, 2)), skin.color("teal1") if candidate else skin.color("gold"), false, 3)

func _draw_detail(_key: String) -> void:
	# Artwork scales; text is always measured in final display pixels.
	_lines(str(definition.get("name", "")), Vector2(size.x * 0.30, size.y * 0.105), 14, size.x * 0.51, 2, skin.color("cream"))
	_fit(skin.illustration(str(definition.get("id", ""))), Rect2(size * Vector2(0.15, 0.20), size * Vector2(0.70, 0.29)))
	var stats: Array[Dictionary] = attribute_rows(definition)
	for index: int in range(mini(stats.size(), 6)):
		var at: Vector2 = Vector2(size.x * 0.13 + (index % 3) * size.x * 0.25, size.y * 0.55 + (index / 3) * 22)
		_fit(skin.texture(str(stats[index]["icon"])), Rect2(at, Vector2(16, 16)))
		var words: PackedStringArray = str(stats[index]["text"]).split(" ")
		_text(words[words.size() - 1], at + Vector2(18, 14), 14, size.x * 0.25 - 18)
	_lines(str(definition.get("text", "")), Vector2(size.x * 0.15, size.y * 0.73), 14, size.x * 0.70, 2)
	_text(TYPE_NAMES[int(definition.get("type", 0))], Vector2(size.x * 0.15, size.y * 0.93), 14, size.x * 0.65, skin.color("cream"))

func _lines(value: String, at: Vector2, font_size: int, width: float, maximum: int, tint: Color = Color("#322324")) -> void:
	var remaining: String = value.replace("\n", " ")
	for row: int in range(maximum):
		if remaining.is_empty():
			return
		if row == maximum - 1:
			_text(remaining, at + Vector2(0, row * (font_size + 3)), font_size, width, tint)
			return
		var count: int = 0
		while count < remaining.length() and skin.font.get_string_size(remaining.substr(0, count + 1), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x <= width:
			count += 1
		_text(remaining.substr(0, count), at + Vector2(0, row * (font_size + 3)), font_size, width, tint)
		remaining = remaining.substr(count)

static func attributes(data: Dictionary) -> Array[String]:
	var result: Array[String] = []
	for row: Dictionary in attribute_rows(data):
		result.append(str(row["text"]))
	return result

static func attribute_rows(data: Dictionary) -> Array[Dictionary]:
	var kind: int = int(data.get("type", 0))
	var result: Array[Dictionary] = []
	if kind == 3:
		return [{"icon": "mana", "text": "设置 %d" % int(data.get("cost", 0))}, {"icon": "body", "text": "阵体 %d" % int(data.get("body", 0))}, {"icon": "ring_slot", "text": "环位 %d" % int(data.get("rings", 0))}, {"icon": "capacity", "text": "容量 %d" % int(data.get("capacity", 0))}, {"icon": "mana_income", "text": "收入 %d" % int(data.get("income", 0))}, {"icon": "rank", "text": "上限 %d" % int(data.get("maxRank", 0))}]
	result.append({"icon": "analysis_cost" if kind == 1 else "mana", "text": ("解析 " if kind == 1 else "费用 ") + str(data.get("cost", 0))})
	if kind == 1:
		result.append({"icon": "cast_cost", "text": "施法 %d" % int(data.get("castCost", 0))})
	if kind in [1, 2]:
		result.append({"icon": "rank", "text": "位阶 %d" % int(data.get("rank", 0))})
		result.append({"icon": "speed", "text": "速度 %d" % int(data.get("speed", 0))})
	if kind == 1:
		result.append({"icon": "state_analyzing", "text": "解析 %d回合" % int(data.get("analysisTurns", 1))})
	return result

func _state() -> String:
	if bool(card.get("base", false)):
		return "基础 · 环%d/%d" % [int(card.get("occupiedRings", 0)), int(card.get("effectiveRings", 0))]
	if card.has("spellState") and int(card["spellState"]) > 0:
		if int(card["spellState"]) == 1:
			return "解析%d回合" % int(card.get("turnsToReady", 0))
		return ["", "解析中", "解析完成", "待释放", "持续生效"][clampi(int(card["spellState"]), 0, 4)]
	return TYPE_NAMES[int(definition.get("type", 0))]

func _text(value: String, at: Vector2, font_size: int, width: float, tint: Color = Color("#322324")) -> void:
	var fitted: String = value
	if skin.font.get_string_size(fitted, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > width:
		while not fitted.is_empty() and skin.font.get_string_size(fitted + "…", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > width:
			fitted = fitted.left(fitted.length() - 1)
		fitted += "…"
	draw_string(skin.font, at, fitted, HORIZONTAL_ALIGNMENT_LEFT, width, font_size, tint)

func _fit(texture: Texture2D, rectangle: Rect2) -> void:
	if texture == null:
		return
	var extent: Vector2 = texture.get_size()
	var factor: float = minf(rectangle.size.x / extent.x, rectangle.size.y / extent.y)
	draw_texture_rect(texture, Rect2(rectangle.position + (rectangle.size - extent * factor) / 2, extent * factor), false)
