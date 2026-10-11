class_name RoomsPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var _address: LineEdit
var _port: SpinBox
var _decks: OptionButton
var _host_first: CheckButton
var _automatic: CheckButton
var _notice: Label
var _room: Label
var _host: Button
var _join: Button
var _ready_button: Button
var _leave: Button
var _reconnect: Button
var _configure: Button

func _ready() -> void:
	var column: VBoxContainer = VBoxContainer.new()
	UiFactory.margin(self, 32).add_child(column)
	UiFactory.heading(column, "多人游戏", navigation_requested.emit.bind(&"menu"))
	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(scroll)
	var body: VBoxContainer = VBoxContainer.new()
	body.custom_minimum_size.x = 740
	body.add_theme_constant_override(&"separation", 14)
	scroll.add_child(body)
	body.add_child(UiFactory.label("本地双人换手", 28))
	body.add_child(UiFactory.label("在同一台设备上对战。每次换手前先遮挡私有信息。", 18, true))
	body.add_child(UiFactory.button("配置本地双人", _hotseat))
	body.add_child(HSeparator.new())
	body.add_child(UiFactory.label("局域网房间", 28))
	body.add_child(UiFactory.label("对方输入房主的局域网地址和相同端口。双方使用合法的 40+10 卡组。", 18, true))
	_address = LineEdit.new()
	_address.placeholder_text = "房主地址，例如 192.168.1.20"
	_address.text = "127.0.0.1"
	_address.custom_minimum_size.y = 48
	body.add_child(_address)
	_port = SpinBox.new()
	_port.min_value = 1024
	_port.max_value = 65535
	_port.value = 18761
	_port.custom_minimum_size.y = 48
	body.add_child(_port)
	body.add_child(UiFactory.label("入场卡组", 18))
	_decks = OptionButton.new()
	_decks.custom_minimum_size.y = 48
	_decks.item_selected.connect(_deck_changed)
	body.add_child(_decks)
	_host_first = CheckButton.new()
	_host_first.text = "房主先手"
	_host_first.button_pressed = true
	body.add_child(_host_first)
	_automatic = CheckButton.new()
	_automatic.text = "自动通过无操作阶段"
	_automatic.button_pressed = true
	body.add_child(_automatic)
	var controls: HFlowContainer = HFlowContainer.new()
	body.add_child(controls)
	_host = UiFactory.button("创建房间", _host_room, true)
	_join = UiFactory.button("加入房间", _join_room)
	_ready_button = UiFactory.button("准备", _ready_room)
	_leave = UiFactory.button("离开房间", _leave_room)
	_reconnect = UiFactory.button("重新连接", _reconnect_room)
	for button: Button in [_host, _join, _ready_button, _leave, _reconnect]:
		controls.add_child(button)
	_configure = UiFactory.button("应用房间设置", _configure_room)
	body.add_child(_configure)
	_room = UiFactory.label("", 18, true)
	body.add_child(_room)
	_notice = UiFactory.label("", 18, true)
	body.add_child(_notice)

func configure(value: FrontendService) -> void:
	service = value
	_decks.clear()
	if not service.refresh_library():
		_notice.text = service.error
	for deck: Dictionary in service.playable:
		_decks.add_item(str(deck.get("name", deck.get("id", ""))))
		_decks.set_item_metadata(_decks.item_count - 1, str(deck.get("id", "")))
	if not service.network_available():
		_notice.text = "当前原生模块未加载局域网房间接口。"
	_network_buttons(service.network_available())
	if service.network_active:
		network_updated(service.poll_network())

func _hotseat() -> void:
	service.setup_mode = "hotseat"
	navigation_requested.emit(&"setup")

func _network_buttons(available: bool) -> void:
	var has_room: bool = available and service.network_active
	_host.disabled = not available or has_room or _decks.selected < 0
	_join.disabled = not available or has_room or _decks.selected < 0
	_ready_button.disabled = not has_room
	_leave.disabled = not has_room
	_reconnect.disabled = not has_room
	var is_host: bool = has_room and bool(service.network_state.get("host", false))
	_configure.disabled = not is_host
	_host_first.disabled = not is_host
	_automatic.disabled = not is_host

func _deck_id() -> String:
	return str(_decks.get_item_metadata(_decks.selected)) if _decks.selected >= 0 else ""

func _send(request: Dictionary) -> void:
	var response: Dictionary = service.network_request(request)
	if not bool(response.get("ok", false)):
		_notice.text = service.error
		return
	_notice.text = str((response.get("data", {}) as Dictionary).get("notice", "请求已提交"))
	if service.network_active:
		network_updated(service.poll_network())
	else:
		_network_buttons(service.network_available())

func _host_room() -> void:
	_send({"operation": "host", "address": "0.0.0.0", "port": int(_port.value), "deck": _deck_id()})

func _join_room() -> void:
	_send({"operation": "join", "address": _address.text.strip_edges(), "port": int(_port.value), "deck": _deck_id()})

func _deck_changed(_index: int) -> void:
	_network_buttons(service != null and service.network_available())
	if service != null and service.network_active:
		_send({"operation": "deck", "deck": _deck_id()})

func _configure_room() -> void:
	_send({"operation": "configure", "hostFirst": _host_first.button_pressed, "automatic": _automatic.button_pressed})

func _ready_room() -> void:
	_send({"operation": "ready"})

func _reconnect_room() -> void:
	_send({"operation": "reconnect"})

func _leave_room() -> void:
	_send({"operation": "leave", "force": true})

func network_updated(state: Dictionary) -> void:
	if state.is_empty():
		if service != null and not service.error.is_empty():
			_notice.text = service.error
		return
	_network_buttons(service.network_available())
	var room: Dictionary = state.get("room", {})
	if bool(state.get("exists", state.has("room"))):
		var ready: Array = room.get("ready", [false, false])
		var host_player: int = int(room.get("hostPlayer", 0))
		var own_player: int = host_player if bool(state.get("host", false)) else 1 - host_player
		_ready_button.disabled = not bool(room.get("joined", false)) or (ready.size() == 2 and bool(ready[own_player]))
		_ready_button.text = "已准备" if ready.size() == 2 and bool(ready[own_player]) else "准备"
		_room.text = "你是%s · %s\n房主：%s　访客：%s\n%s先手 · %s自动通过阶段" % [
			"房主" if bool(state.get("host", false)) else "访客",
			"已连接" if bool(state.get("connected", false)) else "等待连接",
			"已准备" if ready.size() == 2 and bool(ready[host_player]) else "未准备",
			"已准备" if ready.size() == 2 and bool(ready[1 - host_player]) else "未准备",
			"房主" if int(room.get("hostPlayer", 0)) == 0 else "访客",
			"已开启" if bool(room.get("automatic", true)) else "已关闭"]
		_host_first.set_pressed_no_signal(int(room.get("hostPlayer", 0)) == 0)
		_automatic.set_pressed_no_signal(bool(room.get("automatic", true)))
	else:
		_room.text = "当前没有房间"
	_notice.text = str(state.get("notice", ""))
	if bool(state.get("started", false)):
		navigation_requested.emit(&"match")
