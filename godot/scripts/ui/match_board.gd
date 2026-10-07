class_name WizardMatchBoard
extends VBoxContainer

signal selected(card: Dictionary)
signal dropped(data: Dictionary, destination: int, target: String)

var skin: WizardSkin
var widgets: WizardWidgets
var view: Dictionary = {}
var selection: Dictionary = {}
var viewer: int = 0
var zones: Dictionary = {}
var hand: HBoxContainer
var hand_order: Array[String] = []
var compact: bool = false

func configure(resources: WizardSkin, snapshot: Dictionary, interaction: Dictionary, order: Array[String]) -> void:
	skin = resources
	widgets = WizardWidgets.new(skin)
	view = snapshot
	selection = interaction
	viewer = int(view["viewer"])
	hand_order = order
	compact = get_viewport_rect().size.y < 1050
	size_flags_horizontal = Control.SIZE_EXPAND_FILL
	size_flags_vertical = Control.SIZE_EXPAND_FILL
	add_theme_constant_override("separation", 5)
	_side(1 - viewer, true)
	if not compact:
		var divider: Label = widgets.label(self, "──────── 奥术对决 · 双方五区 ────────", WizardSkin.HELPER)
		divider.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_side(viewer, false)

func _side(player: int, opponent: bool) -> void:
	var side: VBoxContainer = widgets.column(self, true)
	side.add_theme_constant_override("separation", 4)
	var stats: Dictionary = (view["players"] as Array)[player] as Dictionary
	var heading: HBoxContainer = widgets.row(side)
	widgets.label(heading, "对手" if opponent else "本方", WizardSkin.BODY)
	widgets.icon_label(heading, "life", "%d / 40" % int(stats["life"]))
	widgets.icon_label(heading, "mana", "魔素 %d" % int(stats["mana"]))
	widgets.icon_label(heading, "load", "荷载 %d/%d" % [int(stats["load"]), int(stats["capacity"])])
	widgets.spacer(heading)
	widgets.icon_label(heading, "zone_deck", "牌库 %d · 手牌 %d" % [int(stats["deckCount"]), int(stats["handCount"])], WizardSkin.HELPER)
	if not opponent:
		_zone(side, player, 5, "施法区", "casting", false)
	var area: HBoxContainer = widgets.row(side, true)
	if not opponent:
		var analysis: WizardZoneView = _zone(area, player, 3, "解析区 · 阵法与宿主", "analysis", true)
		analysis.size_flags_stretch_ratio = 1.2
	var support: VBoxContainer = widgets.column(area, true)
	support.add_theme_constant_override("separation", 2 if compact else 4)
	support.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	if opponent:
		_zone(support, player, 6, "灰烬区", "ash", false)
		_zone(support, player, 2, "行动区", "action", false)
		_zone(support, player, 4, "言灵区", "words", false)
		var analysis: WizardZoneView = _zone(area, player, 3, "解析区 · 阵法与宿主", "analysis", true)
		analysis.size_flags_stretch_ratio = 1.2
	else:
		_zone(support, player, 4, "言灵区", "words", false)
		_zone(support, player, 2, "行动区", "action", false)
		_zone(support, player, 6, "灰烬区", "ash", false)
	if opponent:
		_zone(side, player, 5, "施法区", "casting", false)

func _zone(parent: Node, player: int, zone: int, title: String, key: String, formations: bool) -> WizardZoneView:
	var panel: WizardZoneView = WizardZoneView.new()
	panel.destination = zone
	panel.accepts_cards = player == viewer
	panel.add_theme_stylebox_override("panel", skin.style("region_" + key, 7, 2 if compact else 4))
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	panel.size_flags_vertical = Control.SIZE_FILL if zone == 5 else Control.SIZE_EXPAND_FILL
	parent.add_child(panel)
	panel.dropped.connect(func(data: Dictionary, destination: int, target: String) -> void: dropped.emit(data, destination, target))
	zones["%d:%d" % [player, zone]] = panel
	var column: VBoxContainer = widgets.column(panel)
	column.add_theme_constant_override("separation", 2)
	var bar: HBoxContainer = widgets.row(column)
	widgets.icon_label(bar, "zone_" + key, title, WizardSkin.HELPER)
	var visible_cards: Array[Dictionary] = _cards(player, zone)
	widgets.spacer(bar)
	widgets.label(bar, str(visible_cards.size()), WizardSkin.HELPER)
	if formations:
		_formations(column, player, visible_cards)
	else:
		var scroll: ScrollContainer = ScrollContainer.new()
		scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
		scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		scroll.custom_minimum_size.y = 20 if compact else 32
		if compact:
			bar.add_child(scroll)
		else:
			column.add_child(scroll)
		var row: HBoxContainer = widgets.row(scroll)
		for data: Dictionary in visible_cards:
			_add_card(row, data, "strip", Vector2(125, 20 if compact else 32))
			for seal: Dictionary in _cards(player, 7):
				if str(seal.get("host", "0")) == str(data["id"]):
					var seal_view: WizardCardView = _add_card(row, seal, "strip", Vector2(125, 20 if compact else 32))
					seal_view.tooltip_text += "\n绑定宿主 #" + str(data["id"])
		if visible_cards.is_empty():
			widgets.label(row, "—", WizardSkin.HELPER)
	return panel

