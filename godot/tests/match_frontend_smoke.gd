extends SceneTree

var app: WizardFrontend
var _capture: bool = false
var _output: String = ""
var _response_result: Array[String] = []

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
	root.size = Vector2i(1280, 720)
	await process_frame
	await _capture_page("hotseat-privacy")
	if match_page._privacy_layer.visible and match_page._pending_viewer >= 0:
		match_page._reveal()
		await process_frame
	if not _expect(not app.service.match_snapshot.is_empty(), "match has a viewer projection"):
		return
	if not _expect(match_page.canvas.field.own_deck_count > 0 and match_page.canvas.field.opponent_deck_count > 0, "authoritative deck counts reach battlefield"):
		return
	var side_rows: Array = app.service.match_snapshot.get("cards", []).filter(func(row: Dictionary) -> bool:
		return int(row.get("zone", -1)) == 9 and int(row.get("owner", -1)) == app.service.match_viewer)
	if not _expect(side_rows.size() == match_page.canvas.field.own_side_count, "own side deck cards are projected for inspection"):
		return
	await process_frame
	if not _expect(match_page.canvas.zone_at(match_page.canvas._zone_hits[0]["rect"].get_center()).has("kind"), "deck and ash regions expose click targets"):
		return
	var chosen_card: CardPresentation = match_page.canvas.field.hand[0]
	match_page._select_card(chosen_card)
	await process_frame
	var visible_rect: Rect2 = Rect2(Vector2.ZERO, root.size)
	if not _expect(match_page._detail.visible and match_page._detail.card.instance_id == chosen_card.instance_id, "selecting a card opens its detail pane immediately"):
		return
	if not _expect(visible_rect.encloses(match_page._detail.get_global_rect()), "selected card detail remains inside the viewport"):
		return
	if not _expect(match_page._action_popup.visible and match_page._action_popup.get_global_rect().position.x < match_page._detail.get_global_rect().position.x, "selected card operations stay beside the selected card and outside the detail pane"):
		return
	var second_card: CardPresentation = match_page.canvas.field.hand[1]
	var escape: InputEventKey = InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	root.push_input(escape, true)
	await process_frame
	if not _expect(match_page.canvas.selected_id.is_empty() and not match_page._detail.visible and not match_page._action_popup.visible, "Escape clears the selected card context"):
		return
	var second_point: Vector2 = match_page.canvas.global_position + match_page.canvas.card_bounds(second_card).get_center()
	_mouse_button(second_point, true)
	_mouse_button(second_point, false)
	await process_frame
	if not _expect(match_page.canvas.selected_id == second_card.instance_id and match_page._detail.visible and match_page._detail.card.instance_id == second_card.instance_id, "clicking another card after Escape restores card context"):
		return
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		await process_frame
		await process_frame
		var viewport_rect: Rect2 = Rect2(Vector2.ZERO, dimensions)
		if not _expect(viewport_rect.encloses(match_page._detail.get_global_rect()), "selected card detail remains inside viewport at %dx%d" % [dimensions.x, dimensions.y]):
			return
	match_page._clear_card()
	match_page._zone_clicked("side_deck", app.service.match_viewer)
	await process_frame
	if not _expect(match_page._collection.visible and match_page._collection_grid.get_child_count() == side_rows.size(), "clicking own side deck opens its actual card list"):
		return
	match_page._close_collection()
	match_page._zone_clicked("ash", app.service.match_viewer)
	if not _expect(match_page._collection.visible and match_page._collection_title.text.contains("灰烬区"), "clicking ash opens the ash card viewer: visible=%s title=%s" % [match_page._collection.visible, match_page._collection_title.text]):
		return
	match_page._close_collection()
	var response_panel: DecisionPanel = DecisionPanel.new()
	match_page.add_child(response_panel)
	var response: DecisionPresentation = DecisionPresentation.new()
	response.generation = "10"
	response.revision = "11"
	response.decision_id = "12"
	response.title = "响应窗口 · 玩家 1"
	response.description = "选择响应卡，或不响应。"
	response.response_mode = true
	var response_choice: DecisionChoice = DecisionChoice.new()
	response_choice.id = "9007199254740993"
	response_choice.label = "测试响应"
	response_choice.card = chosen_card.duplicate() as CardPresentation
	response_choice.card.illustration_id = "balance"
	response.choices.append(response_choice)
	response_panel.configure(response)
	_response_result.clear()
	response_panel.confirmed.connect(func(_generation: String, _revision: String, _decision_id: String, selected_ids: Array) -> void:
		_response_result.assign(selected_ids))
	response_panel._on_response_choice(response_choice.id, response.generation, response.revision, response.decision_id)
	if not _expect(response_panel._confirm == null and _response_result == [response_choice.id], "compact response card directly submits the selected legal action"):
		return
	response_panel.queue_free()
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
	var saved_path: String = app.service.last_replay_path.replace("\\", "/").simplify_path()
	var listed_saved: bool = false
	if replay != null:
		for path: String in replay._paths:
			if path.replace("\\", "/").simplify_path() == saved_path:
				listed_saved = true
				break
	if not _expect(listed_saved, "saved replay is listed"):
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

func _mouse_button(point: Vector2, pressed: bool, button: MouseButton = MOUSE_BUTTON_LEFT) -> void:
	var event: InputEventMouseButton = InputEventMouseButton.new()
	event.position = point
	event.global_position = point
	event.button_index = button
	event.pressed = pressed
	root.push_input(event, true)

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
