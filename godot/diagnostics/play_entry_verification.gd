extends Node

var _app: WizardFrontend
var _route: String
var _clicked_card: String
var _failure: String
var _deadline: int

func _ready() -> void:
	_deadline = Time.get_ticks_msec() + 45000
	_run.call_deferred()

func _process(_delta: float) -> void:
	if Time.get_ticks_msec() > _deadline:
		_fail("timed out in " + str(_app.current_page))

func _fail(reason: String) -> bool:
	if _failure.is_empty():
		_failure = reason
		push_error("PLAY_ENTRY " + _route + ": " + reason)
		get_tree().quit(1)
	return false

func _settle() -> void:
	await get_tree().process_frame
	await get_tree().process_frame
	await RenderingServer.frame_post_draw

func _click(point: Vector2, viewport: Viewport) -> void:
	var motion := InputEventMouseMotion.new()
	motion.position = point
	viewport.push_input(motion, true)
	for pressed: bool in [true, false]:
		var event := InputEventMouseButton.new()
		event.position = point
		event.global_position = point
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		viewport.push_input(event, true)
	await _settle()

func _visible(control: Control) -> bool:
	return is_instance_valid(control) and control.is_visible_in_tree() and control.size.x > 2 and control.size.y > 2 and control.get_viewport_rect().encloses(control.get_global_rect())

func _press(control: Control, label: String) -> bool:
	if not _visible(control) or (control is BaseButton and (control as BaseButton).disabled):
		return _fail("missing, clipped or disabled " + label)
	await _click(control.get_global_rect().get_center(), control.get_viewport())
	return true

func _run() -> void:
	await _settle()
	var args := OS.get_cmdline_user_args()
	var index := args.find("--verify-play-entry")
	if index < 0 or index + 1 >= args.size():
		_fail("missing route")
		return
	_route = args[index + 1]
	var table: Control
	match _route:
		"ai":
			if not await _ai():
				return
		"tutorial":
			if not await _tutorial():
				return
		"tutorial-end":
			get_window().mode = Window.MODE_WINDOWED
			get_window().size = Vector2i(2560, 1440)
			await _settle()
			if await _tutorial() and await _finish_first_lesson():
				print("PLAY_ENTRY_PASS ", _route)
				get_tree().quit(0)
			return
		"replay":
			if not await _replay():
				return
		_:
			_fail("unknown route")
			return
	table = _app.page.get_node_or_null("%Table") as Control
	if table == null:
		_fail("board scene missing")
		return
	var canvas := table.get_node_or_null("%FieldCanvas") as FieldCanvas
	if not _visible(canvas):
		_fail("board not visible")
		return
	for path: String in ["%OwnStatus", "%OpponentStatus"]:
		var status := table.get_node_or_null(path) as PlayerStatusBar
		if not _visible(status) or not _visible(status._life) or not status._life.text.contains("/"):
			_fail("missing populated HUD " + path)
			return
	var hit_rect := Rect2()
	canvas.card_selected.connect(func(card: CardPresentation): _clicked_card = card.instance_id)
	for hit: Dictionary in canvas._card_hits:
		var card: CardPresentation = hit.card
		var rect := canvas.card_bounds(card)
		if card.hidden or not rect.has_area() or canvas.card_at(rect.get_center()) != card:
			continue
		var point := canvas.global_position + rect.get_center()
		await _click(point, canvas.get_viewport())
		if _clicked_card == card.instance_id:
			hit_rect = rect
			break
	if not hit_rect.has_area():
		_fail("no visible card received the mouse click")
		return
	var image := get_viewport().get_texture().get_image()
	if image == null or image.is_empty():
		_fail("empty viewport image")
		return
	# Sample the rendered card region to catch blank draw output with live hitboxes.
	var transform := canvas.get_viewport().get_final_transform() * canvas.get_global_transform_with_canvas()
	var region := Rect2i(transform * hit_rect).intersection(Rect2i(Vector2i.ZERO, image.get_size()))
	var colors := {}
	for y in range(region.position.y, region.end.y, maxi(1, region.size.y / 20)):
		for x in range(region.position.x, region.end.x, maxi(1, region.size.x / 20)):
			colors[image.get_pixel(x, y).to_rgba32()] = true
	if colors.size() < 8:
		_fail("card has no rendered detail")
		return
	var output := _app.service.user_directory.path_join(_route + ".png")
	if image.save_png(output) != OK:
		_fail("cannot write " + output)
		return
	print("PLAY_ENTRY_PASS ", _route, " card=", _clicked_card)
	get_tree().quit(0)

