extends SceneTree

var _failures: Array[String] = []
var _capture_output: String = ""

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		_failures.append(message)

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--capture-output" and DisplayServer.get_name() != "headless":
			_capture_output = arguments[index + 1]
	if not _capture_output.is_empty():
		DirAccess.make_dir_recursive_absolute(_capture_output)
	var match_page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(match_page)
	await process_frame
	_expect(match_page._status_own is PlayerStatusBar, "match own resource bar is present")
	_expect(match_page._status_opponent is PlayerStatusBar, "match opponent resource bar is present")
	_expect(not match_page._logs_panel.visible, "match log panel defaults hidden")
	_expect(match_page._confirm is ActionConfirmation, "confirmation uses centered action popup")
	_expect(match_page.get_node("ConfirmShade").mouse_filter == Control.MOUSE_FILTER_STOP, "confirmation shade blocks board input")
	match_page._status_own.set_status("我方", 0, {"life": 35, "temporaryLife": 4, "mana": 7, "load": 3, "capacity": 6}, [{"owner": 0, "source": "9", "amount": 2, "sourceBound": true}], [{"id": "9", "definition": {"name": "测试阵法"}}])
	_expect(match_page._status_own._life.text == "生命 35", "resource bar labels ordinary life")
	_expect(match_page._status_own._temporary_life.text == "临时生命 +4", "resource bar labels temporary life")
	_expect(match_page._status_own._mana.text == "魔素 7/20", "resource bar labels mana cap")
	_expect(match_page._status_own._load.text == "荷载 3/6", "resource bar labels load cap")
	var replay: ReplayPage = (load("res://pages/replay_page.tscn") as PackedScene).instantiate() as ReplayPage
	root.add_child(replay)
	await process_frame
	_expect(replay._status_own is PlayerStatusBar and replay._status_opponent is PlayerStatusBar, "replay exposes both resource bars")
	_expect(not replay._logs_panel.visible, "replay log panel defaults hidden")
	await _native_debug_checks(match_page, replay)
	replay.hide()
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1920, 1080), Vector2i(2560, 1440)]:
		root.size = dimensions
		match_page.show()
		match_page._layout_panels()
		await process_frame
		await process_frame
		_check_corner_bars(match_page._status_own, match_page._status_opponent, dimensions)
		for hit: Dictionary in match_page.canvas._zone_hits:
			var zone: Rect2 = hit["rect"]
			_expect(not zone.intersects(match_page._status_own.get_rect()) and not zone.intersects(match_page._status_opponent.get_rect()), "match pile remains clear of resource panels " + str(dimensions))
		for hit: Dictionary in match_page.canvas._card_hits:
			var card_rect: Rect2 = hit["rect"]
			_expect(not card_rect.intersects(match_page._status_own.get_rect()) and not card_rect.intersects(match_page._status_opponent.get_rect()), "match card remains clear of resource panels " + str(dimensions))
		await _capture("match-corners", dimensions)
		match_page._toggle_logs()
		_expect(not match_page._logs_panel.get_rect().intersects(match_page._status_own.get_rect()), "log drawer leaves own resource panel visible")
		await _capture("match-logs", dimensions)
		match_page._confirm.dialog_text = "确认长文案操作：".repeat(36)
		match_page._confirm.popup_centered()
		await process_frame
		await process_frame
		var bounds: Rect2 = Rect2(Vector2.ZERO, dimensions)
		_expect(bounds.encloses(match_page._confirm.get_global_rect()), "confirmation fits viewport " + str(dimensions))
		_expect(match_page._confirm.get_global_rect().encloses(match_page._confirm.get_node("Column/Actions/Buttons/Confirm").get_global_rect()), "confirmation button stays visible")
		_expect(match_page._confirm.z_index > match_page._logs_panel.z_index, "confirmation remains above the log panel")
		await _capture("match-confirmation", dimensions)
		match_page._confirm.close_popup()
		match_page._toggle_logs()
		match_page.hide()
		replay.show()
		replay._layout_panels()
		await process_frame
		await process_frame
		_check_corner_bars(replay._status_own, replay._status_opponent, dimensions)
		_expect(not replay._canvas.get_global_rect().intersects(replay._status_own.get_global_rect()), "replay board leaves bottom resource panel clear")
		_expect(not replay._canvas.get_global_rect().intersects(replay._status_opponent.get_global_rect()), "replay board leaves top resource panel clear")
		_expect(not replay._command.get_global_rect().intersects(replay._status_own.get_global_rect()), "replay command text leaves own resource panel clear")
		await _capture("replay-corners", dimensions)
		replay._toggle_logs()
		replay._layout_panels()
		_expect(not replay._logs_panel.get_global_rect().intersects(replay._status_own.get_global_rect()), "replay logs leave own resource panel clear")
		await _capture("replay-logs", dimensions)
		replay._toggle_logs()
		replay.hide()
	if _failures.is_empty():
		print("ALPHA_V253_UI: PASS")
		quit(0)
	else:
		for failure: String in _failures:
			push_error("ALPHA_V253_UI: " + failure)
		quit(2)

