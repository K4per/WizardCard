extends Control

var _bridge: WizardBridge = WizardBridge.new()
var _viewer: int = 0
var _view: Dictionary = {}
var _actions: VBoxContainer = VBoxContainer.new()
var _details: RichTextLabel = RichTextLabel.new()
var _status: Label = Label.new()
var _preview: Label = Label.new()
var _confirm: Button = Button.new()
var _continue: Button = Button.new()
var _pause: Button = Button.new()
var _mode: OptionButton = OptionButton.new()
var _covered: bool = false
var _assets_path: String = ""
var _user_directory: String = ""
var _save_notice: String = ""
var _pending_label: String = ""
var _pending_generation: String = ""
var _pending_revision: String = ""

func _ready() -> void:
	var font_path: String = ProjectSettings.globalize_path("res://../assets/fonts/NotoSansCJKsc-Regular.otf")
	var font: FontFile = FontFile.new()
	if font.load_dynamic_font(font_path) == OK:
		add_theme_font_override("font", font)
	var margin: MarginContainer = MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side: String in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 20)
	add_child(margin)
	var column: VBoxContainer = VBoxContainer.new()
	margin.add_child(column)
	var heading: Label = Label.new()
	heading.text = "WizardCard · C++ / Godot 桥接实验台"
	heading.add_theme_font_size_override("font_size", 24)
	column.add_child(heading)
	var note: Label = Label.new()
	note.text = "技术验证页面 · 正式视觉页面待设计确认。选择操作后点击确认才会提交。"
	column.add_child(note)
	var toolbar: HBoxContainer = HBoxContainer.new()
	column.add_child(toolbar)
	for name: String in ["本地换手", "普通AI", "固定教学"]:
		_mode.add_item(name)
	toolbar.add_child(_mode)
	_button(toolbar, "创建对局", _start)
	_pause.text = "暂停 / 恢复"
	_pause.pressed.connect(_toggle_pause)
	toolbar.add_child(_pause)
	_button(toolbar, "保存复盘", _save)
	_button(toolbar, "结束并释放", _release)
	_button(toolbar, "确认接手", _uncover)
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(_status)
	var body: HSplitContainer = HSplitContainer.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(body)
	_details.custom_minimum_size.x = 560
	_details.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_child(_details)
	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.custom_minimum_size.x = 460
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_child(scroll)
	_actions.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(_actions)
	_preview.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(_preview)
	var controls: HBoxContainer = HBoxContainer.new()
	column.add_child(controls)
	_confirm.text = "确认提交"
	_confirm.disabled = true
	_confirm.pressed.connect(_submit)
	controls.add_child(_confirm)
	_button(controls, "取消选择", _cancel)
	_continue.text = "教学：继续"
	_continue.pressed.connect(_continue_lesson)
	controls.add_child(_continue)
	_button(controls, "AI推进一步", _ai_step)
	var assets: String = ProjectSettings.globalize_path("res://../assets")
	var user_dir: String = ""
	var args: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(0, args.size() - 1, 2):
		if args[index] == "--assets":
			assets = args[index + 1]
		elif args[index] == "--user-data":
			user_dir = args[index + 1]
	_show_result(_bridge.initialize(assets, user_dir))
	_assets_path = assets
	_user_directory = user_dir
	if not user_dir.is_empty():
		_status.text += "\n验证数据目录：" + user_dir

func _button(parent: Control, text: String, callback: Callable) -> void:
	var button: Button = Button.new()
	button.text = text
	button.pressed.connect(callback)
	parent.add_child(button)

func _show_result(response: Dictionary) -> bool:
	if not bool(response.get("ok", false)):
		_status.text = str(response.get("error", "桥接失败"))
		return false
	_status.text = "操作成功"
	var data: Dictionary = response.get("data", {}) as Dictionary
	if data.has("replaySaved") and not bool(data["replaySaved"]):
		_save_notice = "操作已接受；复盘未保存，请重试保存：" + str(data.get("saveError", ""))
		_status.text = _save_notice
	return true

func _start() -> void:
	var mode: String = ["hotseat", "ai", "tutorial"][_mode.selected]
	if _show_result(_bridge.start_match({"mode": mode, "seed": 42, "difficulty": 1})):
		_save_notice = ""
		_pending_label = ""
		_mode.disabled = true
		_viewer = 0
		_covered = _mode.selected == 0
		_refresh()

