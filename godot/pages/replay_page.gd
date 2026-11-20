class_name ReplayPage
extends Control

signal navigation_requested(page: StringName)

const BattleTable = preload("res://ui/battle_table.gd")

@onready var _table: BattleTable = %Table
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
var _logs_panel: PanelContainer
var _logs_text: RichTextLabel

func _ready() -> void:
	_files = %Files
	_notice = _table.notice
	_previous = %Previous
	_next = %Next
	_viewer = %Viewer
	_position = %Position
	_command = %Command
	_canvas = _table.canvas
	_detail = _table.detail
	(%Playback as VBoxContainer).reparent(_table.controls, false)
	(%Hand as Button).pressed.connect(_toggle_hand)
	(%Back as Button).pressed.connect(_return)
	(%Load as Button).pressed.connect(_load)
	_previous.pressed.connect(_move.bind(-1))
	_next.pressed.connect(_move.bind(1))
	_viewer.add_item("玩家一视角")
	_viewer.add_item("玩家二视角")
	_viewer.item_selected.connect(_viewer_changed)
	_canvas.card_selected.connect(_show_detail)
	_canvas.card_hovered.connect(_hover_card)
	_canvas.selection_cleared.connect(_hide_detail)
	_canvas.hand_toggle_requested.connect(_toggle_hand)
	_detail.closed.connect(_hide_detail)
	_status_own = _table.own
	_status_opponent = _table.opponent
	_logs_panel = _table.log_panel
	_logs_text = _table.log_text
	_canvas.layout_changed.connect(_position_controls)
	resized.connect(_layout_panels)
	_layout_panels()
	_update_controls()

func _layout_panels() -> void:
	_table.arrange()
	_position_controls()

func _position_controls() -> void:
	_table.place_casting_control(_table.clock, true)
	_table.place_casting_control(_next, false)

func configure(value: FrontendService) -> void:
	service = value
	_paths.clear()
	_files.clear()
	var directory: String = service.user_directory.path_join("replays").replace("\\", "/").simplify_path()
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
		var index: int = _paths.find(service.last_replay_path.replace("\\", "/").simplify_path())
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
	_table.show_frame({})
	_status_own.hide()
	_status_opponent.hide()
	_table.clock.text = ""
	_update_logs({})
	_command.clear()
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
	if int(plan.get("data", {}).get("schema", -1)) != KernelContract.SCHEMA:
		_notice.text = "复盘数据版本不匹配，请更新客户端。"
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
		if int(frame.get("schema", -1)) != KernelContract.SCHEMA:
			_notice.text = "复盘数据版本不匹配，请更新客户端。"
			return
		if _frame_cache.size() >= 16:
			_frame_cache.erase(_frame_cache.keys()[0])
		_frame_cache[cache_key] = frame
	_frame = frame
	var hand_collapsed: bool = _canvas.hand_collapsed
	_canvas.configure(MatchProjection.field(frame))
	_canvas.hand_collapsed = hand_collapsed
	_canvas.set_background_theme(str(service.settings.get("fieldBackground", "arcane")))
	_table.show_frame(frame)
	_table.show_clock(frame.get("clock", {}), _viewer.selected, 0)
	_update_status(frame)
	_update_logs(frame)
	_hide_detail()
	_command.text = "对局开始" if _step == 0 else "步骤 %d · 玩家 %d\n%s" % [_step, int(_rows[_step - 1].get("actor", 0)) + 1, _command_label(_rows[_step - 1].get("command", {}))]
	_update_controls()

func _update_status(frame: Dictionary) -> void:
	var players: Array = frame.get("players", [])
	if players.size() != 2:
		return
	_status_own.show()
	_status_opponent.show()
	var viewer: int = _viewer.selected
	var loads: Array = frame.get("loads", [])
	var cards: Array = frame.get("cards", [])
	var bars: Array[PlayerStatusBar] = [_status_own, _status_opponent]
	for index: int in range(2):
		var player: int = viewer if index == 0 else 1 - viewer
		var values: Dictionary = (players[player] as Dictionary).duplicate()
		bars[index].set_status("我方" if index == 0 else "对手", player, values, loads, cards, frame.get("debug", {}))

func _update_logs(frame: Dictionary) -> void:
	_table.update_events(frame.get("eventLog", []), true)

func _command_label(command: Dictionary) -> String:
	return MatchProjection.action_label(_frame, {"command": command})

func _update_controls() -> void:
	_previous.disabled = _path.is_empty() or _step <= 0
	_next.disabled = _path.is_empty() or _step >= _rows.size()
	_position.text = "%d / %d" % [_step, _rows.size()]

func _show_detail(card: CardPresentation) -> void:
	_canvas.selected_id = card.instance_id
	_canvas.queue_redraw()
	_detail.configure(card)
	_detail.show()

func _hover_card(card: CardPresentation) -> void:
	if card == null:
		card = MatchProjection.card(_frame, _canvas.selected_id)
	if card == null:
		_detail.clear_card()
	else:
		_detail.configure(card)

func _hide_detail() -> void:
	_detail.clear_card()
	_canvas.selected_id = ""
	_canvas.queue_redraw()

func _toggle_hand() -> void:
	_canvas.set_hand_collapsed(not _canvas.hand_collapsed)

func _return() -> void:
	navigation_requested.emit(&"match" if service != null and service.match_active else &"menu")
