class_name WizardClient
extends Control

var bridge: WizardBridge = WizardBridge.new()
var skin: WizardSkin = WizardSkin.new()
var audio: WizardAudio = WizardAudio.new()
var widgets: WizardWidgets
var library: Dictionary = {}
var definitions: Dictionary = {}
var settings: Dictionary = {}
var page: String = "menu"
var mode: String = "hotseat"
var view: Dictionary = {}
var interaction: Dictionary = {}
var viewer: int = 0
var covered: bool = false
var inspected: Dictionary = {}
var draft: Dictionary = {}
var dirty: bool = false
var root_column: VBoxContainer
var status_label: Label
var notice: String = ""
var editor_pool: GridContainer
var editor_deck: GridContainer
var editor_details: VBoxContainer
var editor_problems: Label
var query: Dictionary = {}
var pool_page: int = 0
var hand_order: Array[String] = []
var _hand_orders: Dictionary = {}
var _layout_size: Vector2i = Vector2i.ZERO
var options: Dictionary = {}
var _dialog: ConfirmationDialog
var _choices_dialog: AcceptDialog
var _timer: Timer = Timer.new()
const MATCH_SCENE: PackedScene = preload("res://scenes/match/match_view.tscn")
var match_scene: WizardMatchView
var _cue_batch: Dictionary = {}

func _ready() -> void:
	var assets: String = ProjectSettings.globalize_path("res://../assets")
	var user_directory: String = ""
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(0, arguments.size() - 1, 2):
		if arguments[index] == "--assets":
			assets = arguments[index + 1]
		elif arguments[index] == "--user-data":
			user_directory = arguments[index + 1]
	skin.initialize(assets)
	add_child(audio)
	audio.initialize(assets)
	widgets = WizardWidgets.new(skin, audio)
	theme = skin.theme
	texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	get_tree().auto_accept_quit = false
	if not _result(bridge.initialize(assets, user_directory)):
		_fatal(notice)
		return
	_reload_library()
	if DisplayServer.get_name() != "headless" and not arguments.has("--capture"):
		_apply_window()
	get_viewport().size_changed.connect(_viewport_resized)
	_timer.wait_time = 0.35
	_timer.timeout.connect(_tick)
	add_child(_timer)
	_timer.start()
	show_menu()

func _viewport_resized() -> void:
	var dimensions: Vector2i = Vector2i(get_viewport_rect().size)
	if dimensions != _layout_size:
		_layout_size = dimensions
		if page == "match":
			refresh_match.call_deferred()

func _result(response: Dictionary) -> bool:
	if not bool(response.get("ok", false)):
		notice = str(response.get("error", "操作失败"))
		if is_instance_valid(status_label):
			status_label.text = notice
			status_label.show()
		audio.cue("ui_reject")
		return false
	var data: Dictionary = response.get("data", {}) as Dictionary
	if data.has("cues"):
		_cue_batch = data.duplicate(true)
	for raw: Variant in data.get("sounds", []) as Array:
		audio.play(raw as Dictionary)
	if data.has("replaySaved") and not bool(data["replaySaved"]):
		notice = "操作已接受；复盘保存失败，请重试：" + str(data.get("saveError", ""))
	return true

func _reload_library() -> void:
	var response: Dictionary = bridge.library()
	if not _result(response):
		return
	library = response["data"] as Dictionary
	settings = library["settings"] as Dictionary
	audio.apply_settings(settings)
	definitions.clear()
	for raw: Variant in library["cards"] as Array:
		var data: Dictionary = raw as Dictionary
		definitions[str(data["id"])] = data

func _clear(node: Node) -> void:
	for child: Node in node.get_children():
		node.remove_child(child)
		child.queue_free()

func _page(name: String, title: String) -> void:
	if is_instance_valid(match_scene):
		match_scene.cancel_drag()
		if name in ["match", "settings"]:
			match_scene.reparent(self)
			match_scene.hide()
			match_scene.set_paused(name == "settings")
		else:
			match_scene.get_parent().remove_child(match_scene)
			match_scene.queue_free()
			match_scene = null
			_cue_batch.clear()
	page = name
	if is_instance_valid(root_column):
		var margin: Node = root_column.get_parent()
		remove_child(margin)
		margin.queue_free()
	var margin: MarginContainer = MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side: String in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 12)
	add_child(margin)
	root_column = widgets.column(margin)
	root_column.add_theme_constant_override("separation", 4)
	var header: HBoxContainer = widgets.row(root_column)
	widgets.label(header, title, WizardSkin.TITLE)
	header.visible = name not in ["menu", "match"]
	widgets.spacer(header)
	if name != "menu" and name != "match" and name != "settings" and name != "editor":
		widgets.button(header, "返回标题", show_menu)
	status_label = widgets.label(root_column, notice, WizardSkin.HELPER)
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	status_label.visible = not notice.is_empty()
	queue_redraw()

