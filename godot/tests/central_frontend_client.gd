extends SceneTree

var _app: WizardFrontend
var _failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> bool:
	if not condition:
		_failures += 1
		push_error("CENTRAL_FRONTEND_CLIENT: " + message)
	return condition

func _argument(name: String, fallback: String = "") -> String:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == name:
			return arguments[index + 1]
	return fallback

func _wait_for(predicate: Callable, timeout_ms: int = 20000) -> bool:
	var deadline: int = Time.get_ticks_msec() + timeout_ms
	while Time.get_ticks_msec() < deadline:
		if predicate.call():
			return true
		await create_timer(0.01).timeout
	print("CENTRAL_CLIENT_DIAGNOSTIC: active=", _app.service.network_active,
		" notice=", _app.service.network_state.get("notice", ""),
		" error=", _app.service.error,
		" transportPhase=", _app._central_transport._phase)
	return predicate.call()

func _write_report(path: String, value: Dictionary) -> bool:
	var file: FileAccess = FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		return _expect(false, "cannot write client report: " + error_string(FileAccess.get_open_error()))
	file.store_string(JSON.stringify(value))
	var result: Error = file.get_error()
	file.close()
	return _expect(result == OK, "client report is written")

func _stop(code: int) -> void:
	if _app != null and _app.service.network_active:
		_app.service.network_request({"operation": "leave", "force": true})
	if _app != null:
		_app.queue_free()
	await process_frame
	quit(code)

func _run() -> void:
	var role: String = _argument("--role")
	var host: String = _argument("--hostname", "localhost")
	var port: int = int(_argument("--port", "24443"))
	var ca: String = _argument("--trusted-ca")
	var report_path: String = _argument("--report")
	var room_code: String = _argument("--room-code").strip_edges().to_upper()
	_app = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(_app)
	await process_frame
	if not _expect(_app.service.error.is_empty(), "Godot client initializes: " + _app.service.error):
		await _stop(2)
		return
	if not _expect(role in ["host", "guest"], "role must be host or guest"):
		await _stop(2)
		return
	_app.navigate(&"rooms")
	var room_page: RoomsPage = _app.page as RoomsPage
	room_page._address.text = host
	room_page._port.value = port
	room_page._trusted_ca.text = ca
	if role == "host":
		room_page._host.pressed.emit()
		if not await _wait_for(func() -> bool:
			var room: Dictionary = _app.service.network_state.get("room", {})
			return bool(_app.service.network_state.get("connected", false)) and not str(room.get("code", "")).is_empty()
		):
			_expect(false, "owner connects over verified TLS and receives a room code")
			await _stop(1)
			return
		room_code = str(_app.service.network_state.get("room", {}).get("code", ""))
		if not _write_report(report_path, {"role": role, "roomCode": room_code, "connected": true}):
			await _stop(1)
			return
		if not await _wait_for(func() -> bool:
			return bool(_app.service.network_state.get("room", {}).get("joined", false))
		):
			_expect(false, "guest joins the created room")
			await _stop(1)
			return
		_app.service.network_request({"operation": "reconnect"})
		_app.service.poll_network()
		if not _expect(not bool(_app.service.network_state.get("connected", true)), "manual reconnect invalidates the previous connection"):
			await _stop(1)
			return
		if not await _wait_for(func() -> bool:
			return bool(_app.service.network_state.get("connected", false))
		):
			_expect(false, "manual reconnect reopens TLS and restores the owner seat")
			await _stop(1)
			return
	else:
		if not _expect(room_code.length() == 8, "joiner receives an eight-character room code"):
			await _stop(2)
			return
		room_page._room_code.text = room_code
		room_page._join.pressed.emit()
		if not await _wait_for(func() -> bool:
			return bool(_app.service.network_state.get("connected", false)) and bool(_app.service.network_state.get("room", {}).get("joined", false))
		):
			_expect(false, "guest connects and joins using the room code")
			await _stop(1)
			return
	if not await _wait_for(func() -> bool:
		var room: Dictionary = _app.service.network_state.get("room", {})
		var online: Array = room.get("connected", [false, false])
		var ready: Array = room.get("ready", [false, false])
		return bool(online[0]) and bool(online[1]) and (role == "host" or bool(ready[0]))
	):
		_expect(false, "both seats are online before becoming ready")
		await _stop(1)
		return
	room_page._ready_button.pressed.emit()
	if not await _wait_for(func() -> bool:
		return bool(_app.service.network_state.get("started", false)) and _app.current_page == &"match"
	):
		_expect(false, "ready client enters the authoritative match")
		await _stop(1)
		return
	if role == "guest":
		_app.service.network_request({"operation": "leave"})
		if not await _wait_for(func() -> bool: return not _app.service.network_active):
			_expect(false, "surrender saves the replay and finishes leaving automatically")
			await _stop(1)
			return
		_write_report(report_path, {"role": role, "roomCode": room_code, "phase": "completed", "replayDelivered": true, "left": true})
		await _stop(0 if _failures == 0 else 1)
		return
	if not await _wait_for(func() -> bool:
		return bool(_app.service.network_state.get("replayDelivered", false))
	):
		_expect(false, "client receives and verifies the completed replay")
		await _stop(1)
		return
	var final_room: Dictionary = _app.service.network_state.get("room", {})
	var result: Dictionary = {
		"role": role,
		"roomCode": room_code,
		"phase": str(final_room.get("phase", "")),
		"connected": bool(_app.service.network_state.get("connected", false)),
		"replayDelivered": bool(_app.service.network_state.get("replayDelivered", false)),
		"result": int(_app.service.match_snapshot.get("result", -1)),
	}
	_expect(result.phase == "completed" and result.replayDelivered, "normal terminal result and replay are visible")
	_write_report(report_path, result)
	print("CENTRAL_FRONTEND_CLIENT: role=", role, " room=", room_code, " phase=", result.phase)
	await _stop(0 if _failures == 0 else 1)
