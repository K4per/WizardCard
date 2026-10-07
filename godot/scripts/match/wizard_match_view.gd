class_name WizardMatchView
extends Control

signal requested(action: String, argument: Variant)
signal selected(card: Dictionary, generation: String, revision: String)
signal inspected(card: Dictionary, generation: String, revision: String)
signal dropped(data: Dictionary, destination: int, target: String)
signal reordered(source: String, before: String, generation: String, revision: String)
@onready var board: WizardBattlefield = $HUD/Content/BoardSlot/Board
@onready var hand: WizardHandFan = $HUD/HandSlot/Hand
@onready var details: VBoxContainer = $HUD/Content/DetailsPanel/Details
@onready var actions: VBoxContainer = $HUD/ActionScroll/Actions
@onready var toolbar: HBoxContainer = $HUD/Toolbar
@onready var stats: Label = $HUD/Stats
@onready var log_column: VBoxContainer = $HUD/Content/LogPanel/Log
@onready var hand_header: HBoxContainer = $HUD/HandHeader
@onready var curtain: PanelContainer = $Curtain
@onready var curtain_text: Label = $Curtain/Center/Column/Text
@onready var uncover_button: Button = $Curtain/Center/Column/Uncover
var view: Dictionary = {}
var skin: WizardSkin
var widgets: WizardWidgets
var phase_buttons: Array[Button] = []
var browser: AcceptDialog
var _browser_key: String = ""
var _generation: String = ""
var _revision: String = ""
var _cue_key: String = ""
var _log_pinned: bool = false

func _ready() -> void:
	board.selected.connect(func(card: Dictionary) -> void: selected.emit(card, _generation, _revision))
	board.inspected.connect(func(card: Dictionary) -> void: inspected.emit(card, _generation, _revision))
	board.dropped.connect(func(data: Dictionary, destination: int, target: String) -> void: dropped.emit(data, destination, target))
	board.browsed.connect(browse_zone)
	hand.selected.connect(func(card: Dictionary, generation: String, revision: String) -> void: selected.emit(card, generation, revision))
	hand.canceled.connect(func() -> void: requested.emit("cancel", null))
	hand.inspected.connect(func(card: Dictionary) -> void: inspected.emit(card, _generation, _revision))
	hand.reordered.connect(func(source: String, before: String, generation: String, revision: String) -> void: reordered.emit(source, before, generation, revision))
	uncover_button.pressed.connect(func() -> void: requested.emit("uncover", null))

func initialize(resources: WizardSkin, audio: WizardAudio) -> void:
	skin = resources
	widgets = WizardWidgets.new(skin, audio)
	theme = skin.theme
	var overlay_theme: Theme = skin.theme.duplicate() as Theme
	overlay_theme.set_color("font_color", "Label", Color("f1e4c7"))
	overlay_theme.set_color("default_color", "RichTextLabel", Color("f1e4c7"))
	for panel: Control in [$HUD/Content/DetailsPanel, $HUD/Content/LogPanel, $HUD/ActionScroll]:
		panel.theme = overlay_theme
	($HUD/Content/DetailsPanel as PanelContainer).add_theme_stylebox_override("panel", skin.style("nm_hud_detail", 12, 12))
	($HUD/Content/LogPanel as PanelContainer).add_theme_stylebox_override("panel", skin.style("nm_hud_chain", 12, 12))
	($HUD/ActionScroll as ScrollContainer).add_theme_stylebox_override("panel", skin.style("nm_hud_action", 12, 12))
	var resources_panel: Panel = Panel.new()
	resources_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	resources_panel.show_behind_parent = true
	resources_panel.add_theme_stylebox_override("panel", skin.style("nm_hud_resources", 8, 4))
	stats.add_child(resources_panel)
	resources_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	curtain.add_theme_stylebox_override("panel", WizardSkin.flat(Color("201d18"), skin.color("gold")))
	curtain_text.add_theme_color_override("font_color", skin.color("cream"))
	for phase: String in ["1 抽卡", "2 准备", "3 主要", "4 施法", "5 结束"]:
		var button: Button = widgets.button(toolbar, phase, func() -> void: pass)
		button.disabled = true
		button.add_theme_font_size_override("font_size", 14)
		phase_buttons.append(button)
	widgets.spacer(toolbar)
	widgets.button(toolbar, "详情", func() -> void: $HUD/Content/DetailsPanel.visible = not $HUD/Content/DetailsPanel.visible)
	widgets.button(toolbar, "日志", func() -> void:
		_log_pinned = not $HUD/Content/LogPanel.visible
		$HUD/Content/LogPanel.visible = _log_pinned)
	widgets.button(toolbar, "区域浏览", browse_zone.bind(0, 3))
	widgets.button(toolbar, "设置", func() -> void: requested.emit("settings", null))
	widgets.button(toolbar, "保存", func() -> void: requested.emit("save", null))
	widgets.button(toolbar, "离开", func() -> void: requested.emit("leave_request", null))
	widgets.button(hand_header, "按类型", func() -> void: requested.emit("sort", false))
	widgets.button(hand_header, "按费用", func() -> void: requested.emit("sort", true))
	for bar: HBoxContainer in [toolbar, hand_header]:
		for node: Node in bar.get_children():
			if node is Button:
				(node as Button).theme_type_variation = "MatchButton"