func _draw() -> void:
	if skin.palette.is_empty():
		return
	draw_rect(Rect2(Vector2.ZERO, size), skin.color("bg"))
	var background: Texture2D = skin.texture("match_background")
	if background != null:
		draw_texture_rect(background, Rect2(Vector2.ZERO, size), false, Color(1, 1, 1, 0.4))

func _fatal(message: String) -> void:
	_page("error", "资源或内容未加载")
	widgets.text(root_column, message)

func show_menu() -> void:
	_reload_library()
	notice = str(library.get("notice", ""))
	_page("menu", "巫师牌 · WizardCard")
	var center: CenterContainer = CenterContainer.new()
	center.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root_column.add_child(center)
	var menu: VBoxContainer = widgets.column(center)
	menu.add_theme_constant_override("separation", 12)
	var logo: TextureRect = TextureRect.new()
	logo.texture = skin.image(skin.directory.path_join("art/branding/wizardcard-logo-v1.png"))
	logo.custom_minimum_size = Vector2(460, 150)
	logo.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	logo.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	menu.add_child(logo)
	var subtitle: Label = widgets.label(menu, "构筑法阵 · 编织言灵", WizardSkin.BODY)
	subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var labels: Array[String] = ["单人 AI 对战", "本地换手对战", "卡组构筑", "游戏设置", "退出游戏"]
	var callbacks: Array[Callable] = [show_setup.bind("ai"), show_setup.bind("hotseat"), show_decks, show_settings, _request_quit]
	for index: int in range(labels.size()):
		var button: Button = widgets.button(menu, labels[index], callbacks[index], "", "PrimaryButton")
		button.custom_minimum_size = Vector2(320, 48)
		button.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	var version: Label = widgets.label(root_column, "v1.5 开发版 · 规则 / 卡池 1.0.0", WizardSkin.HELPER)
	version.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER

func _form() -> VBoxContainer:
	var center: CenterContainer = CenterContainer.new()
	center.size_flags_vertical = Control.SIZE_EXPAND_FILL
	root_column.add_child(center)
	var panel: PanelContainer = widgets.panel(center)
	panel.custom_minimum_size.x = 640
	var column: VBoxContainer = widgets.column(panel)
	column.add_theme_constant_override("separation", 12)
	return column

func show_setup(session_mode: String) -> void:
	mode = session_mode
	_reload_library()
	_page("setup", "单人AI开局" if mode == "ai" else "本地换手开局")
	var panel: VBoxContainer = _form()
	widgets.label(panel, "双方独立选择合法卡组及基础阵法", 20)
	var names: Array[String] = []
	for raw: Variant in library["playable"] as Array:
		names.append(str((raw as Dictionary)["name"]))
	widgets.label(panel, "本方卡组")
	var first: OptionButton = widgets.option(panel, names)
	widgets.label(panel, "对方卡组")
	var second: OptionButton = widgets.option(panel, names)
	var difficulty: OptionButton = widgets.option(panel, ["简单AI", "普通AI", "困难AI"], 1)
	difficulty.visible = mode == "ai"
	widgets.label(panel, "随机种子（0–4294967295）")
	var seed: LineEdit = LineEdit.new()
	seed.text = "42"
	panel.add_child(seed)
	widgets.spacer(panel)
	widgets.button(panel, "开始对局", func() -> void:
		if not seed.text.is_valid_int() or int(seed.text) < 0 or int(seed.text) > 4294967295:
			notice = "随机种子超出范围"
			status_label.text = notice
			status_label.show()
			return
		var decks: Array = library["playable"] as Array
		start_match({"mode": mode, "seed": int(seed.text), "difficulty": difficulty.selected, "playerDeck": (decks[first.selected] as Dictionary)["id"], "opponentDeck": (decks[second.selected] as Dictionary)["id"]}))
	if mode == "ai":
		widgets.button(panel, "固定引导教学", func() -> void: start_match({"mode": "tutorial"}))

func start_match(configuration: Dictionary) -> bool:
	if not _result(bridge.start_match(configuration)):
		return false
	options = configuration.duplicate(true)
	mode = str(configuration.get("mode", "hotseat"))
	viewer = 0
	interaction.clear()
	inspected = {}
	hand_order.clear()
	_hand_orders.clear()
	covered = mode == "hotseat"
	notice = ""
	refresh_match()
	return true

