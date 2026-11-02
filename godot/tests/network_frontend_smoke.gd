extends SceneTree

var _host: WizardFrontend
var _guest: WizardFrontend
var _failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> bool:
	if not condition:
		_failures += 1
		push_error("NETWORK_FRONTEND: " + message)
	return condition

func _wait_for(predicate: Callable) -> bool:
	var deadline: int = Time.get_ticks_msec() + 10000
	while Time.get_ticks_msec() < deadline:
		_host._poll_network()
		_guest._poll_network()
		if predicate.call():
			return true
		await create_timer(0.005).timeout
	return false

func _run() -> void:
	_host = load("res://app/main.tscn").instantiate() as WizardFrontend
	_guest = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(_host)
	root.add_child(_guest)
	await process_frame
	if not _expect(_host.service.error.is_empty() and _guest.service.error.is_empty(), "Native clients initialize"):
		quit(2)
		return
	for iteration: int in range(3):
		_host.navigate(&"rooms")
		_guest.navigate(&"rooms")
		var host_page: RoomsPage = _host.page as RoomsPage
		var guest_page: RoomsPage = _guest.page as RoomsPage
		var port: int = 33720 + iteration
		host_page._port.value = port
		guest_page._port.value = port
		guest_page._address.text = "127.0.0.1"
		host_page._host.pressed.emit()
		guest_page._join.pressed.emit()
		# Exercise the pre-handshake DTO immediately through the real join button.
		_expect(_guest.service.network_active and _guest.service.network_state.get("room", {}) is Dictionary, "Joining client renders a stable room object")
		if not _expect(await _wait_for(func() -> bool: return bool(_host.service.network_state.get("connected", false)) and bool(_guest.service.network_state.get("connected", false))), "Loopback handshake completes"):
			break
		host_page._ready_button.pressed.emit()
		guest_page._ready_button.pressed.emit()
		if not _expect(await _wait_for(func() -> bool: return _host.page is MatchPage and _guest.page is MatchPage), "Ready clients both enter the match"):
			break
		_expect(_host.service.match_active and _guest.service.match_active, "Both native sessions started")
		_expect(bool(_guest.service.network_request({"operation": "leave", "force": true}).get("ok", false)), "Guest exits match and room")
		_expect(bool(_host.service.network_request({"operation": "leave", "force": true}).get("ok", false)), "Host exits match and room")
		_expect(not _guest.service.network_active and not _host.service.network_active, "Both clients clear room state")
	print("NETWORK_FRONTEND_SMOKE: ", _failures, " failures")
	_host.queue_free()
	_guest.queue_free()
	await process_frame
	quit(0 if _failures == 0 else 1)