func apply(snapshot: Dictionary, interaction: Dictionary, order: Array[String], covered: bool, reduced: bool) -> void:
	var key: String = str(snapshot["generation"]) + ":" + str(snapshot["viewer"]) + ":" + str(snapshot["revision"])
	if key != _browser_key:
		close_browser()
		_browser_key = key
	_generation = str(snapshot["generation"])
	_revision = str(snapshot["revision"])
	view = snapshot.duplicate(true)
	curtain.visible = covered
	$HUD.visible = not covered
	if covered:
		cancel_drag()
		board.clear_private()
		hand.clear_cards()
		_clear(details)
		_clear(log_column)
		_clear(actions)
		curtain_text.text = "换手遮挡 · 请玩家%d接手" % [int(view["actingPlayer"]) + 1]
		board.input_enabled = false
		return
	board.input_enabled = true
	board.apply(skin, snapshot, interaction, reduced)
	hand.apply(skin, snapshot, interaction, order)
	var phase: int = int(view["phase"])
	for index: int in range(phase_buttons.size()):
		phase_buttons[index].modulate = Color("fff1b1") if index == phase else Color("b1b4ae")
	var lines: PackedStringArray = []
	for player: int in [1 - int(view["viewer"]), int(view["viewer"])]:
		var data: Dictionary = (view["players"] as Array)[player] as Dictionary
		lines.append("%s  生命 %d/40  魔素 %d  荷载 %d/%d  牌库 %d  手牌 %d" % ["本方" if player == int(view["viewer"]) else "对手", int(data["life"]), int(data["mana"]), int(data["load"]), int(data["capacity"]), int(data["deckCount"]), int(data["handCount"])])
	var active: int = int(view["activePlayer"])
	var turn: int = int(((view["players"] as Array)[active] as Dictionary)["ownTurn"])
	stats.text = "回合 %d · %s行动    |    " % [turn, "本方" if active == int(view["viewer"]) else "对手"] + "    |    ".join(lines)
	stats.add_theme_font_size_override("font_size", 14)
	stats.clip_text = true
	stats.tooltip_text = "\n".join(lines)
	_clear(log_column)
	widgets.label(log_column, "公开连锁 / 日志", 16)
	var chain: Dictionary = view["chain"] as Dictionary if view.get("chain") != null else {}
	$HUD/Content/LogPanel.visible = _log_pinned or not chain.is_empty()
	if not chain.is_empty():
		widgets.icon_label(log_column, "nm_chain_priority", "响应：玩家%d" % [int(chain["priority"]) + 1], 14)
		if int(chain.get("mode", 0)) == 1:
			widgets.icon_label(log_column, "nm_chain_resolve", "逆序结算 · 最新链节优先", 14)
		for raw: Variant in chain["links"] as Array:
			var link: Dictionary = raw as Dictionary
			var id: String = str(link["id"])
			var state: String = "cancel" if bool(link.get("canceled", false)) else ("resolve" if int(chain.get("mode", 0)) == 1 else "enter")
			var link_button: Button = widgets.button(log_column, "#%s %s" % [id, str(link["kindName"])], func() -> void: requested.emit("link", id), "nm_chain_" + state, "MatchButton")
			link_button.tooltip_text = "来源 #%s · 目标 #%s · 速度 %d%s" % [str(link.get("source", "0")), str(link.get("target", "0")), int(link.get("speed", 0)), " · 已取消" if state == "cancel" else ""]
	var log: String = ""
	var events: Array = view["events"] as Array
	for index: int in range(maxi(0, events.size() - 12), events.size()):
		log += str((events[index] as Dictionary)["text"]) + "\n"
	widgets.text(log_column, log)