func refresh_match() -> void:
	var response: Dictionary = bridge.snapshot(viewer)
	if not _result(response):
		return
	view = response["data"] as Dictionary
	if not interaction.is_empty() and str(interaction.get("revision", "")) != str(view["revision"]):
		interaction.clear()
	if page != "match" or not is_instance_valid(match_scene):
		_page("match", "奥术对决")
		if not is_instance_valid(match_scene):
			match_scene = MATCH_SCENE.instantiate() as WizardMatchView
			root_column.add_child(match_scene)
			match_scene.initialize(skin, audio)
			match_scene.selected.connect(_card_if_current)
			match_scene.inspected.connect(_inspect_if_current)
			match_scene.dropped.connect(_match_drop)
			match_scene.reordered.connect(_reorder_if_current)
			match_scene.requested.connect(_match_requested)
		else:
			match_scene.reparent(root_column)
		match_scene.size_flags_vertical = Control.SIZE_EXPAND_FILL
		match_scene.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		match_scene.show()
		match_scene.set_paused(false)
	status_label.text = notice
	status_label.visible = not notice.is_empty()
	if covered:
		inspected = {}
		interaction.clear()
	_sync_hand()
	match_scene.apply(view, interaction, hand_order, covered, bool(settings.get("reducedMotion", false)))
	if covered:
		_cue_batch.clear()
		return
	_clear(match_scene.details)
	_render_details(match_scene.details, inspected)
	if not inspected.is_empty():
		match_scene.reveal_details()
	_clear(match_scene.actions)
	var page_column: VBoxContainer = root_column
	root_column = match_scene.actions
	_render_actions()
	if int(view["result"]) >= 0:
		var end: HBoxContainer = widgets.row(root_column)
		widgets.label(end, "平局" if int(view["result"]) == 2 else "玩家%d获胜" % [int(view["result"]) + 1], 22)
		widgets.button(end, "再来一局", _restart)
		widgets.button(end, "返回标题", _leave)
	root_column = page_column
	if not _cue_batch.is_empty():
		match_scene.play_cues(_cue_batch)
		_cue_batch.clear()

func _match_requested(action: String, argument: Variant) -> void:
	if page != "match":
		return
	match action:
		"settings": show_settings()
		"save": save_replay()
		"leave_request": _request_leave()
		"uncover": uncover()
		"cancel": cancel_selection()
		"sort": _sort_hand(bool(argument))
		"link": _pick_link(str(argument))

func _inspect_if_current(card: Dictionary, generation: String, revision: String) -> void:
	if page != "match" or covered or generation != str(view.get("generation", "")) or revision != str(view.get("revision", "")):
		return
	inspected = card.duplicate(true)
	_clear(match_scene.details)
	_render_details(match_scene.details, inspected)
	match_scene.reveal_details()

func _render_actions() -> void:
	var row: HFlowContainer = HFlowContainer.new()
	root_column.add_child(row)
	var pending: Variant = interaction.get("pending")
	if pending != null:
		var description: Label = widgets.label(row, "待确认：" + str(pending), WizardSkin.BODY)
		description.custom_minimum_size.x = 230
		description.clip_text = true
		description.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		description.tooltip_text = description.text
		widgets.button(row, "确认提交", confirm_selection, "confirm", "PrimaryButton")
		widgets.button(row, "取消", cancel_selection, "cancel")
	elif int(interaction.get("step", 0)) in [1, 2, 4]:
		widgets.label(row, "选择额外成本" if int(interaction["step"]) == 2 else "选择高亮目标 / 链节", WizardSkin.BODY)
		widgets.button(row, "取消选择", cancel_selection)
	else:
		var groups: Array = interaction.get("groups", []) as Array
		for index: int in range(groups.size()):
			widgets.button(row, str(groups[index]), _activate.bind(index))
		var legal: Array = view["actions"] as Array
		for raw: Variant in legal:
			var action: Dictionary = raw as Dictionary
			var command: Dictionary = action["command"] as Dictionary
			if str(action["source"]) == "0" or int(command["type"]) == 11:
				widgets.button(row, str(action["label"]), _flat_action.bind(action))
		if view.get("decision") != null:
			widgets.button(row, "展开全部合法选择", _legal_choices)
	var tutorial: Dictionary = view.get("tutorial", {}) as Dictionary if view.get("tutorial") != null else {}
	if not tutorial.is_empty():
		widgets.label(row, str(tutorial["title"]), WizardSkin.HELPER).tooltip_text = str(tutorial["hint"])
		if bool(tutorial["canContinue"]):
			widgets.button(row, "教学：继续", _continue_tutorial)

func _render_details(parent: Node, data: Dictionary) -> void:
	widgets.label(parent, "卡牌详情 · 只读", WizardSkin.SECTION)
	if data.is_empty():
		widgets.text(parent, "点选卡牌查看完整效果、费用和实例状态。执行操作显示在下方操作栏。")
		return
	if bool(data.get("hidden", false)):
		widgets.text(parent, "对方埋伏卡\n反转前名称、类型和效果隐藏。")
		return
	var definition: Dictionary = data.get("definition", data) as Dictionary
	var scroll: ScrollContainer = widgets.scroll(parent)
	var column: VBoxContainer = widgets.column(scroll)
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	widgets.card(column, data, "detail", Vector2(220, 330))
	widgets.label(column, str(definition.get("name", "未知ID")), 20)
	for stat: Dictionary in WizardCardView.attribute_rows(definition):
		widgets.icon_label(column, str(stat["icon"]), str(stat["text"]), WizardSkin.BODY)
	var text: RichTextLabel = widgets.text(column, str(definition.get("text", "未知卡牌ID保留在草稿中，可移除修复。")) + "\n\n" + str(definition.get("flavor", "")), 120)
	text.fit_content = true
	text.scroll_active = false
	if data.has("host"):
		widgets.label(column, "宿主 #%s · 绑定荷载 %d" % [str(data["host"]), int(data.get("analysisLoad", 0)) + int(data.get("castLoad", 0))], WizardSkin.HELPER)
		if int(definition.get("type", 0)) == 1:
			widgets.label(column, "解析剩余%d回合 · 当前施法费用%d" % [int(data.get("turnsToReady", 0)), int(data.get("effectiveCastCost", 0))], WizardSkin.HELPER)
		if bool(data.get("base", false)):
			widgets.label(column, "基础阵法 · 受基础保护", WizardSkin.HELPER)

