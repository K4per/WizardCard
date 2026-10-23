class_name PlayerStatusBar
extends PanelContainer

const PANEL_SIZE: Vector2 = Vector2(316, 72)
const EDGE_MARGIN: float = 12.0

var _name: Label
var _life: Label
var _temporary_life: Label
var _mana: Label
var _load: Label

func _ready() -> void:
	var panel: StyleBoxFlat = StyleBoxFlat.new()
	panel.bg_color = Color(0.035, 0.065, 0.085, 0.92)
	panel.border_color = Color("54717b")
	panel.set_border_width_all(1)
	panel.set_corner_radius_all(4)
	panel.content_margin_left = 8
	panel.content_margin_right = 8
	panel.content_margin_top = 4
	panel.content_margin_bottom = 4
	add_theme_stylebox_override(&"panel", panel)

	var column: VBoxContainer = VBoxContainer.new()
	column.add_theme_constant_override(&"separation", 1)
	column.mouse_filter = Control.MOUSE_FILTER_IGNORE
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	add_child(column)
	_name = UiFactory.label("玩家", 12)
	_name.mouse_filter = Control.MOUSE_FILTER_IGNORE
	column.add_child(_name)
	var stats: GridContainer = GridContainer.new()
	stats.columns = 2
	stats.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	stats.add_theme_constant_override(&"h_separation", 10)
	stats.add_theme_constant_override(&"v_separation", 1)
	stats.mouse_filter = Control.MOUSE_FILTER_IGNORE
	column.add_child(stats)
	_life = _add_stat(stats, "res://art/icons/c2d_icon_hud_life.png", "生命", Color("f28a83"))
	_temporary_life = _add_stat(stats, "res://art/icons/c2d_icon_hud_temp-life.png", "临时生命", Color("85d8ec"))
	_mana = _add_stat(stats, "res://art/icons/c2d_icon_hud_mana.png", "魔素", Color("78cfb0"))
	_load = _add_stat(stats, "res://art/icons/c2d_icon_hud_load.png", "荷载", Color("e9c978"))
	custom_minimum_size = PANEL_SIZE

func _add_stat(parent: GridContainer, icon_path: String, caption: String, tint: Color) -> Label:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 4)
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(row)
	var icon: TextureRect = TextureRect.new()
	icon.texture = load(icon_path) as Texture2D
	icon.custom_minimum_size = Vector2(19, 19)
	icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon.modulate = tint
	icon.tooltip_text = caption
	icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	row.add_child(icon)
	var value: Label = UiFactory.label(caption + " —", 14)
	value.add_theme_color_override(&"font_color", tint)
	# An unwrapped minimum width keeps the GridContainer from allocating only
	# the icon width. Resource numbers must remain visible at every resolution.
	value.clip_text = false
	value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	value.mouse_filter = Control.MOUSE_FILTER_PASS
	row.add_child(value)
	return value

static func place_corners(own: PlayerStatusBar, opponent: PlayerStatusBar, viewport_size: Vector2) -> void:
	for bar: PlayerStatusBar in [own, opponent]:
		bar.size = bar.get_combined_minimum_size().max(PANEL_SIZE)
	own.position = Vector2(EDGE_MARGIN, viewport_size.y - own.size.y - EDGE_MARGIN)
	opponent.position = Vector2(viewport_size.x - opponent.size.x - EDGE_MARGIN, EDGE_MARGIN)

func set_status(player_name: String, player_index: int, values: Dictionary, loads: Array = [], cards: Array = [], debug: Dictionary = {}, clock_text: String = "", clock_warning: bool = false) -> void:
	if not is_node_ready():
		return
	_name.text = player_name + (" · " + clock_text if not clock_text.is_empty() else "")
	_name.add_theme_color_override(&"font_color", Color("f0aaaa") if clock_warning else Color("e7e6da"))
	var unlimited_life: bool = bool(debug.get("infiniteLife", values.get("infiniteLife", false)))
	var unlimited_mana: bool = bool(debug.get("infiniteMana", values.get("infiniteMana", false)))
	var unlimited_capacity: bool = bool(debug.get("infiniteLoad", values.get("infiniteLoad", false)))
	_life.text = "生命 " + ("∞" if unlimited_life else str(int(values.get("life", 0))))
	_temporary_life.text = "临时生命 +%d" % int(values.get("temporaryLife", 0))
	_mana.text = "魔素 %s/%d" % ["∞" if unlimited_mana else str(int(values.get("mana", 0))), 20]
	_load.text = "荷载 %d/%s" % [int(values.get("load", 0)), "∞" if unlimited_capacity else str(int(values.get("capacity", 0)))]
	var details: Array[String] = []
	for card: Dictionary in cards:
		if int(card.get("owner", -1)) != player_index:
			continue
		var bound: int = int(card.get("analysisLoad", 0)) + int(card.get("castLoad", 0))
		if bound > 0:
			var source_name: String = str((card.get("definition", {}) as Dictionary).get("name", "盖伏法术"))
			details.append("绑定荷载 · %s · %d" % [source_name, bound])
	for entry: Dictionary in loads:
		if int(entry.get("owner", -1)) != player_index:
			continue
		var source: String = str(entry.get("source", "0"))
		var source_name: String = "未绑定来源"
		for card: Dictionary in cards:
			if str(card.get("id", "0")) == source:
				source_name = str((card.get("definition", {}) as Dictionary).get("name", source_name))
				break
		var kind: String = "独立荷载" if bool(entry.get("independent", false)) else ("绑定荷载" if bool(entry.get("sourceBound", false)) else "临时荷载")
		details.append("%s · %s · %d" % [kind, source_name, int(entry.get("amount", 0))])
	_load.tooltip_text = "荷载来源：\n" + ("\n".join(details) if not details.is_empty() else "暂无额外荷载来源")
	_life.tooltip_text = "当前生命，不含临时生命。"
	_temporary_life.tooltip_text = "临时生命；己方抽卡阶段清除。"
	_mana.tooltip_text = "当前魔素 / 上限 20。"
