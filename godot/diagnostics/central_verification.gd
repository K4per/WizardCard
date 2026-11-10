extends Node

var _app: WizardFrontend
var _failures: int = 0
var _released_spell: bool = false
var _opening_privacy: bool = false
var _opening_tie: bool = false
var _default_connection: bool = false

func _ready() -> void:
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
		await get_tree().create_timer(0.01).timeout
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

func _verification_deck() -> String:
	# Use a normal, validated 40+10 draft in the isolated verification profile.
	# A word deck avoids spending all income on new analysis before casting.
	if not _expect(_app.service.create_draft(), "create verification draft"):
		return ""
	var cards: Dictionary = {}
	var formations: Dictionary = {}
	for index: int in range(1, 21):
		cards["v260-word-%04d" % index] = 2
	for index: int in range(1, 11):
		formations["v260-formation-%04d" % index] = 1
	_app.service.editing_draft["name"] = "Alpha 2.6.3 release verification"
	_app.service.editing_draft["cards"] = cards
	_app.service.editing_draft["sideCards"] = formations
	if not _expect(_app.service.draft_problems().is_empty() and _app.service.save_draft(), "verification deck passes normal 40+10 validation"):
		return ""
	var deck_id: String = "user/" + str(_app.service.saved_draft.get("id", ""))
	if not _expect(_app.service.playable.any(func(deck: Dictionary) -> bool: return str(deck.get("id", "")) == deck_id),
		"saved verification deck is available for play"):
		return ""
	return deck_id

func _stop(code: int) -> void:
	if _app != null and _app.service.network_active:
		_app.service.network_request({"operation": "leave", "force": code != 0})
		var left: bool = await _wait_for(func() -> bool: return not _app.service.network_active, 5000)
		if code == 0 and not _expect(left, "terminal leave is acknowledged before closing TLS"):
			code = 1
	if _app != null:
		_app.queue_free()
	await get_tree().process_frame
	get_tree().quit(code)