func _formations(parent: Node, player: int, visible_cards: Array[Dictionary]) -> void:
	var scroll: ScrollContainer = widgets.scroll(parent)
	scroll.custom_minimum_size.y = 84 if compact else 104
	var rows: VBoxContainer = widgets.column(scroll)
	rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	rows.add_theme_constant_override("separation", 2)
	var occupied: int = 0
	for formation: Dictionary in visible_cards:
		var definition: Dictionary = formation.get("definition", {}) as Dictionary
		if int(definition.get("type", -1)) != 3:
			continue
		var body: int = int(definition.get("body", 1))
		occupied += body
		var host: WizardZoneView = WizardZoneView.new()
		host.target = str(formation["id"])
		host.destination = 3
		host.accepts_cards = player == viewer
		host.custom_minimum_size.y = (22 if compact else 32) * body
		host.add_theme_stylebox_override("panel", skin.style("slot_span" if body > 1 else "slot_occupied", 3, 2))
		rows.add_child(host)
		host.dropped.connect(func(data: Dictionary, destination: int, target: String) -> void: dropped.emit(data, destination, target))
		var host_scroll: ScrollContainer = ScrollContainer.new()
		host_scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
		host.add_child(host_scroll)
		var row: HBoxContainer = widgets.row(host_scroll)
		_add_card(row, formation, "strip", Vector2(130, 20 if compact else 32))
		var links: HBoxContainer = widgets.row(row)
		var hosts: Array[String] = [str(formation["id"])]
		for spell: Dictionary in visible_cards:
			if str(spell.get("host", "0")) == str(formation["id"]):
				hosts.append(str(spell["id"]))
				_add_card(links, spell, "strip", Vector2(125, 20 if compact else 32))
		for seal: Dictionary in _cards(player, 7):
			if hosts.has(str(seal.get("host", "0"))):
				var seal_view: WizardCardView = _add_card(links, seal, "strip", Vector2(125, 20 if compact else 32))
				seal_view.tooltip_text += "\n绑定宿主 #" + str(seal["host"])
		widgets.label(links, "阵体%d · 环%d/%d" % [body, int(formation.get("occupiedRings", 0)), int(formation.get("effectiveRings", 0))], 12)
	for index: int in range(occupied, 5):
		var empty: PanelContainer = PanelContainer.new()
		empty.add_theme_stylebox_override("panel", skin.style("slot_empty", 3, 0 if compact else 4))
		rows.add_child(empty)
		widgets.label(empty, "阵法槽 %d · 空" % [index + 1], 12)

func _cards(player: int, zone: int) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	for raw: Variant in view["cards"] as Array:
		var data: Dictionary = raw as Dictionary
		if int(data["owner"]) == player and int(data["zone"]) == zone:
			result.append(data)
	return result

func _add_card(parent: Node, data: Dictionary, kind: String, dimensions: Vector2) -> WizardCardView:
	var node: WizardCardView = widgets.card(parent, data, kind, dimensions)
	node.highlighted = str(data["id"]) == str(selection.get("selected", "0"))
	node.candidate = (selection.get("candidates", []) as Array).has(str(data["id"]))
	node.selected.connect(func(card: Dictionary) -> void: selected.emit(card))
	node.drag_enabled = int(data.get("owner", -1)) == viewer and int(data.get("zone", -1)) == 1
	node.drag_context = "match"
	node.drag_generation = str(view["generation"])
	node.drag_revision = str(view["revision"])
	return node