func _find_card(id: String) -> Dictionary:
	for raw: Variant in view.get("cards", []) as Array:
		var card: Dictionary = raw as Dictionary
		if str(card["id"]) == id:
			return card
	return {}

func _interact(request: Dictionary) -> void:
	var response: Dictionary = bridge.interact(viewer, request, str(view["generation"]), str(view["revision"]))
	if _result(response):
		interaction = response["data"] as Dictionary
		interaction["revision"] = view["revision"]
		refresh_match()

func _card_selected(card: Dictionary) -> void:
	audio.cue("card_select")
	inspected = card.duplicate(true)
	var id: String = str(card["id"])
	_interact({"operation": "pick" if (interaction.get("candidates", []) as Array).has(id) else "select", "card": id})

func _card_if_current(card: Dictionary, generation: String, revision: String) -> void:
	if page == "match" and not covered and generation == str(view.get("generation", "")) and revision == str(view.get("revision", "")):
		_card_selected(card)

func _reorder_if_current(source: String, before: String, generation: String, revision: String) -> void:
	if page == "match" and not covered and generation == str(view.get("generation", "")) and revision == str(view.get("revision", "")):
		_reorder_hand(source, before)

func _cancel_if_current(_card: Dictionary, generation: String, revision: String) -> void:
	if page == "match" and not covered and generation == str(view.get("generation", "")) and revision == str(view.get("revision", "")):
		cancel_selection()

func _activate(group: int) -> void:
	_interact({"operation": "activate", "group": group})

func _pick_link(id: String) -> void:
	if (interaction.get("links", []) as Array).has(id):
		_interact({"operation": "pick_link", "link": id})

func _flat_action(action: Dictionary) -> void:
	if _result(bridge.select_action(viewer, str(action["id"]), str(view["generation"]), str(view["revision"]))):
		interaction = {"pending": action["label"], "revision": view["revision"], "command": action["command"]}
		refresh_match()

func confirm_selection() -> void:
	if _result(bridge.confirm_action(str(view["generation"]), str(view["revision"]))):
		interaction.clear()
		_after_commit()

func cancel_selection() -> void:
	audio.cue("ui_cancel")
	bridge.cancel_action()
	interaction.clear()
	refresh_match()

func _after_commit() -> void:
	var response: Dictionary = bridge.snapshot(viewer)
	if not _result(response):
		return
	var next: Dictionary = response["data"] as Dictionary
	if mode == "hotseat" and int(next["actingPlayer"]) != viewer and int(next["result"]) < 0:
		audio.stop()
		covered = true
		inspected = {}
		_hand_orders[viewer] = hand_order.duplicate()
	refresh_match()

func uncover() -> void:
	viewer = int(view["actingPlayer"])
	hand_order.assign(_hand_orders.get(viewer, []) as Array)
	covered = false
	audio.cue("turn_ready")
	refresh_match()

func _match_drop(data: Dictionary, destination: int, target: String) -> void:
	if page != "match" or covered or str(data.get("context", "")) != "match":
		return
	if str(data.get("generation", "")) != str(view["generation"]) or str(data.get("revision", "")) != str(view["revision"]):
		return
	var source: String = str(data["id"])
	if destination == 1:
		_reorder_hand(source, "")
		return
	inspected = _find_card(source)
	_interact({"operation": "drop", "card": source, "target": target, "destination": destination})

func _reorder_hand(source: String, before: String) -> void:
	if not hand_order.has(source) or source == before:
		return
	hand_order.erase(source)
	var index: int = hand_order.find(before)
	if index >= 0:
		hand_order.insert(index, source)
	else:
		hand_order.append(source)
	_hand_orders[viewer] = hand_order.duplicate()
	audio.cue("card_place")
	refresh_match()

func _sync_hand() -> void:
	var available: Array[String] = []
	for raw: Variant in view["cards"] as Array:
		var card: Dictionary = raw as Dictionary
		if int(card["zone"]) == 1 and int(card["owner"]) == viewer:
			available.append(str(card["id"]))
	for id: String in hand_order.duplicate():
		if not available.has(id):
			hand_order.erase(id)
	for id: String in available:
		if not hand_order.has(id):
			hand_order.append(id)