func _run() -> void:
	var role: String = _argument("--role")
	var host: String = _argument("--hostname")
	var port: int = int(_argument("--port", "23513"))
	var ca: String = _argument("--trusted-ca")
	var report_path: String = _argument("--report")
	var room_code: String = _argument("--room-code").strip_edges().to_upper()
	if _app == null:
		_app = load("res://app/main.tscn").instantiate() as WizardFrontend
		get_tree().root.add_child(_app)
	await get_tree().process_frame
	if not _expect(_app.service.error.is_empty(), "Godot client initializes: " + _app.service.error):
		await _stop(2)
		return
	_app.service.events_changed.connect(func(batch: Dictionary) -> void:
		for event: Dictionary in batch.get("events", []):
			if bool(event.get("spellReleased", false)):
				_released_spell = true
	)
	if not _expect(role in ["host", "guest"], "role must be host or guest"):
		await _stop(2)
		return
	var deck_id: String = _verification_deck()
	if not _expect(not deck_id.is_empty(), "verification deck is saved"):
		await _stop(2)
		return
	_app.navigate(&"rooms")
	var room_page: RoomsPage = _app.page as RoomsPage
	for index: int in range(room_page._decks.item_count):
		if str(room_page._decks.get_item_metadata(index)) == deck_id:
			room_page._decks.select(index)
			break
	if not _expect(room_page._deck_id() == deck_id, "room uses the saved verification deck"):
		await _stop(2)
		return
	_default_connection = host.is_empty() and ca.is_empty()
	if not host.is_empty():
		room_page._address.text = host
		room_page._port.value = port
	if not ca.is_empty():
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
	var stable_until: int = Time.get_ticks_msec() + int(_argument("--stable-ms", "5000"))
	while Time.get_ticks_msec() < stable_until:
		await get_tree().create_timer(0.01).timeout
		if not _expect(bool(_app.service.network_state.get("connected", false)),
			"match connection stays online: " + str(_app.service.network_state.get("notice", ""))):
			await _stop(1)
			return
	if not await _play_opening(role):
		await _stop(1)
		return
	if role == "host":
		_app.service.network_request({"operation": "reconnect"})
		_app.service.poll_network()
		if not _expect(not bool(_app.service.network_state.get("connected", true)), "in-match reconnect first closes the old connection"):
			await _stop(1)
			return
		if not await _wait_for(func() -> bool:
			return bool(_app.service.network_state.get("connected", false)) and not bool(_app.service.network_state.get("room", {}).get("reconnecting", true))
		):
			_expect(false, "in-match reconnect restores both seats without a query flood")
			await _stop(1)
			return
	if not await _wait_for(func() -> bool:
		return bool(_app.service.network_state.get("connected", false)) and not bool(_app.service.network_state.get("room", {}).get("reconnecting", true))
	):
		_expect(false, "both clients resume before gameplay commands")
		await _stop(1)
		return
	var gameplay_deadline: int = Time.get_ticks_msec() + 300000
	var target_revision: int = int(_app.service.match_snapshot.get("revision", "0")) + int(_argument("--gameplay-actions", "40"))
	var submitted_revision: String = ""
	var query_context: String = ""
	var gameplay_intent: Dictionary = {"selections": []}
	while (int(_app.service.match_snapshot.get("revision", "0")) < target_revision or not _released_spell) and int(_app.service.match_snapshot.get("result", -1)) < 0:
		if not _expect(Time.get_ticks_msec() < gameplay_deadline,
			"continuous authoritative gameplay finishes after opening: " + JSON.stringify(_app.service.match_snapshot.get("actionSummary", {}))):
			await _stop(1)
			return
		if not _expect(bool(_app.service.network_state.get("connected", false)), "gameplay stays connected"):
			await _stop(1)
			return
		var snapshot: Dictionary = _app.service.match_snapshot
		var revision: String = str(snapshot.get("revision", ""))
		var generation: String = str(snapshot.get("generation", ""))
		if query_context != generation + ":" + revision:
			query_context = generation + ":" + revision
			gameplay_intent = {"selections": []}
		if revision != submitted_revision and not bool(_app.service.network_state.get("room", {}).get("reconnecting", false)):
			var query: Dictionary = _app.service.match_request(&"query_actions", [
				_app.service.match_viewer, gameplay_intent, generation, revision])
			if not bool(query.get("ok", false)) and (str(query.get("errorCode", "")) == "stale_query" \
				or str(query.get("error", "")) in ["network_wait", "match_paused", "room_paused"]):
				await get_tree().create_timer(0.01).timeout
				continue
			if not _expect(bool(query.get("ok", false)), "gameplay action query succeeds: " + str(query.get("error", ""))):
				await _stop(1)
				return
			var data: Dictionary = query.get("data", {})
			if data.get("complete") is Dictionary:
				var response: Dictionary = _app.service.match_request(&"submit_command", [
					_app.service.match_viewer, data["complete"], generation, revision])
				if not _expect(bool(response.get("ok", false)), "authoritative gameplay command is accepted"):
					await _stop(1)
					return
				submitted_revision = revision
			elif not bool(data.get("pending", false)):
				var candidates: Array = data.get("options", [])
				if int(data.get("field", -1)) == KernelContract.ActionField.Operation:
					for preferred: int in [KernelContract.CommandKind.ActivateSpell, KernelContract.CommandKind.PrepareCast,
						KernelContract.CommandKind.PreloadWord, KernelContract.CommandKind.StartAnalysis,
						KernelContract.CommandKind.AttachSeal, KernelContract.CommandKind.SetFormation,
						KernelContract.CommandKind.Respond, KernelContract.CommandKind.AdvancePhase,
						KernelContract.CommandKind.Advance, KernelContract.CommandKind.PassResponse, KernelContract.CommandKind.Choose]:
						var matching: Array = candidates.filter(func(candidate: Dictionary) -> bool: return int(candidate["value"]) == preferred)
						if not matching.is_empty():
							candidates = matching
							break
				for candidate: Dictionary in candidates:
					gameplay_intent["selections"].append({"field": data["field"], "value": candidate["value"]})
					break
		await get_tree().create_timer(0.01).timeout
	if role == "guest":
		await get_tree().create_timer(4.0).timeout
		_app.service.network_request({"operation": "leave"})
		if not await _wait_for(func() -> bool: return not _app.service.network_active):
			_expect(false, "surrender saves the replay and finishes leaving automatically")
			await _stop(1)
			return
		_write_report(report_path, {"role": role, "roomCode": room_code, "phase": "completed", "replayDelivered": true, "left": true, "releasedSpellObserved": _released_spell, "openingPrivacy": _opening_privacy, "openingTieObserved": _opening_tie, "defaultConnection": _default_connection})
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
		"releasedSpellObserved": _released_spell,
		"openingPrivacy": _opening_privacy,
		"openingTieObserved": _opening_tie,
		"defaultConnection": _default_connection,
	}
	_expect(result.phase == "completed" and result.replayDelivered, "normal terminal result and replay are visible")
	_write_report(report_path, result)
	print("CENTRAL_FRONTEND_CLIENT: role=", role, " room=", room_code, " phase=", result.phase)
	await _stop(0 if _failures == 0 else 1)

