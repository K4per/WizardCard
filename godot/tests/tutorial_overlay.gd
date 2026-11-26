extends SceneTree

var failures: int = 0
var completions: Array[String] = []
var actions: Array[StringName] = []
var continued: bool = false

func _initialize() -> void:
	_run.call_deferred()

func check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error("TUTORIAL_OVERLAY: " + message)

func _run() -> void:
	var page: MatchPage = load("res://pages/match_page.tscn").instantiate() as MatchPage
	root.add_child(page)
	page.lesson_completed.connect(func(id: String, _order: int) -> void: completions.append(id))
	page.lesson_navigation_requested.connect(func(action: StringName, _id: String, _order: int) -> void: actions.append(action))
	var field: FieldPresentation = FieldPresentation.new()
	for index: int in range(2):
		var card: CardPresentation = CardPresentation.new()
		card.instance_id = str(index + 1)
		card.definition_id = "v260-analytic-0004"
		card.illustration_id = "aid"
		field.hand.append(card)
		field.card_zones[card.instance_id] = 1
	field.opponent_hand_count = 3
	page.canvas.configure(field)
	var lesson: Dictionary = {"lessonId": "lesson-04", "order": 4, "step": "look", "hintKey": "ui.lesson.badge", "params": {"order": 4}, "highlights": [], "hintAnchor": "auto", "canContinue": true, "blockedHint": "tutorial.lesson.unavailable", "complete": false}
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1920, 1080)]:
		root.size = dimensions
		await process_frame
		await process_frame
		for step: Array in [["zone:learner.status"], ["zone:opponent.hand"], ["zone:learner.hand", "zone:opponent.hand"], ["zone:learner.formation", "zone:opponent.casting"], ["zone:learner.deck", "zone:opponent.ash", "zone:learner.side_deck"], ["card:hand.v260-analytic-0004", "ui:EndPhase", "ui:phase"]]:
			var input: Dictionary = lesson.duplicate(true)
			input.highlights = step
			page._refresh_tutorial({"tutorial": input, "generation": "1"})
			await process_frame
			for id: String in step:
				check(not page.tutorial_target_rects(id).is_empty(), "%s resolves at %s" % [id, dimensions])
			_check_bar(page, str(step))
		check(page.tutorial_target_rects("card:hand.v260-analytic-0004").size() == 2, "both visible instances of the same definition resolve")
		check(page.tutorial_target_rects("card:casting.v260-analytic-0004").is_empty(), "card in a different zone never resolves")
		# Author-selected anchor still yields to the occupied hand or a response popup.
		var anchored: Dictionary = lesson.duplicate(true)
		anchored.hintAnchor = "bottom"
		page._refresh_tutorial({"tutorial": anchored})
		var bottom: Rect2 = page._overlay.bar.get_global_rect()
		check(bottom.get_center().y > dimensions.y * 0.75, "explicit bottom anchor")
		page._confirm.popup_response("response:test", "响应", "查看", "放弃")
		check(page._overlay.highlights.has(page.tutorial_target_rects("zone:opponent.casting")[0]), "every tutorial response highlights the opponent casting zone")
		check(not anchored.highlights.has("zone:opponent.casting"), "response geometry never changes the lesson snapshot")
		page._confirm.size = Vector2(440, 150)
		page._confirm.position = bottom.get_center() - page._confirm.size * 0.5
		await process_frame
		await process_frame
		check(not page._overlay.bar.get_global_rect().intersects(page._confirm.get_global_rect()), "hint avoids a moved confirmation panel")
		page._confirm.close_popup()
		await process_frame
		check(page._overlay.highlights.is_empty(), "closing the question restores the course targets")
		_check_installed_steps(page)
	page._refresh_tutorial({"tutorial": lesson})
	check(page._lesson_badge.text == "教学模式 · 第 4 课" and page._overlay.hint.text == page._lesson_badge.text, "snapshot order and localized hint parameters")
	page._overlay.show_blocked()
	check(page._overlay.hint.text == "无法加载所选教学课程。", "a blocked action uses the current step's text")
	page._overlay._timer.timeout.emit()
	check(page._overlay.hint.text == "教学模式 · 第 4 课", "blocked feedback restores the current step")
	page._overlay.continue_requested.disconnect(page._continue_tutorial)
	page._overlay.continue_requested.connect(func() -> void: continued = true)
	page._overlay.advance.pressed.emit()
	check(continued, "visible Continue forwards the request")
	var pointer_step: Dictionary = lesson.duplicate(true)
	pointer_step.highlights = ["ui:hand_toggle"]
	page._refresh_tutorial({"tutorial": pointer_step})
	var mouse: InputEventMouseButton = InputEventMouseButton.new()
	mouse.position = page._hand_button.get_global_rect().get_center()
	mouse.button_index = MOUSE_BUTTON_LEFT
	mouse.pressed = true
	root.push_input(mouse, true)
	mouse = mouse.duplicate() as InputEventMouseButton
	mouse.pressed = false
	root.push_input(mouse, true)
	check(page.canvas.hand_collapsed, "highlighted button receives a real click through the overlay")
	page._open_match_menu()
	check(not page._overlay.visible, "lesson controls yield to the match menu")
	page.get_node("%SkipLesson").pressed.emit()
	check(actions == [&"skip"], "skip is explicit and distinct from leaving")
	page._close_match_menu()
	var complete: Dictionary = lesson.duplicate(true)
	complete.complete = true
	for order: int in [4, 8]:
		complete.order = order
		complete.lessonId = "lesson-%02d" % order
		for repeat_index: int in range(2):
			page._refresh_tutorial({"tutorial": complete.duplicate(true), "generation": str(order)})
		check(page._lesson_end.visible and not page._overlay.visible, "lesson completion replaces the hint")
		check(page._lesson_end.get_node("%Message").text == "教学模式 · 第 4 课", "terminal step text is preserved")
		page._lesson_end.get_node("%Next").pressed.emit()
	page._lesson_end.get_node("%Retry").pressed.emit()
	page._lesson_end.get_node("%Menu").pressed.emit()
	check(completions == ["lesson-04", "lesson-08"], "completed notifications are idempotent across refreshes")
	check(actions == [&"skip", &"next", &"deck", &"retry", &"menu"], "each end action requests its destination")
	for repeat_index: int in range(2):
		page._refresh_tutorial({"tutorial": lesson, "generation": "lost", "result": 1})
	check(page._lesson_end.visible and not page._overlay.visible and not page._lesson_end.get_node("%Next").visible and page._lesson_end.get_node("%Retry").has_focus(), "an unfinished terminal lesson focuses retry and suppresses next")
	check(completions.size() == 2 and page._lesson_end.get_node("%Message").text.contains("重新练习"), "an unfinished match shows retry feedback without recording completion")
	page._refresh_tutorial({})
	check(not page._overlay.visible and not page._lesson_badge.visible and not page._lesson_end.visible, "ordinary matches have no tutorial UI")
	page.queue_free()
	await process_frame
	print("Tutorial overlay failures: ", failures)
	quit(0 if failures == 0 else 1)