func reveal_details() -> void:
	$HUD/Content/DetailsPanel.show()

func _clear(node: Node) -> void:
	for child: Node in node.get_children():
		node.remove_child(child)
		child.queue_free()

func browse_zone(side: int, zone: int) -> void:
	if curtain.visible or view.is_empty():
		return
	close_browser()
	browser = AcceptDialog.new()
	browser.title = "公开区域 · 点选查看 / 选择合法目标"
	browser.theme = skin.theme
	var column: VBoxContainer = widgets.column(browser)
	var selectors: HBoxContainer = widgets.row(column)
	var owner: OptionButton = widgets.option(selectors, ["本方", "对手"], side)
	var zone_ids: Array[int] = [3, 5, 4, 2, 6, 7]
	var area: OptionButton = widgets.option(selectors, ["解析区", "施法区", "言灵区", "行动区", "灰烬区", "附着符文"], maxi(0, zone_ids.find(zone)))
	var scroll: ScrollContainer = widgets.scroll(column)
	scroll.custom_minimum_size = Vector2(620, 340)
	var grid: GridContainer = GridContainer.new()
	grid.columns = 4
	scroll.add_child(grid)
	var fill: Callable = func() -> void:
		_clear(grid)
		var player: int = int(view["viewer"]) if owner.selected == 0 else 1 - int(view["viewer"])
		for raw: Variant in view["cards"] as Array:
			var card: Dictionary = raw as Dictionary
			if int(card["owner"]) != player or int(card["zone"]) != zone_ids[area.selected]:
				continue
			var node: WizardCardView = widgets.card(grid, card, "hand", Vector2(135, 180))
			var generation: String = _generation
			var revision: String = _revision
			node.selected.connect(func(data: Dictionary) -> void:
				close_browser()
				selected.emit(data, generation, revision))
	owner.item_selected.connect(func(_index: int) -> void: fill.call())
	area.item_selected.connect(func(_index: int) -> void: fill.call())
	add_child(browser)
	fill.call()
	browser.confirmed.connect(close_browser)
	browser.close_requested.connect(close_browser)
	browser.popup_centered(Vector2i(680, 440))

func close_browser() -> void:
	if is_instance_valid(browser):
		browser.hide()
		browser.queue_free()
		browser = null

func cancel_drag() -> void:
	if get_viewport().gui_is_dragging():
		get_viewport().gui_cancel_drag()
	hand.hovered = ""
	hand.arrange()

func set_paused(paused: bool) -> void:
	cancel_drag()
	close_browser()
	board.set_paused(paused)
	hand.set_paused(paused)

func play_cues(batch: Dictionary) -> void:
	var key: String = str(view["generation"]) + ":" + str(view["viewer"]) + ":" + str(view["revision"])
	if curtain.visible or key == _cue_key or str(batch.get("generation", "")) != _generation or str(batch.get("revision", "")) != _revision:
		return
	_cue_key = key
	board.play_cues(batch)
	for raw: Variant in batch.get("cues", []) as Array:
		var cue: Dictionary = raw as Dictionary
		if int(cue["kind"]) == 1 and int(cue["player"]) == int(view["viewer"]):
			hand.draw_card(str(cue["source"]), board.reduced)

func _notification(what: int) -> void:
	if is_node_ready() and what in [NOTIFICATION_WM_WINDOW_FOCUS_OUT, NOTIFICATION_WM_MOUSE_EXIT]:
		cancel_drag()

func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT and get_viewport().gui_is_dragging():
		cancel_drag()
		get_viewport().set_input_as_handled()