func _refresh() -> void:
	for child: Node in _actions.get_children():
		_actions.remove_child(child)
		child.queue_free()
	_confirm.disabled = true
	_preview.text = ""
	var response: Dictionary = _bridge.snapshot(_viewer)
	if not _show_result(response):
		_details.text = "没有进行中的对局"
		return
	_view = response["data"] as Dictionary
	if _covered:
		_details.text = "换手遮挡 · 请当前操作者点击「确认接手」"
		_status.text = "等待接手"
		_continue.disabled = true
		return
	var players: Array = _view["players"] as Array
	var text: String = "%s · 操作者 %d · generation %s / revision %s\n\n" % [str(_view["phaseName"]), int(_view["actingPlayer"]) + 1, str(_view["generation"]), str(_view["revision"])]
	for player: int in range(2):
		var stats: Dictionary = players[player] as Dictionary
		text += "玩家 %d：生命 %d · 魔素 %d · 荷载 %d/%d · 手牌 %d · 牌库 %d\n" % [player + 1, stats["life"], stats["mana"], stats["load"], stats["capacity"], stats["handCount"], stats["deckCount"]]
	text += "\n可见卡牌\n"
	for raw: Variant in _view["cards"] as Array:
		var card: Dictionary = raw as Dictionary
		var definition: Dictionary = card.get("definition", {}) as Dictionary
		text += "#%s · %s · %s\n" % [str(card["id"]), str(definition.get("name", "埋伏卡")), str(card["zoneName"])]
	text += "\n公开 / 本方日志\n"
	var events: Array = _view["events"] as Array
	for index: int in range(maxi(0, events.size() - 10), events.size()):
		var event: Dictionary = events[index] as Dictionary
		text += str(event["text"]) + "\n"
	_details.text = text
	for raw: Variant in _view["actions"] as Array:
		var action: Dictionary = raw as Dictionary
		_button(_actions, str(action["label"]), _select.bind(str(action["id"])))
	var tutorial: Dictionary = _view.get("tutorial", {}) as Dictionary if _view.get("tutorial") != null else {}
	_continue.disabled = not bool(tutorial.get("canContinue", false))
	if not tutorial.is_empty():
		_status.text = str(tutorial["title"]) + " · " + str(tutorial["hint"])
	elif bool(_view["paused"]):
		_status.text = "已暂停；恢复后保留当前决策"
	elif int(_view["result"]) >= 0:
		_status.text = "对局结束 · result " + str(_view["result"])
	if not _save_notice.is_empty():
		_status.text = _save_notice
	if _pending_generation == str(_view["generation"]) and _pending_revision == str(_view["revision"]):
		_preview.text = _pending_label
		_confirm.disabled = bool(_view["paused"]) or _pending_label.is_empty()
	else:
		_pending_label = ""

func _select(id: String) -> void:
	var response: Dictionary = _bridge.select_action(_viewer, id, str(_view["generation"]), str(_view["revision"]))
	if _show_result(response):
		var data: Dictionary = response["data"] as Dictionary
		_pending_label = "待确认：" + str(data["label"])
		_pending_generation = str(_view["generation"])
		_pending_revision = str(_view["revision"])
		_preview.text = _pending_label
		_confirm.disabled = false

func _submit() -> void:
	if _show_result(_bridge.confirm_action(str(_view["generation"]), str(_view["revision"]))):
		_after_submission()

func _after_submission() -> void:
	_pending_label = ""
	var v: Dictionary = (_bridge.snapshot(_viewer)["data"] as Dictionary)
	if _mode.selected == 0 and int(v["actingPlayer"]) != _viewer and int(v["result"]) == -1:
		_covered = true
	_refresh()

func _uncover() -> void:
	if not _covered:
		return
	var v: Dictionary = (_bridge.snapshot(_viewer)["data"] as Dictionary)
	_viewer = int(v["actingPlayer"])
	_covered = false
	_refresh()

func _cancel() -> void:
	_bridge.cancel_action()
	_pending_label = ""
	_refresh()

func _toggle_pause() -> void:
	if _view.is_empty():
		return
	_bridge.set_paused(not bool(_view.get("paused", false)))
	_refresh()

func _continue_lesson() -> void:
	if _show_result(_bridge.continue_tutorial(str(_view["generation"]), str(_view["revision"]))):
		_refresh()

func _ai_step() -> void:
	if _view.is_empty():
		return
	if _show_result(_bridge.step_ai(str(_view["generation"]), str(_view["revision"]))):
		_refresh()

func _save() -> void:
	var response: Dictionary = _bridge.save_replay()
	if _show_result(response):
		var data: Dictionary = response["data"] as Dictionary
		_save_notice = ""
		_status.text = "复盘已保存：" + str(data["path"])

func _release() -> void:
	_bridge.release_session()
	_bridge.initialize(_assets_path, _user_directory)
	_view.clear()
	_save_notice = ""
	_pending_label = ""
	_covered = false
	_mode.disabled = false
	_refresh()

func _exit_tree() -> void:
	_bridge.release_session()
