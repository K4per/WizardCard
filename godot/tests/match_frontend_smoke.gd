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
	seed(42)
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
	while bool(app.service.match_snapshot.get("opening", false)):
		if not _expect(match_page._decision.visible and match_page._decision.request.decision_id == "opening", "opening shows private formation selection"):
			return
		var opening_request: DecisionPresentation = match_page._decision.request
		if not _expect(not opening_request.choices.is_empty(), "opening exposes own level-one candidates"):
			return
		var opening_option: Button = match_page._decision._options[opening_request.choices[0].id]
		if not _expect(opening_option != null, "opening candidate is rendered as a real button"):
			return
		var opening_point: Vector2 = opening_option.get_global_rect().get_center()
		_mouse_button(opening_point, true)
		_mouse_button(opening_point, false)
		await process_frame
		if not _expect(match_page._decision._selected == [opening_request.choices[0].id], "real mouse click selects the opening formation"):
			return
		if not _expect(not match_page._decision._confirm.disabled, "opening confirmation enables after selecting one formation"):
			return
		var opening_confirm_point: Vector2 = match_page._decision._confirm.get_global_rect().get_center()
		_mouse_button(opening_confirm_point, true)
		_mouse_button(opening_confirm_point, false)
		await process_frame
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
	# Finish any phase-start response window before exercising card contexts.
	for response_step: int in range(40):
		if not match_page._decision.visible:
			if app.service.match_snapshot.get("automaticAction") is String or match_page._auto_busy:
				await create_timer(0.06).timeout
				if match_page._privacy_layer.visible and match_page._pending_viewer >= 0:
					match_page._reveal()
					await process_frame
				continue
			break
		var response_actions: Array = app.service.match_snapshot.get("actions", [])
		var response_snapshot: Dictionary = app.service.match_snapshot
		var pass_action: Dictionary = response_actions[0]
		for candidate: Dictionary in response_actions:
			if int((candidate.get("command", {}) as Dictionary).get("type", -1)) == 13:
				pass_action = candidate
		var decision_id: String = str((response_snapshot.get("decision", {}) as Dictionary).get("id", ""))
		match_page._decision_confirmed(str(response_snapshot["generation"]), str(response_snapshot["revision"]), decision_id, [str(pass_action["id"])])
		await process_frame
		if match_page._privacy_layer.visible and match_page._pending_viewer >= 0:
			match_page._reveal()
			await process_frame
	if not _expect(not match_page._auto_busy and not match_page._decision.visible, "phase-start automatic actions settle before card contexts"):
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
	var second_motion: InputEventMouseMotion = InputEventMouseMotion.new()
	second_motion.position = second_point
	second_motion.global_position = second_point
	root.push_input(second_motion, true)
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
	var menu_escape: InputEventKey = InputEventKey.new()
	menu_escape.keycode = KEY_ESCAPE
	menu_escape.pressed = true
	root.push_input(menu_escape, true)
	await process_frame
	if not _expect(match_page._match_menu.visible and match_page._menu_shade.visible and not match_page._leave_dialog.visible, "Escape opens the match menu when no card context is selected"):
		return
	await _capture_page("match-menu")
	root.push_input(menu_escape, true)
	await process_frame
	if not _expect(not match_page._match_menu.visible and not match_page._menu_shade.visible, "Escape closes the match menu without leaving the match"):
		return
	if not _expect(match_page.canvas.get_global_rect().size.is_equal_approx(Vector2(root.size)), "battlefield fills the complete viewport"):
		return
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
	if not _expect(not response_panel._confirm.visible and _response_result == [response_choice.id], "compact response card directly submits the selected legal action"):
		return
	response_panel.queue_free()
	await process_frame
	if not await _exercise_card_input(match_page):
		return
	if not await _exercise_second_player_side_deck(match_page):
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
	if not _expect(replay._step == 1 and replay._previous.disabled == false and replay._canvas.field.opening, "replay displays the private first opening choice and navigation"):
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

