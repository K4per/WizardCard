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
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var assets_index: int = args.find("--assets")
	var assets: String = args[assets_index + 1] if assets_index >= 0 else ProjectSettings.globalize_path("res://../assets/release-v260")
	var definitions: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(assets.path_join("cards.json")))
	for card: Dictionary in definitions.cards:
		GameText.shared().card_names[card.id] = card.name
	var page: MatchPage = load("res://pages/match_page.tscn").instantiate() as MatchPage
	# Godot's binary scene conversion used to add position layout to the match instance.
	# Exercise that serialized override before the shared panel enters the tree.
	page.get_node("%LessonEndPanel").set(&"layout_mode", 0)
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
	var lesson: Dictionary = {"lessonId": "lesson-04", "order": 4, "step": "look", "hintKey": "ui.lesson.badge", "params": {"order": 4, "mode": {"key": "tutorial.general.g02"}}, "highlights": [], "hintAnchor": "auto", "canContinue": true, "blockedHint": "tutorial.lesson.unavailable", "complete": false}
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1920, 1080), Vector2i(2560, 1440)]:
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
		await _check_installed_steps(page)
	page._lesson_end.present("教学模式 · 第 4 课")
	var first_panel: Control = page._lesson_end.find_child("Panel", true, false)
	check(first_panel.get_global_rect().get_center().distance_to(root.get_visible_rect().get_center()) < 2, "first terminal presentation is centered before container sorting")
	page._lesson_end.hide()
	await _check_authored_surfaces(page)
	page._refresh_tutorial({"tutorial": lesson})
	check(page._lesson_badge.text == "教学模式 · 第 4 课" and page._overlay.hint.text == page._lesson_badge.text, "snapshot order and localized hint parameters")
	check(page._overlay.hint.get_theme_font_size(&"font_size") >= 22, "instruction uses readable body type")
	check(page._overlay.advance.size.x >= 112 and page._overlay.advance.size.y >= 48, "Continue has a prominent click target")
	page._overlay._timer.wait_time = 0.08
	page._overlay.show_blocked()
	page._refresh_tutorial({"tutorial": lesson.duplicate(true)})
	await process_frame
	var warning: Label = page._overlay.bar.find_child("BlockedHint", true, false) as Label
	check(page._overlay.hint.text == "教学模式 · 第 4 课", "a blocked action retains the instruction")
	check(warning != null and warning.is_visible_in_tree() and warning.text == "无法加载所选教学课程。", "same-step refresh preserves the extra feedback")
	if warning != null:
		check(warning.get_global_rect().position.y >= page._overlay.hint.get_global_rect().end.y and page._overlay.bar.get_global_rect().encloses(warning.get_global_rect()), "feedback fits on its own line below the instruction")
	await page._overlay._timer.timeout
	check(warning == null or not warning.is_visible_in_tree(), "blocked feedback clears automatically")
	check(page._overlay.hint.text == "教学模式 · 第 4 课", "timeout keeps the current instruction")
	page._overlay.show_blocked()
	var next_step: Dictionary = lesson.duplicate(true)
	next_step.step = "next"
	next_step.hintKey = "tutorial.general.g02"
	next_step.params = {}
	page._refresh_tutorial({"tutorial": next_step})
	check((warning == null or not warning.is_visible_in_tree()) and page._overlay.hint.text == "教学模式", "advancing clears the old action feedback immediately")
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
		check(page._lesson_end.get_node("%Title").text == "这节课你会了。", "G03 is visible on completion")
		check(page._lesson_end.get_node("%Message").text == "教学模式 · 第 4 课", "terminal step text is preserved")
		check(page._lesson_end.get_node("%Next").text == "下一课" and page._lesson_end.get_node("%Menu").text == "回主菜单", "G03 uses course navigation at every battle order")
		page._lesson_end.get_node("%Next").pressed.emit()
	page._lesson_end.present("editor", true, true)
	page._lesson_end.get_node("%Next").pressed.emit()
	page._lesson_end.get_node("%Retry").pressed.emit()
	page._lesson_end.get_node("%Menu").pressed.emit()
	check(completions == ["lesson-04", "lesson-08"], "completed notifications are idempotent across refreshes")
	check(actions == [&"skip", &"next", &"next", &"deck", &"retry", &"menu"], "each end action requests its destination")
	var lost := _authored_step("lesson-07-chain", "lost")
	for repeat_index: int in range(2):
		page._refresh_tutorial({"tutorial": lost, "generation": "lost", "result": 1})
	check(page._lesson_end.visible and not page._overlay.visible and not page._lesson_end.get_node("%Next").visible and page._lesson_end.get_node("%Retry").has_focus(), "an unfinished terminal lesson focuses retry and suppresses next")
	check(completions.size() == 2 and page._lesson_end.get_node("%Title").text == "重来一次？", "G05 appears on loss")
	check(completions.size() == 2 and page._lesson_end.get_node("%Message").text == page._text.t("tutorial.l07.s07"), "L07 pass keeps its explanation on the loss panel")
	await _check_end_geometry(page._lesson_end)
	page._refresh_tutorial({})
	check(not page._overlay.visible and not page._lesson_badge.visible and not page._lesson_end.visible, "ordinary matches have no tutorial UI")
	page.queue_free()
	await process_frame
	print("Tutorial overlay failures: ", failures)
	quit(0 if failures == 0 else 1)