func _play_opening(role: String) -> bool:
	var submitted: String = ""
	var deadline: int = Time.get_ticks_msec() + 60000
	while bool(_app.service.match_snapshot.get("opening", true)):
		if not _expect(Time.get_ticks_msec() < deadline, "opening UI finishes ties, gestures and formations"):
			return false
		var snapshot: Dictionary = _app.service.match_snapshot
		var stage: String = str(snapshot.get("openingStage", ""))
		var round_number: int = int(snapshot.get("openingRound", 1))
		var key: String = "%s:%d" % [stage, round_number]
		var viewer: int = _app.service.match_viewer
		if stage == "gestures":
			_opening_tie = _opening_tie or round_number > 1
			var revealed: Array = snapshot.get("revealedGestures", [-1, -1])
			var confirmed: Array = snapshot.get("gestureConfirmed", [false, false])
			if round_number == 1 and bool(confirmed[1 - viewer]) and not bool(confirmed[viewer]):
				if not _expect(int(snapshot.get("openingGesture", -1)) == -1 and revealed == [-1, -1], "opponent gesture stays private before both submit"):
					return false
				_opening_privacy = true
			# Guest observes the private first submission; host observes its own
			# private acknowledgement before the guest can complete the round.
			if role == "guest" and not bool(confirmed[0]):
				await get_tree().create_timer(0.01).timeout
				continue
			if int(snapshot.get("openingGesture", -1)) >= 0:
				if round_number == 1:
					_opening_privacy = _opening_privacy or revealed == [-1, -1]
				await get_tree().create_timer(0.01).timeout
				continue
		var page: MatchPage = _app.page as MatchPage
		var panel: DecisionPanel = page._decision
		if key != submitted and panel.visible and panel.request != null and not panel.request.choices.is_empty() \
			and panel.request.generation == str(snapshot.get("generation")) and panel.request.revision == str(snapshot.get("revision")):
			var choice: DecisionChoice = panel.request.choices[0]
			if stage == "gestures":
				var desired: String = "2" if role == "guest" and round_number > 1 else "0"
				for candidate: DecisionChoice in panel.request.choices:
					if candidate.id == desired:
						choice = candidate
			var option: Button = panel._options[choice.id]
			option.button_pressed = true
			panel._confirm.pressed.emit()
			submitted = key
		await get_tree().create_timer(0.01).timeout
	return _expect(_opening_tie and _opening_privacy, "opening tie and private confirmation observed")