func _exercise_card_input(match_page: MatchPage) -> bool:
	# Reach a card action through the native phase gate; only setup uses the
	# service directly. The regression itself clicks the real viewport controls.
	var playable: CardPresentation
	for step: int in range(16):
		for card: CardPresentation in match_page.canvas.field.hand:
			for action: Dictionary in app.service.match_snapshot.get("actions", []):
				if str(action.get("source", "0")) == card.instance_id:
					playable = card
		if playable != null:
			break
		var actions: Array = app.service.match_snapshot.get("actions", [])
		if not _expect(not actions.is_empty(), "native setup has a phase or response action"):
			return false
		var snapshot: Dictionary = app.service.match_snapshot
		var generation: String = str(snapshot["generation"])
		var revision: String = str(snapshot["revision"])
		var selected: Dictionary = app.service.match_request(&"select_action", [app.service.match_viewer, str(actions[0]["id"]), generation, revision])
		if not _expect(bool(selected.get("ok", false)), "native phase setup accepts current action"):
			return false
		match_page._apply_result(app.service.match_request(&"confirm_action", [generation, revision]))
		await process_frame
		if match_page._privacy_layer.visible and match_page._pending_viewer >= 0:
			match_page._reveal()
			await process_frame
	if not _expect(playable != null, "fixed seed reaches a playable hand card"):
		return false
	match_page._clear_card()
	await process_frame
	await process_frame
	var point: Vector2 = match_page.canvas.global_position + match_page.canvas.card_bounds(playable).get_center()
	_mouse_button(point, true)
	_mouse_button(point, false)
	await process_frame
	await process_frame
	if not _expect(match_page._selected != null and match_page._selected.instance_id == playable.instance_id, "real mouse click selects the playable card"):
		return false
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		await process_frame
		await process_frame
		var original_position: Vector2 = match_page._action_popup.position
		for repetition: int in range(3):
			var escape: InputEventKey = InputEventKey.new()
			escape.keycode = KEY_ESCAPE
			escape.pressed = true
			root.push_input(escape, true)
			await process_frame
			await process_frame
			if not _expect(match_page._selected == null and not match_page._action_popup.visible, "Escape clears card actions before reselection"):
				return false
			point = match_page.canvas.global_position + match_page.canvas.card_bounds(playable).get_center()
			_mouse_button(point, true)
			_mouse_button(point, false)
			await process_frame
			await process_frame
			if not _expect(match_page._selected == playable and match_page._action_popup.position.is_equal_approx(original_position), "Escape and reselection preserve action placement at %s" % dimensions):
				return false
		if not _expect(Rect2(Vector2.ZERO, dimensions).encloses(match_page._action_popup.get_global_rect()), "card actions stay inside the resized viewport"):
			return false
	# A selected hand card rises after the input callback. Its actions must follow
	# that fresh geometry instead of remaining attached to its lowered position.
	var selected_bounds: Rect2 = match_page.canvas.card_bounds(playable)
	var selected_top: float = match_page.canvas.global_position.y + selected_bounds.position.y
	if not _expect(absf(match_page._action_popup.get_global_rect().end.y + 8.0 - selected_top) < 1.0, "card actions follow the raised hand card after redraw"):
		return false
	await _capture_page("card-actions")
	var action_button: Button = match_page._action_list.get_child(0) as Button
	if not _expect(action_button != null and not action_button.disabled, "selected playable card exposes an enabled action button"):
		return false
	var action_scroll: ScrollContainer = match_page._action_list.get_parent() as ScrollContainer
	for button: Control in match_page._action_list.get_children():
		if not _expect(action_scroll.get_global_rect().encloses(button.get_global_rect()), "short action list shows each complete action button"):
			return false
	point = action_button.get_global_rect().get_center()
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = point
	motion.global_position = point
	root.push_input(motion, true)
	if not _expect(root.gui_get_hovered_control() == action_button, "card action receives mouse input above the detail shade"):
		return false
	var original_revision: String = str(app.service.match_snapshot["revision"])
	_mouse_button(point, true)
	_mouse_button(point, false)
	await process_frame
	for target_step: int in range(10):
		if match_page._target_actions.is_empty() or str(app.service.match_snapshot["revision"]) != original_revision:
			break
		if not _expect(not match_page._confirm.visible and not match_page.canvas.target_ids.is_empty(), "placement exposes board candidates without an extra confirmation"):
			return false
		await process_frame
		var target_point: Vector2 = Vector2.ZERO
		for hit: Dictionary in match_page.canvas._card_hits:
			var target: CardPresentation = hit["card"]
			if match_page.canvas.target_ids.has(target.instance_id):
				target_point = match_page.canvas.global_position + (hit["rect"] as Rect2).get_center()
				break
		if not _expect(target_point != Vector2.ZERO, "an authoritative target is clickable on the battlefield"):
			return false
		_mouse_button(target_point, true)
		_mouse_button(target_point, false)
		await process_frame
	if match_page._confirm.visible:
		if not _expect(not match_page._pending.is_empty(), "single action retains a pending command for confirmation"):
			return false
		var confirm_button: Button = match_page._confirm.get_node("Column/Actions/Buttons/Confirm") as Button
		point = confirm_button.get_global_rect().get_center()
		_mouse_button(point, true)
		_mouse_button(point, false)
		await process_frame
	if not _expect(str(app.service.snapshot()["revision"]) != original_revision, "mouse operation and target selection execute the card in the native session"):
		return false
	if match_page._privacy_layer.visible and match_page._pending_viewer >= 0:
		match_page._reveal()
		await process_frame
	return true

func _exercise_second_player_side_deck(match_page: MatchPage) -> bool:
	# Show player two's projection while checking the field's own/opponent zones.
	var projection: Dictionary = app.service.snapshot(1)
	app.service.match_viewer = 1
	match_page.canvas.configure(MatchProjection.field(projection))
	match_page._privacy_layer.hide()
	match_page._decision.hide()
	match_page._shade.hide()
	await process_frame
	await process_frame
	var point: Vector2 = Vector2.ZERO
	for hit: Dictionary in match_page.canvas._zone_hits:
		if str(hit.get("kind", "")) == "side_deck" and int(hit.get("owner", -1)) == 1:
			point = match_page.canvas.global_position + (hit["rect"] as Rect2).get_center()
			break
	var zone: Dictionary = match_page.canvas.zone_at(point - match_page.canvas.global_position)
	if not _expect(int(zone.get("owner", -1)) == 1, "player two's own side-deck input belongs to player two"):
		return false
	_mouse_button(point, true)
	_mouse_button(point, false)
	await process_frame
	if not _expect(match_page._collection.visible and match_page._collection_title.text.begins_with("我方"), "player two can click and inspect their own side deck"):
		return false
	match_page._close_collection()
	# Return to the actual acting viewer before the remaining match/replay checks.
	match_page._cover(int(projection["actingPlayer"]))
	match_page._reveal()
	await process_frame
	return true

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
