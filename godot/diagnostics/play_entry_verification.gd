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
	if guide == null or guide.chapter_buttons.is_empty():
		return _fail("tutorial chapter missing")
	if not await _press(guide.chapter_buttons[0], "chapter one"):
		return false
	var page := _app.page as MatchPage
	if page == null or int(_app.service.match_snapshot.get("tutorial", {}).get("step", -1)) != 0:
		return _fail("tutorial did not open")
	if not await _press(page._status_own, "inspect own status"):
		return false
	if int(_app.service.match_snapshot.get("tutorial", {}).get("step", -1)) != 1:
		return _fail("tutorial step did not advance")
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
