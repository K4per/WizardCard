class_name ReplayPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var _files: OptionButton
var _notice: Label
var _paths: Array[String] = []
var _canvas: FieldCanvas
var _previous: Button
var _next: Button
var _viewer: OptionButton
var _position: Label
var _command: RichTextLabel
var _detail: CardDetail
var _rows: Array[Dictionary] = []
var _step: int = 0
var _path: String = ""
var _frame_cache: Dictionary = {}
var _frame: Dictionary = {}
var _status_own: PlayerStatusBar
var _status_opponent: PlayerStatusBar
var _logs_toggle: Button
var _logs_panel: PanelContainer
var _logs_text: RichTextLabel

func _ready() -> void:
	_files = %Files
	_notice = %Notice
	_previous = %Previous
	_next = %Next
	_viewer = %Viewer
	_position = %Position
	_command = %Command
	_canvas = %Canvas
	_detail = %Detail
	(%Back as Button).pressed.connect(_return)
	(%Load as Button).pressed.connect(_load)
	_previous.pressed.connect(_move.bind(-1))
	_next.pressed.connect(_move.bind(1))
	_viewer.add_item("玩家一视角")
	_viewer.add_item("玩家二视角")
	_viewer.item_selected.connect(_viewer_changed)
	_canvas.card_selected.connect(_show_detail)
	_canvas.selection_cleared.connect(_hide_detail)
	_canvas.hand_toggle_requested.connect(_toggle_hand)
	_detail.closed.connect(_hide_detail)
	_status_own = %OwnStatus
	_status_opponent = %OpponentStatus
	_logs_toggle = %LogsToggle
	_logs_toggle.pressed.connect(_toggle_logs)
	_build_logs()
	resized.connect(_layout_panels)
	get_viewport().size_changed.connect(_layout_panels)
	_layout_panels()
	_update_controls()

func _layout_panels() -> void:
	if _status_own == null or _status_opponent == null:
		return
	var available: Vector2 = size if size.x > 0.0 and size.y > 0.0 else get_viewport_rect().size
	PlayerStatusBar.place_corners(_status_own, _status_opponent, available)
	if _logs_panel != null:
		_logs_panel.size = Vector2(minf(400.0, available.x - 24.0), minf(296.0, available.y - 240.0))
		_logs_panel.position = Vector2(12.0, maxf(144.0, _status_own.position.y - _logs_panel.size.y - 12.0))

func configure(value: FrontendService) -> void:
	service = value
	_paths.clear()
	_files.clear()
	var directory: String = service.user_directory.path_join("replays")
	var access: DirAccess = DirAccess.open(directory)
	if access != null:
		var files: PackedStringArray = access.get_files()
		files.sort()
		files.reverse()
		for name: String in files:
			if name.ends_with(".json"):
				_paths.append(directory.path_join(name))
				_files.add_item(name)
	if not service.last_replay_path.is_empty():
		var index: int = _paths.find(service.last_replay_path)
		if index >= 0:
			_files.select(index)
	if _paths.is_empty():
		_notice.text = "暂无已保存的复盘。"
	else:
		_load()

func _load() -> void:
	if _files.selected < 0 or _files.selected >= _paths.size():
		return
	_path = ""
	_rows.clear()
	_frame_cache.clear()
	_frame = {}
	_canvas.configure(FieldPresentation.new())
	_step = 0
	_hide_detail()
	var path: String = _paths[_files.selected]
	# read_replay_plan validates every digest and builds the seek checkpoints.
	var plan: Dictionary = service.match_request(&"read_replay_plan", [path])
	if not bool(plan.get("ok", false)):
		_notice.text = "复盘读取失败：" + service.error
		_update_controls()
		return
	_path = path
	_rows.assign((plan.get("data", {}) as Dictionary).get("commands", []))
	_notice.text = "验证通过 · %d 步 · 逐步浏览完整对局" % _rows.size()
	_step = 0
	_show_frame()

func _move(delta: int) -> void:
	_step = clampi(_step + delta, 0, _rows.size())
	_show_frame()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and (event as InputEventKey).pressed and not (event as InputEventKey).echo:
		if (event as InputEventKey).keycode == KEY_ESCAPE and _logs_panel.visible:
			_toggle_logs()
			get_viewport().set_input_as_handled()

func _viewer_changed(_index: int) -> void:
	_show_frame()