func _sort_hand(by_cost: bool) -> void:
	hand_order.sort_custom(func(a: String, b: String) -> bool:
		var first: Dictionary = _find_card(a).get("definition", {}) as Dictionary
		var second: Dictionary = _find_card(b).get("definition", {}) as Dictionary
		return int(first.get("cost" if by_cost else "type", 0)) < int(second.get("cost" if by_cost else "type", 0)))
	refresh_match()

func _tick() -> void:
	if is_instance_valid(match_scene) and match_scene.board.busy():
		return
	if page != "match" or covered or is_instance_valid(_dialog) or is_instance_valid(_choices_dialog) or view.is_empty() or not interaction.is_empty() or get_viewport().gui_is_dragging():
		return
	if int(view["result"]) >= 0 or bool(view["paused"]):
		return
	if bool(view["aiTurn"]):
		if _result(bridge.step_ai(str(view["generation"]), str(view["revision"]))):
			_after_commit()
	elif view.get("automaticAction") != null:
		if _result(bridge.select_action(viewer, str(view["automaticAction"]), str(view["generation"]), str(view["revision"]))):
			if _result(bridge.confirm_action(str(view["generation"]), str(view["revision"]))):
				_after_commit()

func _continue_tutorial() -> void:
	if _result(bridge.continue_tutorial(str(view["generation"]), str(view["revision"]))):
		interaction.clear()
		refresh_match()

func save_replay() -> void:
	var result: Dictionary = bridge.save_replay()
	if _result(result):
		notice = "复盘已保存：" + str((result["data"] as Dictionary)["path"])
		status_label.text = notice
		status_label.show()

func _restart() -> void:
	audio.stop()
	options["seed"] = int(options.get("seed", 42)) + 1
	if _result(bridge.restart_match(int(options["seed"]))):
		covered = mode == "hotseat"
		viewer = 0
		interaction.clear()
		inspected = {}
		hand_order.clear()
		_hand_orders.clear()
		refresh_match()

func _legal_choices() -> void:
	var dialog: AcceptDialog = AcceptDialog.new()
	_choices_dialog = dialog
	dialog.title = "当前合法选择 · 选择后仍需最终确认"
	dialog.theme = skin.theme
	var scroll: ScrollContainer = widgets.scroll(dialog)
	scroll.custom_minimum_size = Vector2(560, 350)
	var column: VBoxContainer = widgets.column(scroll)
	for raw: Variant in view["actions"] as Array:
		var action: Dictionary = raw as Dictionary
		widgets.button(column, str(action["label"]), func() -> void:
			dialog.queue_free()
			_flat_action(action))
	add_child(dialog)
	dialog.close_requested.connect(dialog.queue_free)
	dialog.confirmed.connect(dialog.queue_free)
	dialog.popup_centered()

func _confirmation(message: String, callback: Callable) -> void:
	_dialog = ConfirmationDialog.new()
	_dialog.title = "请确认"
	_dialog.dialog_text = message
	_dialog.ok_button_text = "确认"
	_dialog.cancel_button_text = "取消"
	_dialog.theme = skin.theme
	add_child(_dialog)
	_dialog.confirmed.connect(func() -> void:
		_dialog.queue_free()
		callback.call())
	_dialog.canceled.connect(func() -> void: _dialog.queue_free())
	_dialog.popup_centered()

func _request_leave() -> void:
	_confirmation("离开将结束当前对局，并保存复盘。", _leave)

func _leave() -> void:
	if _result(bridge.leave_match(viewer)):
		audio.stop()
		view.clear()
		interaction.clear()
		inspected = {}
		show_menu()

func _request_quit() -> void:
	if page == "editor" and dirty:
		_discard_prompt(_quit)
	elif not view.is_empty():
		_confirmation("结束对局、保存复盘并退出？", func() -> void:
			if _result(bridge.leave_match(viewer)):
				_quit())
	else:
		_quit()

func _quit() -> void:
	get_tree().quit()

func _notification(what: int) -> void:
	if what in [NOTIFICATION_WM_WINDOW_FOCUS_OUT, NOTIFICATION_WM_MOUSE_EXIT] and is_instance_valid(match_scene):
		match_scene.cancel_drag()
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		_request_quit()

func _unhandled_key_input(event: InputEvent) -> void:
	if event is InputEventKey and (event as InputEventKey).pressed and (event as InputEventKey).keycode == KEY_ESCAPE:
		if get_viewport().gui_is_dragging():
			get_viewport().gui_cancel_drag()
		elif page == "match":
			cancel_selection()
		elif page == "editor":
			_editor_back()

func _exit_tree() -> void:
	bridge.release_session()