func _ai() -> bool:
	if not await _press(_app.page.get_node_or_null("%SinglePlayer"), "single player"):
		return false
	var setup := _app.page as SetupPage
	if setup == null or setup._mode.selected != 0:
		return _fail("challenge AI setup did not open")
	if not await _press(setup._start, "start match"):
		return false
	var page := _app.page as MatchPage
	if page == null or _app.service.match_mode != "ai":
		return _fail("AI match did not open")
	var confirmed := 0
	while bool(_app.service.match_snapshot.get("opening", false)):
		var panel := page._decision
		if panel.visible and not panel._options.is_empty() and not bool(_app.service.match_snapshot.get("aiTurn", false)):
			var option: Button = panel._options.values()[0]
			if not await _press(option, "opening choice") or not await _press(panel._confirm, "confirm opening choice"):
				return false
			confirmed += 1
		else:
			await _settle()
	while page._end_phase.disabled:
		await _settle()
	var revision := str(_app.service.match_snapshot.revision)
	if confirmed < 2 or not await _press(page._end_phase, "end phase"):
		return _fail("opening confirmation or phase action missing")
	if page._phase_selector.visible:
		if not await _press(page._phase_selector._choices.get_child(0), "choose next phase"):
			return false
	while str(_app.service.match_snapshot.revision) == revision:
		await _settle()
	return true

func _tutorial() -> bool:
	if not await _press(_app.page.get_node_or_null("%Library"), "guide"):
		return false
	var guide := _app.page as GuidePage
	if guide == null or guide._course_list.get_child_count() == 0:
		return _fail("tutorial chapter missing")
	if not await _press(guide._course_list.get_child(0) as Control, "chapter one"):
		return false
	var page := _app.page as MatchPage
	if page == null or _app.service.match_snapshot.get("tutorial", {}).get("step", "") != "welcome":
		return _fail("tutorial did not open")
	if not await _press(page._overlay.advance, "continue the first lesson"):
		return false
	if _app.service.match_snapshot.get("tutorial", {}).get("step", "") != "gesture":
		return _fail("tutorial step did not advance")
	# The real opening deals the hand only after both players confirm their gesture.
	var snapshot := _app.service.snapshot()
	var gesture := {"type": KernelContract.CommandKind.SubmitOpeningGesture, "round": str(snapshot.openingRound), "gesture": "0"}
	var response := _app.service.match_request(&"submit_command", [0, gesture, snapshot.generation, snapshot.revision])
	if not response.get("ok", false):
		return _fail("opening gesture was rejected")
	snapshot = _app.service.snapshot()
	if snapshot.tutorial.step == "gesture":
		response = _app.service.match_request(&"step_ai", [snapshot.generation, snapshot.revision])
		if not response.get("ok", false):
			return _fail("scripted opponent could not confirm its gesture")
	_app.service.poll_session()
	await _settle()
	return not page.canvas.field.hand.is_empty()

func _first_lesson_action(snapshot: Dictionary, operation: int, definition: String) -> Dictionary:
	var intent := {"source": "0", "selections": [{"field": KernelContract.ActionField.Operation, "value": str(operation)}]}
	for card: Dictionary in snapshot.cards:
		if card.owner == 0 and card.get("definition", {}).get("id") == definition:
			intent.source = str(card.id)
			break
	for field_index: int in range(12):
		var response := _app.service.match_request(&"query_actions", [0, intent, snapshot.generation, snapshot.revision])
		var options: Dictionary = response.get("data", {})
		if options.get("complete") is Dictionary:
			return options.complete
		if options.get("options", []).is_empty():
			return {}
		var choice: Dictionary = options.options[0]
		if options.field == KernelContract.ActionField.Target:
			for target: Dictionary in options.options:
				if target.get("player") == 1:
					choice = target
					break
		intent.selections.append({"field": options.field, "value": choice.value})
	return {}