func _check_end_geometry(end: Control) -> void:
	var panel: PanelContainer = end.find_child("Panel", true, false) as PanelContainer
	var message: Label = end.get_node("%Message")
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1920, 1080), Vector2i(2560, 1440), Vector2i(1600, 900)]:
		root.size = dimensions
		await process_frame
		for variant: Array in [[true, false], [false, false], [true, true]]:
			end.hide()
			end.call(&"present", message.text, variant[0], variant[1])
			for frame: int in range(2):
				var bounds: Rect2 = panel.get_global_rect()
				var viewport: Rect2 = root.get_visible_rect()
				check(end.get_global_rect().is_equal_approx(viewport), "terminal shade covers the viewport at %s" % dimensions)
				check(bounds.get_center().distance_to(viewport.get_center()) < 2, "terminal panel is centered on frame %s at %s" % [frame, dimensions])
				check(viewport.encloses(bounds) and bounds.size.x >= dimensions.x * 0.4 and bounds.size.y >= dimensions.y * 0.30, "terminal content is large and in view at %s" % dimensions)
				await process_frame
			var primary: Button = end.get_node("%Next") if variant[0] else end.get_node("%Retry")
			check(primary.has_focus() and primary.size.y >= 48, "terminal action is prominent and focused")
			check(panel.get_global_rect().encloses(message.get_global_rect()), "terminal message fits the panel")

func _authored_step(file: String, id: String) -> Dictionary:
	var lesson: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://data/tutorial/lessons/" + file + ".json"))
	for step: Dictionary in lesson.steps:
		if step.id == id:
			return {"lessonId": lesson.id, "order": lesson.order, "step": id, "hintKey": step.hint,
				"hintSurface": step.get("hintSurface", "bar"), "highlights": step.get("highlight", []),
				"actionHints": step.get("actionHints", {}), "recapHint": step.get("recapHint", ""), "canContinue": step.get("continue", false)}
	return {}