func _show_frame() -> void:
	if _path.is_empty():
		return
	var cache_key: String = "%d:%d" % [_step, _viewer.selected]
	var frame: Dictionary
	if _frame_cache.has(cache_key):
		frame = _frame_cache[cache_key]
	else:
		var response: Dictionary = service.match_request(&"replay_frame", [_path, _step, _viewer.selected])
		if not bool(response.get("ok", false)):
			_notice.text = "无法显示此步骤：" + service.error
			return
		frame = (response.get("data", {}) as Dictionary).duplicate(true)
		if _frame_cache.size() >= 24:
			_frame_cache.erase(_frame_cache.keys()[0])
		_frame_cache[cache_key] = frame
	_frame = frame
	_canvas.configure(MatchProjection.field(frame))
	_canvas.set_background_theme(str(service.settings.get("fieldBackground", "arcane")))
	_update_status(frame)
	_update_logs(frame)
	_hide_detail()
	_command.text = "对局开始" if _step == 0 else "步骤 %d · 玩家 %d\n%s" % [_step, int(_rows[_step - 1].get("actor", 0)) + 1, _command_label(_rows[_step - 1].get("command", {}))]
	_update_controls()

func _update_status(frame: Dictionary) -> void:
	var players: Array = frame.get("players", [])
	if players.size() != 2:
		return
	var viewer: int = _viewer.selected
	var loads: Array = frame.get("loads", [])
	var cards: Array = frame.get("cards", [])
	var bars: Array[PlayerStatusBar] = [_status_own, _status_opponent]
	for index: int in range(2):
		var player: int = viewer if index == 0 else 1 - viewer
		var values: Dictionary = (players[player] as Dictionary).duplicate()
		bars[index].set_status("我方" if index == 0 else "对手", player, values, loads, cards, frame.get("debug", {}))

func _build_logs() -> void:
	_logs_panel = PanelContainer.new()
	_logs_panel.visible = false
	_logs_panel.custom_minimum_size = Vector2(320, 220)
	_logs_panel.z_index = 5
	add_child(_logs_panel)
	var column: VBoxContainer = VBoxContainer.new()
	_logs_panel.add_child(column)
	var header: HBoxContainer = HBoxContainer.new()
	column.add_child(header)
	var title: Label = UiFactory.label("对局日志", 16, true)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	var close: Button = UiFactory.button("×", _toggle_logs)
	close.custom_minimum_size = Vector2(36, 36)
	header.add_child(close)
	_logs_text = RichTextLabel.new()
	_logs_text.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_logs_text.bbcode_enabled = false
	_logs_text.fit_content = false
	_logs_text.custom_minimum_size = Vector2(300, 190)
	_logs_text.add_theme_font_size_override(&"normal_font_size", 14)
	column.add_child(_logs_text)

func _toggle_logs() -> void:
	_logs_panel.visible = not _logs_panel.visible
	if _logs_panel.visible:
		_logs_panel.move_to_front()
		_logs_toggle.grab_focus()

func _update_logs(frame: Dictionary) -> void:
	_logs_text.clear()
	for event: Dictionary in frame.get("events", []):
		var text: String = str(event.get("text", "对局状态已更新"))
		_logs_text.append_text(text + "\n")

func _command_label(command: Dictionary) -> String:
	var kinds: Array[String] = ["设置阵法", "移除阵法", "开始解析", "准备施法", "使用行动", "设置法术", "附着符文", "解除符文", "弃置法术", "推进阶段", "确认决策", "投降", "响应", "放弃响应", "推进阶段", "盖伏", "翻开盖伏", "发动法术", "选择初始阵法"]
	var type: int = int(command.get("type", -1))
	return kinds[type] if type >= 0 and type < kinds.size() else "执行操作"

func _update_controls() -> void:
	_previous.disabled = _path.is_empty() or _step <= 0
	_next.disabled = _path.is_empty() or _step >= _rows.size()
	_position.text = "%d / %d" % [_step, _rows.size()]

func _show_detail(card: CardPresentation) -> void:
	_canvas.selected_id = card.instance_id
	_canvas.queue_redraw()
	_detail.configure(card)
	_detail.show()

func _hide_detail() -> void:
	_detail.hide()
	_canvas.selected_id = ""
	_canvas.queue_redraw()

func _toggle_hand() -> void:
	_canvas.set_hand_collapsed(not _canvas.hand_collapsed)

func _return() -> void:
	navigation_requested.emit(&"match" if service != null and service.match_active else &"menu")