func _check_corner_bars(own: PlayerStatusBar, opponent: PlayerStatusBar, dimensions: Vector2i) -> void:
	var bounds: Rect2 = Rect2(Vector2.ZERO, dimensions)
	_expect(bounds.encloses(own.get_global_rect()) and bounds.encloses(opponent.get_global_rect()), "both resource panels fit viewport " + str(dimensions))
	_expect(own.position.x < 20.0 and own.get_rect().end.y > dimensions.y - 20.0, "own resource panel sits in bottom-left corner")
	_expect(opponent.position.y < 20.0 and opponent.get_rect().end.x > dimensions.x - 20.0, "opponent resource panel sits in top-right corner")
	for bar: PlayerStatusBar in [own, opponent]:
		for label: Label in [bar._life, bar._temporary_life, bar._mana, bar._load]:
			var text_width: float = label.get_theme_font(&"font").get_string_size(label.text, HORIZONTAL_ALIGNMENT_LEFT, -1, label.get_theme_font_size(&"font_size")).x
			_expect(label.size.x >= text_width - 1.0, "resource label has visible text width: " + label.text)
			_expect(bar.get_global_rect().encloses(label.get_global_rect()), "resource label stays inside its corner panel")

func _capture(label: String, dimensions: Vector2i) -> void:
	if _capture_output.is_empty():
		return
	await process_frame
	await RenderingServer.frame_post_draw
	var capture: Image = root.get_texture().get_image()
	_expect(capture.save_png(_capture_output.path_join(label + "-%dx%d.png" % [dimensions.x, dimensions.y])) == OK, "save screenshot")