func _check_authored_surfaces(page: MatchPage) -> void:
	var catalog: Node = root.get_node("Text")
	var service := QueryFixtureService.new()
	service.match_mode = "tutorial"
	service.match_viewer = 0
	service.match_snapshot = {"generation": "1", "revision": "1", "viewer": 0, "result": -1, "cards": []}
	page.service = service
	page._selection.service = service
	page._selection.changed.connect(page._query_changed)
	var field := FieldPresentation.new()
	for def: String in ["v260-analytic-0006", "v260-word-0001", "v260-talisman-0003"]:
		var card := CardPresentation.from_definition({"id": def, "name": def, "illustrationId": "aid"})
		card.instance_id = str(field.hand.size() + 1)
		field.hand.append(card)
	page.canvas.configure(field)
	var detail: Label = page._detail.get_node("%LessonHint")
	var lessons := {"lesson-01-first-match": ["rank", "cost", "type", "effect"], "lesson-04-spells": ["analytic", "word", "talisman"]}
	for file: String in lessons:
		for id: String in lessons[file]:
			var step := _authored_step(file, id)
			page._refresh_tutorial({"tutorial": step})
			check(detail.is_visible_in_tree() and detail.text == catalog.t(step.hintKey), file + "/" + id + " appears in card details")
			check(page._detail.card != null and page._detail.card.definition_id == str(step.highlights[0]).trim_prefix("card:hand."), "detail opens for the highlighted card")
	page._hover_card(field.hand[0])
	check(not detail.visible, "a different card hides the previous card's explanation")
	page._hover_card(field.hand[2])
	check(detail.is_visible_in_tree(), "returning to the lesson card restores its explanation")
	var choice := _authored_step("lesson-02-set-ambush", "conceal-or-set")
	page._refresh_tutorial({"tutorial": choice})
	check(not detail.visible, "leaving a detail step clears its explanation")
	service.set_actions([
		{"source": "1", "command": {"type": KernelContract.CommandKind.StartAnalysis, "card": "1", "formation": "4"}},
		{"source": "1", "command": {"type": KernelContract.CommandKind.SetAmbush, "card": "1", "formation": "4"}}])
	page._show_action_list("1", "", true)
	for operation: String in ["StartAnalysis", "SetAmbush"]:
		var button := page._action_list.get_node_or_null(operation) as Button
		var key: String = "tutorial.l02.s02" if operation == "StartAnalysis" else "tutorial.l02.s02.ambush"
		check(button != null and button.is_visible_in_tree() and button.text == catalog.t(key), "L02-02 labels " + operation)
	page._selection.cancel()
	page.request_leave()
	check(page._leave_dialog.visible and page._leave_dialog.dialog_text == catalog.t("tutorial.general.g04"), "G04 appears on leaving an unfinished lesson")
	check(page._leave_dialog.get_cancel_button().text == catalog.t("tutorial.general.g04.continue") and page._leave_dialog.get_ok_button().text == catalog.t("tutorial.general.g04.exit"), "G04 choices")
	page._leave_dialog.hide()
	var response := _authored_step("lesson-07-chain", "respond")
	service.match_snapshot["tutorial"] = response
	service.match_snapshot["decision"] = {"id": "9", "kind": KernelContract.DecisionKind.Response, "player": 0}
	service.match_snapshot["actionSummary"] = {"hasNonPhaseAction": true, "safeAdvance": {"type": KernelContract.CommandKind.PassResponse, "decision": "9"}}
	page._refresh_tutorial(service.match_snapshot)
	page._response_mode_changed(ResponseTiming.Mode.IGNORE)
	await create_timer(0.2).timeout
	check(page._confirm.is_visible_in_tree() and page._confirm.get_node("%Heading").text == catalog.t("tutorial.l07.s03"), "L07-03 asks in the actual response dialog, including Ignore mode")
	check(service.selected_actions.is_empty(), "the learner retains the response decision")
	page._confirm.close_popup()
	var won := _authored_step("lesson-04-spells", "won")
	won.complete = true
	page._refresh_tutorial({"tutorial": won, "generation": "won", "result": 0})
	var message: Label = page._lesson_end.get_node("%Message")
	check(message.is_visible_in_tree() and message.text == catalog.t("tutorial.l04.s06") + "\n" + catalog.t("tutorial.l04.s07"), "L04 victory includes the recap after resolving")
	completions.clear()
	service.match_snapshot = {}
	page._refresh_tutorial({})

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
			if step.has("blockedHint"):
				page._overlay.show_blocked(step.blockedHint)
			await process_frame
			_check_bar(page, file + "/" + str(step.id))
			if step.has("phaseHint"):
				projected = projected.duplicate(true)
				projected.hintKey = "tutorial.lesson.phase_instruction"
				projected.params = {"note": {"key": step.hint, "args": step.get("params", {})}, "instruction": {"key": step.phaseHint}}
				projected.highlights.append("ui:end_phase")
				page._overlay.configure(projected, _area_envelope.bind(page), [])
				page._overlay.show_blocked()
				await process_frame
				_check_bar(page, file + "/" + str(step.id) + " with phase instruction and blocked feedback")

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
