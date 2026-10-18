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
	_update_controls()

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
	_canvas.configure(FieldPresentation.new())
	_step = 0
	_hide_detail()
	var path: String = _paths[_files.selected]
	var verified: Dictionary = service.match_request(&"verify_replay", [path])
	if not bool(verified.get("ok", false)):
		_notice.text = "复盘验证失败：" + service.error
		_update_controls()
		return
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
		_frame_cache[cache_key] = frame
	_canvas.configure(MatchProjection.field(frame))
	_canvas.set_background_theme(str(service.settings.get("fieldBackground", "arcane")))
	_hide_detail()
	_command.text = "对局开始" if _step == 0 else "步骤 %d · 玩家 %d\n%s" % [_step, int(_rows[_step - 1].get("actor", 0)) + 1, _command_label(_rows[_step - 1].get("command", {}))]
	_update_controls()

func _command_label(command: Dictionary) -> String:
	var kinds: Array[String] = ["设置阵法", "拆除阵法", "开始解析", "准备施法", "使用行动", "设置法术", "附着符文", "解除符文", "弃置法术", "推进阶段", "确认决策", "投降", "响应", "放弃响应", "推进阶段", "盖伏", "翻开盖伏", "发动法术", "选择初始阵法"]
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