func show_settings() -> void:
	var from_match: bool = not view.is_empty()
	if from_match:
		audio.stop()
		bridge.set_paused(true)
	_page("settings", "游戏设置")
	var column: VBoxContainer = _form()
	var values: Dictionary = {}
	for item: String in ["masterVolume", "soundVolume"]:
		widgets.label(column, "总音量" if item == "masterVolume" else "音效音量")
		var volume: HSlider = HSlider.new()
		volume.max_value = 100
		volume.value = int(settings[item])
		column.add_child(volume)
		values[item] = volume
	widgets.label(column, "窗口模式")
	var window: OptionButton = widgets.option(column, ["窗口", "无边框", "全屏"], int(settings["windowMode"]))
	widgets.label(column, "窗口尺寸")
	var sizes: OptionButton = widgets.option(column, ["1280×720", "1600×1000", "1920×1080"])
	for index: int in range(3):
		if int(settings["width"]) == [1280, 1600, 1920][index] and int(settings["height"]) == [720, 1000, 1080][index]:
			sizes.select(index)
	var automatic: CheckBox = CheckBox.new()
	automatic.text = "自动通过没有操作的阶段"
	automatic.button_pressed = bool(settings["automaticPhases"])
	column.add_child(automatic)
	var reduced: CheckBox = CheckBox.new()
	reduced.text = "精简动态表现"
	reduced.button_pressed = bool(settings["reducedMotion"])
	column.add_child(reduced)
	widgets.label(column, "保存成功后应用；对局暂停保留当前决策与未确认选择。", WizardSkin.BODY)
	widgets.spacer(column)
	var buttons: HBoxContainer = widgets.row(column)
	widgets.button(buttons, "应用并保存", func() -> void:
		var updated: Dictionary = settings.duplicate(true)
		updated["masterVolume"] = int((values["masterVolume"] as HSlider).value)
		updated["soundVolume"] = int((values["soundVolume"] as HSlider).value)
		updated["windowMode"] = window.selected
		updated["width"] = [1280, 1600, 1920][sizes.selected]
		updated["height"] = [720, 1000, 1080][sizes.selected]
		updated["automaticPhases"] = automatic.button_pressed
		updated["reducedMotion"] = reduced.button_pressed
		var response: Dictionary = bridge.apply_settings(updated)
		if _result(response):
			settings = response["data"] as Dictionary
			audio.apply_settings(settings)
			_apply_window()
			_settings_back(from_match))
	widgets.button(buttons, "放弃修改并返回", _settings_back.bind(from_match))

func _settings_back(from_match: bool) -> void:
	if from_match:
		bridge.set_paused(false)
		refresh_match()
	else:
		show_menu()

func _apply_window() -> void:
	var window_mode: int = int(settings["windowMode"])
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN if window_mode == 2 else DisplayServer.WINDOW_MODE_WINDOWED)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, window_mode == 1)
	if window_mode == 0:
		DisplayServer.window_set_size(Vector2i(int(settings["width"]), int(settings["height"])))

func show_decks() -> void:
	_reload_library()
	_page("decks", "卡组列表")
	var toolbar: HBoxContainer = widgets.row(root_column)
	widgets.button(toolbar, "新建草稿", _new_draft.bind(""))
	widgets.label(toolbar, "预设可复制；不合法草稿可保存修复。", WizardSkin.BODY)
	var list: VBoxContainer = widgets.column(widgets.scroll(root_column))
	list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for raw: Variant in library["drafts"] as Array:
		var saved: Dictionary = raw as Dictionary
		var row: HBoxContainer = widgets.row(widgets.panel(list))
		widgets.label(row, str(saved["name"]) + (" · 合法" if (saved["problems"] as Array).is_empty() else " · 待修复"))
		widgets.spacer(row)
		widgets.button(row, "编辑", edit_draft.bind(saved))
		widgets.button(row, "删除", func() -> void:
			_confirmation("删除卡组「%s」？" % str(saved["name"]), func() -> void:
				if _result(bridge.erase_draft(str(saved["id"]))):
					show_decks()))
	widgets.label(list, "发布预设", 20)
	for raw: Variant in library["presets"] as Array:
		var preset: Dictionary = raw as Dictionary
		var row: HBoxContainer = widgets.row(widgets.panel(list))
		widgets.label(row, str(preset["name"])).tooltip_text = str(preset["description"])
		widgets.spacer(row)
		widgets.button(row, "复制并编辑", _new_draft.bind(str(preset["id"])))

func _new_draft(preset: String) -> void:
	var response: Dictionary = bridge.create_draft(preset)
	if _result(response):
		edit_draft(response["data"] as Dictionary)