func _native_debug_checks(match_page: MatchPage, replay_page: ReplayPage) -> void:
	var bridge: WizardBridge = WizardBridge.new()
	var assets: String = ProjectSettings.globalize_path("res://../assets")
	var user_dir: String = ProjectSettings.globalize_path("res://../build/alpha-v253-ui-data")
	_expect(bool(bridge.initialize(assets, user_dir).get("ok", false)), "native bridge initialized")
	_expect(bool(bridge.start_match({"mode": "ai", "seed": 253}).get("ok", false)), "AI session started")
	for attempt: int in range(10):
		var view: Dictionary = bridge.snapshot(0).get("data", {})
		if not bool(view.get("opening", false)):
			break
		if bool(view.get("aiTurn", false)):
			_expect(bool(bridge.step_ai(str(view.generation), str(view.revision)).get("ok", false)), "AI opening action")
		else:
			var actions: Array = view.get("actions", [])
			_expect(not actions.is_empty(), "human opening has legal choice")
			if actions.is_empty():
				return
			_expect(bool(bridge.select_action(0, str(actions[0].id), str(view.generation), str(view.revision)).get("ok", false)), "select initial formation")
			_expect(bool(bridge.confirm_action(str(view.generation), str(view.revision)).get("ok", false)), "confirm initial formation")
	var before: Dictionary = bridge.snapshot(0).get("data", {})
	_expect(not bool(before.get("opening", true)), "opening completed")
	_expect(bool(bridge.debug_command({"operation": "set_infinite_mana", "player": 0, "enabled": true}).get("ok", false)), "debug calls native authority")
	_expect(bool(bridge.debug_command({"operation": "gain_temporary_life", "player": 1, "amount": 6}).get("ok", false)), "debug adjusts opponent temporary life")
	var current: Dictionary = bridge.snapshot(0).get("data", {})
	match_page.canvas.configure(MatchProjection.field(current))
	match_page._append_log_events(current)
	var log_text: String = match_page._logs_text.get_parsed_text()
	_expect(not log_text.is_empty(), "native event descriptions populate the match log")
	match_page._append_log_events(current)
	_expect(match_page._logs_text.get_parsed_text() == log_text, "repeated snapshots do not duplicate log events")
	# Reserve the widest legal hand fan while testing corner HUD clearance.
	if not match_page.canvas.field.hand.is_empty():
		var sample: CardPresentation = match_page.canvas.field.hand[0]
		for index: int in range(match_page.canvas.field.hand.size(), 20):
			var hand_card: CardPresentation = sample.duplicate() as CardPresentation
			hand_card.instance_id = "qa-hand-%d" % index
			match_page.canvas.field.hand.append(hand_card)
		match_page.canvas.field.opponent_hand_count = 20
		match_page.canvas.queue_redraw()
	var players: Array = current.get("players", [])
	if players.size() != 2:
		_expect(false, "both players projected")
		return
	match_page._status_own.set_status("我方", 0, players[0])
	match_page._status_opponent.set_status("对手", 1, players[1])
	_expect(match_page._status_own._mana.text.contains("∞"), "native infinite state displayed")
	var saved: Dictionary = bridge.save_replay().get("data", {})
	_expect(not saved.is_empty(), "debug replay saved")
	if saved.is_empty():
		return
	var path: String = str(saved.path)
	var verified: Dictionary = bridge.verify_replay(path).get("data", {})
	_expect(not verified.is_empty(), "debug recording verifies")
	var latest_frame: Dictionary = bridge.replay_frame(path, int(verified.get("commandCount", 0)), 0).get("data", {})
	var frame: Dictionary = latest_frame
	replay_page._canvas.configure(MatchProjection.field(frame))
	replay_page._update_status(frame)
	_expect(replay_page._status_own._mana.text.contains("∞"), "replay displays recorded unlimited state")
	_expect(replay_page._status_opponent._temporary_life.text == "临时生命 +6", "replay displays opponent snapshot")
	frame = bridge.replay_frame(path, 0, 0).get("data", {})
	replay_page._update_status(frame)
	_expect(not replay_page._status_own._mana.text.contains("∞"), "rewinding replay clears later unlimited state")
	_expect(replay_page._status_opponent._temporary_life.text == "临时生命 +0", "rewinding replay clears later temporary life")
	_expect(not bool(bridge.debug_command({"operation": "gain_mana", "amount": 1}).get("ok", true)), "replay browsing rejects debug authority")
	replay_page._update_status(latest_frame)
	replay_page._update_logs(latest_frame)
	_expect(not replay_page._logs_text.get_parsed_text().is_empty(), "recorded event descriptions populate replay logs")
	bridge.release_session()
	_expect(bool(bridge.initialize(assets, user_dir).get("ok", false)), "bridge reinitialized")
	_expect(bool(bridge.start_match({"mode": "hotseat", "seed": 253}).get("ok", false)), "hotseat started")
	_expect(not bool(bridge.debug_command({"operation": "gain_mana", "amount": 1}).get("ok", true)), "hotseat rejects debug authority")