func _check_bar(page: MatchPage, label: String) -> void:
	check(page._overlay.bar.is_visible_in_tree(), "hint remains visible for " + label)
	var bounds: Rect2 = page._overlay.bar.get_global_rect()
	for target: Rect2 in page._overlay.highlights:
		check(not bounds.intersects(target), "hint overlaps a target in " + label)

func _check_installed_steps(page: MatchPage) -> void:
	# The catalog is added in the content stage. Read every installed match
	# lesson so future additions automatically exercise their actual hint text.
	var directory: DirAccess = DirAccess.open("res://data/tutorial/lessons")
	if directory == null:
		return
	for file: String in directory.get_files():
		if file.get_extension() != "json":
			continue
		var input: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(directory.get_current_dir().path_join(file)))
		if not input.has("scenario"):
			continue
		for step: Dictionary in input.steps:
			var projected: Dictionary = {"hintKey": step.hint, "params": step.get("params", {}), "highlights": step.get("highlight", []), "hintAnchor": step.get("hintAnchor", "auto"), "canContinue": step.get("continue", false)}
			page._overlay.configure(projected, _area_envelope.bind(page), [])
			_check_bar(page, file + "/" + str(step.id))

func _area_envelope(id: String, page: MatchPage) -> Array[Rect2]:
	var rects: Array[Rect2] = page.tutorial_target_rects(id)
	if not rects.is_empty() or not id.begins_with("card:"):
		return rects
	# Outside a running script, cards may not yet be in this fixture's field.
	# Cover the whole corresponding visible zone on both sides as a stricter
	# collision check than any particular arrangement of those cards.
	var zone: String = id.trim_prefix("card:").get_slice(".", 0)
	for owner: String in ["learner", "opponent"]:
		rects.append_array(page.tutorial_target_rects("zone:" + owner + "." + zone))
	return rects