func edit_draft(data: Dictionary) -> void:
	draft = {"id": data["id"], "name": data["name"], "baseFormation": data["baseFormation"], "cards": (data["cards"] as Dictionary).duplicate(true)}
	dirty = false
	query.clear()
	pool_page = 0
	inspected = (definitions.get("fireball", {}) as Dictionary).duplicate(true)
	_page("editor", "卡组构筑 · 草稿")
	var toolbar: HBoxContainer = widgets.row(root_column)
	widgets.label(toolbar, "卡组名称", WizardSkin.BODY)
	var name_input: LineEdit = LineEdit.new()
	name_input.text = str(draft["name"])
	name_input.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	toolbar.add_child(name_input)
	name_input.text_changed.connect(func(value: String) -> void:
		draft["name"] = value
		dirty = true
		_validate_editor())
	widgets.label(toolbar, "基础阵法", WizardSkin.BODY)
	var bases: Array[String] = []
	var base_names: Array[String] = []
	for id: String in definitions:
		if bool((definitions[id] as Dictionary).get("baseEligible", false)):
			bases.append(id)
			base_names.append(str((definitions[id] as Dictionary)["name"]))
	if not bases.has(str(draft["baseFormation"])):
		bases.append(str(draft["baseFormation"]))
		base_names.append("未知ID：" + str(draft["baseFormation"]))
	var base: OptionButton = widgets.option(toolbar, base_names, bases.find(str(draft["baseFormation"])))
	base.item_selected.connect(func(index: int) -> void:
		draft["baseFormation"] = bases[index]
		dirty = true
		_validate_editor())
	widgets.button(toolbar, "保存草稿", _save_draft, "save")
	widgets.button(toolbar, "返回列表", _editor_back)
	var body: HBoxContainer = widgets.row(root_column, true)
	editor_details = widgets.column(widgets.panel(body), true)
	editor_details.custom_minimum_size.x = 230
	_render_details(editor_details, inspected)
	var deck_zone: WizardZoneView = WizardZoneView.new()
	deck_zone.add_theme_stylebox_override("panel", skin.style("tooltip"))
	deck_zone.custom_minimum_size.x = 485
	deck_zone.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	deck_zone.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.add_child(deck_zone)
	deck_zone.dropped.connect(func(payload: Dictionary, _destination: int, _target: String) -> void:
		if str(payload.get("context", "")) == "pool":
			_adjust_draft(str(payload["id"]), 1))
	var deck_column: VBoxContainer = widgets.column(deck_zone)
	widgets.label(deck_column, "主卡组 · 右键移除", WizardSkin.SECTION).add_theme_color_override("font_color", skin.color("text"))
	editor_problems = widgets.label(deck_column, "", WizardSkin.HELPER)
	editor_problems.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	editor_deck = GridContainer.new()
	editor_deck.columns = 5
	editor_deck.add_theme_constant_override("h_separation", 4)
	editor_deck.add_theme_constant_override("v_separation", 4)
	widgets.scroll(deck_column).add_child(editor_deck)
	editor_deck.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var pool_zone: WizardZoneView = WizardZoneView.new()
	pool_zone.add_theme_stylebox_override("panel", skin.style("tooltip"))
	pool_zone.custom_minimum_size.x = 430
	pool_zone.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	pool_zone.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.add_child(pool_zone)
	pool_zone.dropped.connect(func(payload: Dictionary, _destination: int, _target: String) -> void:
		if str(payload.get("context", "")) == "draft":
			_adjust_draft(str(payload["id"]), -1))
	var pool_column: VBoxContainer = widgets.column(pool_zone)
	widgets.label(pool_column, "卡池 · 右键加入 / 拖入卡组", WizardSkin.SECTION).add_theme_color_override("font_color", skin.color("text"))
	var search: LineEdit = LineEdit.new()
	search.placeholder_text = "搜索卡名"
	pool_column.add_child(search)
	search.text_changed.connect(func(value: String) -> void:
		query["name"] = value
		pool_page = 0
		_refresh_pool())
	var filters: HBoxContainer = widgets.row(pool_column)
	var types: OptionButton = widgets.option(filters, ["全部类型", "行动", "解析", "言灵", "阵法", "符文"])
	types.item_selected.connect(func(index: int) -> void:
		query.erase("type")
		if index > 0:
			query["type"] = index - 1
		pool_page = 0
		_refresh_pool())
	var rarity: OptionButton = widgets.option(filters, ["全部稀有度", "普通", "罕见", "稀有", "史诗", "传说"])
	rarity.item_selected.connect(func(index: int) -> void:
		query["rarity"] = ["", "common", "uncommon", "rare", "epic", "legendary"][index]
		pool_page = 0
		_refresh_pool())
	var costs: HBoxContainer = widgets.row(pool_column)
	_numeric_filter(costs, "cost", "费用")
	_numeric_filter(costs, "castCost", "施法")
	_numeric_filter(costs, "rank", "位阶")
	var tags: LineEdit = LineEdit.new()
	tags.placeholder_text = "标签（逗号分隔，组合筛选）"
	pool_column.add_child(tags)
	tags.text_changed.connect(func(value: String) -> void:
		var selected_tags: Array[String] = []
		for part: String in value.replace("，", ",").split(",", false):
			if not part.strip_edges().is_empty():
				selected_tags.append(part.strip_edges())
		query["tags"] = selected_tags
		pool_page = 0
		_refresh_pool())
	var pager: HBoxContainer = widgets.row(pool_column)
	widgets.button(pager, "上一页", _pool_turn.bind(-1))
	widgets.button(pager, "下一页", _pool_turn.bind(1))
	editor_pool = GridContainer.new()
	editor_pool.columns = 3
	widgets.scroll(pool_column).add_child(editor_pool)
	_refresh_draft()
	_refresh_pool()