func _finish_first_lesson() -> bool:
	const K = KernelContract.CommandKind
	const OPERATIONS := {
		"initial-formation": [K.SelectInitialFormation, "v260-formation-0009"],
		"upgrade": [K.SetFormation, "v260-formation-0010"],
		"place": [K.StartAnalysis, "v260-analytic-0006"],
		"prepare": [K.PrepareCast, "v260-analytic-0006"],
		"cast": [K.ActivateSpell, "v260-analytic-0006"],
	}
	var page := _app.page as MatchPage
	for tick: int in range(400):
		var snapshot := _app.service.snapshot()
		var lesson: Dictionary = snapshot.tutorial
		if lesson.complete and snapshot.result == 0:
			break
		var command: Dictionary = {}
		var method: StringName = &"submit_command"
		var args: Array = [snapshot.generation, snapshot.revision]
		if lesson.canContinue:
			method = &"continue_tutorial"
		elif snapshot.aiTurn:
			method = &"step_ai"
		else:
			if OPERATIONS.has(lesson.step):
				var action: Array = OPERATIONS[lesson.step]
				command = _first_lesson_action(snapshot, action[0], action[1])
			if command.is_empty() and snapshot.actionSummary.get("safeAdvance") is Dictionary:
				command = snapshot.actionSummary.safeAdvance
			elif command.is_empty() and snapshot.decision is Dictionary:
				command = _first_lesson_action(snapshot, K.Choose, "")
			if command.is_empty():
				return _fail("no legal move in first lesson at " + str(lesson.step))
			args = [0, command, snapshot.generation, snapshot.revision]
		var response := _app.service.match_request(method, args)
		if not response.get("ok", false):
			return _fail(str(lesson.step) + ": " + str(response.get("error")))
	_app.service.poll_session()
	if not _app.service.match_snapshot.tutorial.complete or _app.service.match_snapshot.result != 0:
		return _fail("first lesson did not end in victory at " + str(_app.service.match_snapshot.tutorial.step))
	page._refresh()
	await _settle()
	var terminal := page._lesson_end
	var panel := terminal.get_node("%Panel") as PanelContainer
	var viewport := page.get_viewport_rect()
	var bounds := panel.get_global_rect()
	print("Lesson end viewport=", viewport, " backdrop=", terminal.get_global_rect(), " panel=", bounds)
	if not _visible(terminal) or not terminal.get_global_rect().is_equal_approx(viewport):
		return _fail("first lesson victory backdrop does not fill the viewport")
	if not viewport.encloses(bounds) or bounds.get_center().distance_to(viewport.get_center()) > 2:
		return _fail("first lesson victory panel is clipped or off-center")
	for control: String in ["%Next", "%Retry", "%Menu"]:
		var button := terminal.get_node(control) as Button
		if not _visible(button) or not bounds.encloses(button.get_global_rect()):
			return _fail("first lesson victory action is clipped: " + control)
	if get_viewport().get_texture().get_image().save_png(_app.service.user_directory.path_join(_route + ".png")) != OK:
		return _fail("cannot capture first lesson victory")
	if not await _press(terminal.get_node("%Next"), "next lesson"):
		return false
	if _app.service.match_snapshot.get("tutorial", {}).get("lessonId") != "lesson-02-set-ambush":
		return _fail("next lesson did not open")
	return true

func _replay() -> bool:
	if not await _press(_app.page.get_node_or_null("%Replay"), "replay"):
		return false
	var page := _app.page as ReplayPage
	if page == null or page._path.get_file() != "acceptance.json" or page._rows.is_empty():
		return _fail("supplied replay failed integrity or command validation")
	while page._step == 0 or bool(page._frame.get("opening", false)):
		var previous := page._step
		if not await _press(page._next, "next replay step"):
			return false
		if page._step != previous + 1 or not page._frame_cache.has("%d:%d" % [page._step, page._viewer.selected]):
			return _fail("new replay frame did not load")
	return true
