extends SceneTree

var _failures: int = 0
var _output: String = ""

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		_failures += 1
		push_error("RESPONSE_TIMING: " + message)

func _settle() -> void:
	await process_frame
	await process_frame

func _snapshot(with_response: bool = false) -> Dictionary:
	var snapshot: Dictionary = {
		"generation": "9007199254740993", "revision": "9007199254740995", "viewer": 0,
		"activePlayer": 0, "actingPlayer": 0, "phase": 2, "phaseName": "主要", "phaseStep": 2, "result": -1,
		"players": [{"life": 34, "lifeLimit": 36, "temporaryLife": 7, "ownTurn": 2, "sevenMode": 2}, {"life": 40, "lifeLimit": 40, "ownTurn": 1}], "cards": [],
		"decision": {"id": "9007199254740997", "player": 0, "kind": 0},
		"chain": {"mode": 0, "windowName": "主要阶段开始", "priority": 0},
		"actionSummary": {"hasNonPhaseAction": with_response, "safeAdvance": {"type": KernelContract.CommandKind.PassResponse, "decision": "9007199254740997"}},
	}
	return snapshot

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--output":
			_output = arguments[index + 1]
	if not _output.is_empty():
		DirAccess.make_dir_recursive_absolute(_output)
	var empty: Dictionary = _snapshot()
	var response: Dictionary = _snapshot(true)
	_expect(ResponseTiming.pass_action(empty, ResponseTiming.Mode.ALL).is_empty(), "all timing asks even when only pass is legal")
	_expect(ResponseTiming.pass_action(empty, ResponseTiming.Mode.AUTO).get("type", -1) == KernelContract.CommandKind.PassResponse, "automatic timing skips empty windows using the native pass")
	_expect(ResponseTiming.pass_action(response, ResponseTiming.Mode.AUTO).is_empty(), "automatic timing asks for legal responses")
	_expect(ResponseTiming.pass_action(response, ResponseTiming.Mode.IGNORE).get("type", -1) == KernelContract.CommandKind.PassResponse, "ignore timing passes a response opportunity")
	var forced: Dictionary = empty.duplicate(true)
	forced["decision"]["kind"] = 13
	forced["actionSummary"]["safeAdvance"] = null
	_expect(ResponseTiming.pass_action(forced, ResponseTiming.Mode.IGNORE).is_empty(), "ignore never skips effect choices even when they allow passing")
	var other: Dictionary = empty.duplicate(true)
	other["decision"]["player"] = 1
	_expect(ResponseTiming.pass_action(other, ResponseTiming.Mode.IGNORE).is_empty(), "local mode never passes the other player's priority")
	var malformed: Dictionary = empty.duplicate(true)
	malformed["actionSummary"]["safeAdvance"]["decision"] = "old"
	_expect(ResponseTiming.pass_action(malformed, ResponseTiming.Mode.IGNORE).is_empty(), "pass must belong to the current native decision")
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await _settle()
	var service: QueryFixtureService = QueryFixtureService.new()
	service.match_mode = "network"
	service.match_snapshot = empty
	page.service = service
	page._selection.service = service
	page._selection.changed.connect(page._query_changed)
	page._selection.completed.connect(page._query_completed)
	service.set_actions([{"id": "pass", "source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "9007199254740997"}}])
	page.canvas.configure(MatchProjection.field(empty))
	page._update_status()
	page._phase.text = "第 2 个己方回合\n玩家 1 · 主要"
	_expect(page._response_mode.item_count == 3 and page._current_response_mode() == ResponseTiming.Mode.AUTO, "response selector defaults to automatic with three modes")
	page._response_mode_changed(ResponseTiming.Mode.ALL)
	_expect(page._decision.visible and page._decision.request.pass_id == str(KernelContract.CommandKind.PassResponse), "all timing exposes an empty response tray with pass")
	page._response_mode_changed(ResponseTiming.Mode.AUTO)
	_expect(not page._decision.visible, "automatic mode hides the empty response tray")
	await create_timer(0.2).timeout
	_expect(service.selected_actions == ["pass"] and service.confirmations == 1, "automatic empty response submits exactly one native pass")
	service.selected_actions.clear()
	service.confirmations = 0
	page._network_waiting = false
	service.match_snapshot = response
	service.set_actions([{"id": "pass", "source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "9007199254740997"}}, {"id": "respond", "source": "33", "command": {"type": KernelContract.CommandKind.Respond, "card": "33", "decision": "9007199254740997"}}])
	page._render_decision()
	_expect(page._decision.visible and page._decision.request.choices.size() == 2, "automatic mode displays actual response options")
	page._response_mode_changed(ResponseTiming.Mode.IGNORE)
	await create_timer(0.2).timeout
	_expect(service.selected_actions == ["pass"] and service.confirmations == 1, "ignore submits pass instead of activating an available card")
	page._network_waiting = false
	service.selected_actions.clear()
	service.match_snapshot = forced
	service.set_actions([{"id": "mode", "command": {"type": KernelContract.CommandKind.Choose, "option": {"kind": KernelContract.ChoiceKind.Mode, "value": "1"}, "decision": "9007199254740997"}}])
	page._render_decision()
	_expect(page._decision.visible, "mandatory effect selection stays visible in ignore mode")
	service.match_snapshot = response
	service.set_actions([{"id": "pass", "source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "9007199254740997"}}, {"id": "respond", "source": "33", "command": {"type": KernelContract.CommandKind.Respond, "card": "33", "decision": "9007199254740997"}}])
	page._response_mode_changed(ResponseTiming.Mode.IGNORE)
	page._response_mode_changed(ResponseTiming.Mode.ALL)
	await create_timer(0.2).timeout
	_expect(service.selected_actions.is_empty() and page._decision.visible, "switching back to all cancels a queued automatic pass")
	service.match_viewer = 1
	_expect(page._current_response_mode() == ResponseTiming.Mode.AUTO, "hotseat viewer has an independent mode")
	service.match_viewer = 0
	service.match_snapshot = empty
	page._decision.hide()
	page._restore_tray_hand()
	page._render_actions()
	var typography: Node = root.get_node("GlobalTypography")
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1440, 900), Vector2i(1600, 1000), Vector2i(1920, 1080), Vector2i(2560, 1440)]:
		root.size = dimensions
		for scale: int in [90, 100, 110, 120]:
			typography.call(&"set_scale", scale)
			await _settle()
			page._position_phase_button()
			var rect: Rect2 = page._response_mode.get_global_rect()
			_expect(Rect2(Vector2.ZERO, dimensions).encloses(rect), "selector remains in viewport at %s / %d" % [dimensions, scale])
			_expect(not rect.intersects(page._end_phase.get_global_rect()) and not rect.intersects(page.canvas._own_region), "selector clears phase button and own formation area")
			if not _output.is_empty() and scale == 120:
				await RenderingServer.frame_post_draw
				_expect(root.get_texture().get_image().save_png(_output.path_join("response-mode-%dx%d.png" % [dimensions.x, dimensions.y])) == OK, "capture selector")
	typography.call(&"set_scale", 100)
	page.queue_free()
	await process_frame
	print("Response timing smoke: %s" % ("PASS" if _failures == 0 else "FAIL"))
	quit(0 if _failures == 0 else 2)