func _numeric_filter(parent: Node, key: String, title: String) -> void:
	var amounts: Array[int] = []
	for id: String in definitions:
		var definition: Dictionary = definitions[id] as Dictionary
		var amount: int = int(definition.get(key, 0))
		if not amounts.has(amount):
			amounts.append(amount)
	amounts.sort()
	var names: Array[String] = [title + "：全部"]
	for amount: int in amounts:
		names.append(title + " " + str(amount))
	var node: OptionButton = widgets.option(parent, names)
	node.item_selected.connect(func(index: int) -> void:
		query.erase(key)
		if index > 0:
			query[key] = amounts[index - 1]
		pool_page = 0
		_refresh_pool())

func _refresh_draft() -> void:
	_clear(editor_deck)
	for id: String in draft["cards"] as Dictionary:
		var definition: Dictionary = definitions.get(id, {"id": id, "name": "未知ID：" + id, "type": 0}) as Dictionary
		for copy: int in range(int((draft["cards"] as Dictionary)[id])):
			var node: WizardCardView = widgets.card(editor_deck, definition, "hand", Vector2(90, 148))
			node.drag_enabled = true
			node.drag_context = "draft"
			node.selected.connect(_editor_select)
			node.secondary.connect(func(_card: Dictionary) -> void: _adjust_draft(id, -1))
	_validate_editor()

func _validate_editor() -> void:
	var response: Dictionary = bridge.validate_draft(draft)
	if not _result(response):
		return
	var problems: Array = (response["data"] as Dictionary)["problems"] as Array
	var count: int = 0
	for amount: Variant in (draft["cards"] as Dictionary).values():
		count += int(amount)
	editor_problems.text = "%d / 30 · %s" % [count, "合法" if problems.is_empty() else "\n".join(PackedStringArray(problems))]
	if dirty:
		status_label.text = "草稿有修改；尚未保存。"
		status_label.show()

func _refresh_pool() -> void:
	_clear(editor_pool)
	var response: Dictionary = bridge.query_cards(query)
	if not _result(response):
		return
	var ids: Array = (response["data"] as Dictionary)["ids"] as Array
	pool_page = clampi(pool_page, 0, maxi(0, (ids.size() - 1) / 9))
	for index: int in range(pool_page * 9, mini(ids.size(), pool_page * 9 + 9)):
		var id: String = str(ids[index])
		var node: WizardCardView = widgets.card(editor_pool, definitions[id] as Dictionary, "hand", Vector2(118, 164))
		node.drag_enabled = true
		node.drag_context = "pool"
		node.selected.connect(_editor_select)
		node.secondary.connect(func(_card: Dictionary) -> void: _adjust_draft(id, 1))
	status_label.text = "卡池第%d/%d页 · 未保存草稿%s" % [pool_page + 1, maxi(1, ceili(ids.size() / 9.0)), "有修改" if dirty else "无修改"]
	status_label.show()

func _editor_select(card: Dictionary) -> void:
	audio.cue("card_select")
	inspected = card.duplicate(true)
	_clear(editor_details)
	_render_details(editor_details, inspected)

func _adjust_draft(id: String, amount: int) -> void:
	var cards: Dictionary = draft["cards"] as Dictionary
	var next: int = int(cards.get(id, 0)) + amount
	if next > 3 and amount > 0:
		status_label.text = "同名最多3张；已有超量或未知ID可移除修复。"
		status_label.show()
		return
	if next <= 0:
		cards.erase(id)
	else:
		cards[id] = next
	dirty = true
	_refresh_draft()

func _pool_turn(direction: int) -> void:
	pool_page += direction
	_refresh_pool()

func _save_draft() -> bool:
	var response: Dictionary = bridge.save_draft(draft)
	if not _result(response):
		return false
	dirty = false
	status_label.text = "草稿已保存；合法卡组将出现在开局选项中。"
	status_label.show()
	return true

func _editor_back() -> void:
	if dirty:
		_discard_prompt(show_decks)
	else:
		show_decks()

func _discard_prompt(callback: Callable) -> void:
	_dialog = ConfirmationDialog.new()
	_dialog.title = "草稿尚未保存"
	_dialog.dialog_text = "保存后离开，或放弃这次修改？"
	_dialog.ok_button_text = "保存并离开"
	_dialog.cancel_button_text = "继续编辑"
	_dialog.dialog_hide_on_ok = false
	_dialog.theme = skin.theme
	_dialog.add_button("放弃修改", true, "discard")
	add_child(_dialog)
	_dialog.confirmed.connect(func() -> void:
		if _save_draft():
			_dialog.queue_free()
			callback.call()
		else:
			_dialog.dialog_text = "保存失败，草稿保留。\n" + notice)
	_dialog.custom_action.connect(func(action: StringName) -> void:
		if action == &"discard":
			_dialog.queue_free()
			callback.call())
	_dialog.canceled.connect(func() -> void: _dialog.queue_free())
	_dialog.popup_centered()
