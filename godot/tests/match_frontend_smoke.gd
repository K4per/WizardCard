extends SceneTree

var app: WizardFrontend
var _capture: bool = false
var _output: String = ""

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> bool:
	if not condition:
		push_error("MATCH_FRONTEND: " + message)
		quit(2)
	return condition

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	_capture = arguments.has("--capture")
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--output":
			_output = arguments[index + 1]
	if not _output.is_empty():
		DirAccess.make_dir_recursive_absolute(_output)
	app = (load("res://app/main.tscn") as PackedScene).instantiate() as WizardFrontend
	root.add_child(app)
	await process_frame
	if not _expect(app.service.error.is_empty(), "content and bridge initialized: " + app.service.error):
		return
	app.navigate(&"setup")
	await process_frame
	var setup: SetupPage = app.page as SetupPage
	if not _expect(setup != null and not setup.service.playable.is_empty(), "setup lists playable decks"):
		return
	await _capture_page("setup")
	setup._mode.select(1)
	setup._mode_changed(1)
	setup.start()
	await process_frame
	if not _expect(app.page is MatchPage, "hotseat setup opens the real match page: " + app.service.error):
		return
	var match_page: MatchPage = app.page as MatchPage
	await _capture_page("hotseat-privacy")
	if match_page._privacy_layer.visible and match_page._pending_viewer >= 0:
		match_page._reveal()
		await process_frame
	if not _expect(not app.service.match_snapshot.is_empty(), "match has a viewer projection"):
		return
	if not _expect(match_page.canvas.field.own_deck_count > 0 and match_page.canvas.field.opponent_deck_count > 0, "authoritative deck counts reach battlefield"):
		return
	if not _expect(match_page._zone_picker.item_count >= 10, "own side deck is inspectable"):
		return
	await _capture_page("match")
	var original_revision: String = str(app.service.match_snapshot.get("revision", ""))
	var actions: Array = app.service.match_snapshot.get("actions", [])
	if not _expect(not actions.is_empty(), "viewer has legal actions"):
		return
	var first: Dictionary = actions[0]
	var generation: String = str(app.service.match_snapshot.get("generation", ""))
	var selected: Dictionary = app.service.match_request(&"select_action", [app.service.match_viewer, str(first["id"]), generation, original_revision])
	if not _expect(bool(selected.get("ok", false)), "select current legal action"):
		return
	match_page._apply_result(app.service.match_request(&"confirm_action", [generation, original_revision]))
	await process_frame
	if match_page._privacy_layer.visible and match_page._pending_viewer >= 0:
		match_page._reveal()
		await process_frame
	if not _expect(str(app.service.match_snapshot.get("revision", "")) != original_revision, "confirmation advances authoritative revision"):
		return
	if not _expect(app.service.save_replay(), "save replay: " + app.service.error):
		return
	# A local test opens the partial recording directly; the live match page
	# does not expose opponent private views until the match ends.
	app.navigate(&"replay")
	await process_frame
	var replay: ReplayPage = app.page as ReplayPage
	if not _expect(replay != null and replay._paths.has(app.service.last_replay_path), "saved replay is listed"):
		return
	if not _expect(replay._notice.text.begins_with("验证通过"), "replay verifies before display: " + replay._notice.text):
		return
	replay._move(1)
	await process_frame
	if not _expect(replay._step == 1 and replay._previous.disabled == false and not replay._canvas.field.own_formations.is_empty(), "replay displays recomputed step and navigation"):
		return
	if not _expect(str(app.service.match_snapshot.get("revision", "")) == str(app.service.snapshot().get("revision", "")), "replay leaves live match unchanged"):
		return
	await _capture_page("replay")
	if not _expect(app.service.leave_match(), "leave hotseat: " + app.service.error):
		return
	app.navigate(&"setup")
	await process_frame
	setup = app.page as SetupPage
	setup._mode.select(0)
	setup._mode_changed(0)
	setup.start()
	await process_frame
	if not _expect(app.page is MatchPage and app.service.match_mode == "ai", "AI setup opens match"):
		return
	if not _expect(app.service.snapshot(1).is_empty(), "AI opponent projection stays private"):
		return
	app.service.match_viewer = 0
	if not _expect(app.service.save_replay() and app.service.leave_match(), "AI match leaves with saved replay"):
		return
	app.navigate(&"setup")
	await process_frame
	setup = app.page as SetupPage
	setup._mode.select(2)
	setup._mode_changed(2)
	setup.start()
	await process_frame
	if not _expect(app.page is MatchPage and app.service.match_mode == "tutorial", "tutorial starts from setup: " + app.service.error):
		return
	match_page = app.page as MatchPage
	if not _expect(match_page._tutorial.visible and not match_page._tutorial.text.is_empty(), "native tutorial guidance is visible"):
		return
	await _capture_page("tutorial")
	if not _expect(app.service.leave_match(), "tutorial leaves safely"):
		return
	app.navigate(&"rooms")
	await process_frame
	await _capture_page("rooms")
	print("MATCH_FRONTEND ok · setup / hotseat / authority / replay / AI privacy / tutorial / rooms")
	quit(0)

func _capture_page(label: String) -> void:
	if not _capture or _output.is_empty():
		return
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		await process_frame
		await process_frame
		await RenderingServer.frame_post_draw
		var capture: Image = root.get_texture().get_image()
		if not _expect(capture.save_png(_output.path_join(label + "-%dx%d.png" % [dimensions.x, dimensions.y])) == OK, "save screenshot"):
			return
