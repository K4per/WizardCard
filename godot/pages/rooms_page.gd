class_name RoomsPage
extends Control

signal navigation_requested(page: StringName)

const PRODUCTION_HOST: String = "aq.rainplay.cn"
const PRODUCTION_PORT: int = 23513
const PRODUCTION_CA: String = "res://data/aq.rainplay.cn.crt"

var _advanced: VBoxContainer
var service: FrontendService
var _address: LineEdit
var _port: SpinBox
var _room_code: LineEdit
var _trusted_ca: LineEdit
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
	_address = %Address
	_port = %Port
	_room_code = %RoomCode
	_trusted_ca = %TrustedCa
	_decks = %Decks
	_host_first = %HostFirst
	_host_first.hide()
	_automatic = %Automatic
	_notice = %Notice
	_room = %Room
	_host = %Host
	_join = %Join
	_ready_button = %Ready
	_leave = %Leave
	_reconnect = %Reconnect
	_configure = %Configure
	var body: VBoxContainer = _address.get_parent() as VBoxContainer
	var insertion: int = _address.get_index()
	var advanced_toggle: CheckButton = CheckButton.new()
	advanced_toggle.text = GameText.label("ui.rooms.advanced")
	body.add_child(advanced_toggle)
	body.move_child(advanced_toggle, insertion)
	_advanced = VBoxContainer.new()
	body.add_child(_advanced)
	body.move_child(_advanced, insertion + 1)
	for node: Node in [_address, body.get_node("PortCaption"), _port, body.get_node("TrustedCaCaption"), _trusted_ca]:
		node.reparent(_advanced)
	_advanced.add_child(UiFactory.button(GameText.label("ui.rooms.restore_production"), _restore_production))
	advanced_toggle.toggled.connect(_advanced.set_visible)
	_advanced.hide()
	_restore_production()
	(%Back as Button).pressed.connect(navigation_requested.emit.bind(&"menu"))
	(%Hotseat as Button).pressed.connect(_hotseat)
	_decks.item_selected.connect(_deck_changed)
	_host.pressed.connect(_host_room)
	_join.pressed.connect(_join_room)
	_room_code.text_changed.connect(_room_code_changed)
	_ready_button.pressed.connect(_ready_room)
	_leave.pressed.connect(_leave_room)
	_reconnect.pressed.connect(_reconnect_room)
	_configure.pressed.connect(_configure_room)

func configure(value: FrontendService) -> void:
	service = value
	_decks.clear()
	if not service.refresh_library():
		_notice.text = service.error
	for deck: Dictionary in service.playable:
		_decks.add_item(str(deck.get("name", deck.get("id", ""))))
		_decks.set_item_metadata(_decks.item_count - 1, str(deck.get("id", "")))
	if not service.network_available():
		_notice.text = GameText.label("ui.rooms.interface_unavailable")
	_network_buttons(service.network_available())
	if service.network_active:
		network_updated(service.poll_network())

func _hotseat() -> void:
	service.setup_mode = "hotseat"
	navigation_requested.emit(&"setup")

func _network_buttons(available: bool) -> void:
	var has_room: bool = available and service.network_active
	_host.disabled = not available or has_room or _decks.selected < 0
	_join.disabled = not available or has_room or _decks.selected < 0 or _room_code.text.strip_edges().length() != 8
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
	_notice.text = str((response.get("data", {}) as Dictionary).get("notice", GameText.label("ui.rooms.request_submitted")))
	if service.network_active:
		network_updated(service.poll_network())
	else:
		_network_buttons(service.network_available())

func _host_room() -> void:
	_send({"operation": "central_create", "hostname": _address.text.strip_edges(), "port": int(_port.value), "trustedCa": _trusted_ca.text.strip_edges(), "deck": _deck_id()})

func _join_room() -> void:
	var code: String = _room_code.text.strip_edges().to_upper()
	if code.length() != 8:
		_notice.text = GameText.label("ui.rooms.invalid_code")
		return
	_send({"operation": "central_join", "hostname": _address.text.strip_edges(), "port": int(_port.value), "trustedCa": _trusted_ca.text.strip_edges(), "roomCode": code, "deck": _deck_id()})

func _room_code_changed(_value: String) -> void:
	_network_buttons(service != null and service.network_available())

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
	var room_value: Variant = state.get("room", {})
	var room: Dictionary = room_value as Dictionary if room_value is Dictionary else {}
	var room_notice: String = str(state.get("notice", ""))
	if bool(state.get("exists", state.has("room"))):
		var ready: Array = room.get("ready", [false, false])
		var own_player: int = int(room.get("seat", 0))
		var owner: bool = bool(room.get("owner", state.get("host", false)))
		var phase: String = str(room.get("phase", "waiting"))
		var occupied: Array = room.get("occupied", [false, false])
		var both_present: bool = occupied.size() == 2 and bool(occupied[0]) and bool(occupied[1])
		_ready_button.disabled = not bool(state.get("connected", false)) or phase != "waiting" or not both_present or (ready.size() == 2 and bool(ready[own_player]))
		_ready_button.text = GameText.label("ui.rooms.ready") if ready.size() == 2 and bool(ready[own_player]) else GameText.label("ui.rooms.waiting_ready")
		_configure.disabled = not owner or phase != "waiting" or not bool(state.get("connected", false))
		_host_first.disabled = _configure.disabled
		_automatic.disabled = _configure.disabled
		var seats: Array[String] = []
		for seat: int in range(2):
			var occupied_seat: bool = occupied.size() == 2 and bool(occupied[seat])
			var seat_ready: bool = ready.size() == 2 and bool(ready[seat])
			seats.append(GameText.label("ui.rooms.seat", {"seat": seat + 1, "status": GameText.label("ui.rooms.ready") if seat_ready else GameText.label("ui.rooms.not_ready") if occupied_seat else GameText.label("ui.rooms.empty_seat"), "viewer": GameText.label("ui.rooms.you") if seat == own_player else ""}))
		var code: String = str(room.get("code", ""))
		_room.text = GameText.label("ui.rooms.summary", {
			"code": code if not code.is_empty() else GameText.label("ui.rooms.connecting"),
			"role": GameText.label("ui.rooms.owner") if owner else GameText.label("ui.rooms.guest"),
			"connection": GameText.label("ui.rooms.connected") if bool(state.get("connected", false)) else GameText.label("ui.rooms.connecting_server"),
			"first_seat": seats[0],
			"second_seat": seats[1],
			"first_player": GameText.label("ui.rooms.owner") if bool(room.get("ownerFirst", true)) else GameText.label("ui.rooms.guest"),
			"automatic": GameText.label("ui.rooms.enabled") if bool(room.get("automatic", true)) else GameText.label("ui.rooms.disabled")
		})
		_host_first.set_pressed_no_signal(bool(room.get("ownerFirst", true)))
		_automatic.set_pressed_no_signal(bool(room.get("automatic", true)))
		if bool(room.get("aborted", false)) or phase == "aborted":
			room_notice = GameText.label("ui.rooms.aborted", {"reason": str(room.get("abortReason", GameText.label("ui.rooms.unknown")))})
	else:
		_room.text = GameText.label("ui.rooms.empty")
	_notice.text = room_notice
	if bool(state.get("started", false)):
		navigation_requested.emit(&"match")

func _restore_production() -> void:
	_address.text = PRODUCTION_HOST
	_port.value = PRODUCTION_PORT
	_trusted_ca.text = PRODUCTION_CA
