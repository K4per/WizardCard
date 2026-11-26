extends SceneTree

const HandInput = preload("res://tests/hand_input.gd")

var failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func check(value: bool, message: String) -> void:
	if not value:
		failures += 1
		push_error("V263_GUIDE: " + message)

func click_board(canvas: FieldCanvas, local: Vector2) -> void:
	var mouse := InputEventMouseButton.new()
	mouse.position = canvas.get_global_transform_with_canvas() * local
	mouse.button_index = MOUSE_BUTTON_LEFT
	mouse.pressed = true
	root.push_input(mouse, true)
	mouse = mouse.duplicate() as InputEventMouseButton
	mouse.pressed = false
	root.push_input(mouse, true)

func _run() -> void:
	var app: WizardFrontend = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(app)
	await process_frame
	check(app.service.error.is_empty(), "current content loads")
	if OS.has_feature("windows"):
		check(ProjectSettings.globalize_path("user://").replace("\\", "/").trim_suffix("/").ends_with("/Godot/app_userdata/WizardCard Alpha v2.6.3"), "Windows keeps its previous player data directory")
	var previous_directory: String = app.service.user_directory.path_join("previous-release-guide-test")
	DirAccess.make_dir_recursive_absolute(previous_directory)
	var previous_path: String = previous_directory.path_join("guide-progress.json")
	var previous: FileAccess = FileAccess.open(previous_path, FileAccess.WRITE)
	previous.store_string(JSON.stringify({"version": "2.6.4", "completed": [4]}))
	previous.close()
	var upgrade := GuideProgress.read(previous_directory)
	check(upgrade.askedIntro and upgrade.isVeteran and upgrade.completed.is_empty(), "legacy progress migrates without mapping integer chapters")
	check(GuideProgress.mark(previous_directory, "example-lesson"), "can save a new course after upgrading")
	var migrated: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(previous_path)) as Dictionary
	check(migrated.get("version") == "2.6.6" and migrated.schemaVersion == 2 and GuideProgress.completed(previous_directory) == ["example-lesson"], "new course IDs survive reload")
	check((app.page as MenuPage)._buttons[3].text.begins_with("巫师指南"), "menu has the unified guide entry")
	app.navigate(&"setup")
	await process_frame
	check((app.page as SetupPage)._mode.item_count == 2, "single player has no duplicate tutorial entry")
	app.navigate(&"guide")
	await process_frame
	var guide: GuidePage = app.page as GuidePage
	check(guide.tabs.get_tab_count() == 3 and guide._course_list.get_child_count() == app.service.tutorial_lessons.size() and guide._course_list.get_child_count() == 8, "guide has three offline tabs and all installed lessons")
	await capture("guide-chapters")
	guide.tabs.current_tab = 1
	for index: int in range(guide._rule_chapters.item_count):
		guide._read_rule(index)
		check(guide._rule_text.text.contains("2.6.4-hotfix.1") and not guide._rule_text.text.contains("\n略。"), "rule chapters preserve their rules version and remain complete")
	check(not str(guide._rules["chapters"][0]["text"]).contains("②在自己的结束阶段"), "overload is a defeat condition, not victory for the overloaded player")
	await capture("guide-rulebook")
	guide.tabs.current_tab = 2
	check(guide._library._grid.get_child_count() == 90, "guide catalogue uses all 90 release cards")
	await capture("guide-catalogue")
	guide.tabs.current_tab = 0
	guide.lesson_requested.emit("lesson-01-first-match")
	await process_frame
	var page := app.page as MatchPage
	check(page != null and page._overlay.visible, "guide opens the installed course")
	await capture("guide-tutorial")
	page._continue_tutorial()
	var snap := app.service.snapshot()
	check(snap.tutorial.step == "gesture", "welcome advances to the real opening")
	var response := app.service.match_request(&"submit_command", [0, {"type": KernelContract.CommandKind.SubmitOpeningGesture, "round": str(snap.openingRound), "gesture": "1"}, snap.generation, snap.revision])
	check(response.get("ok", false), "learner chooses a gesture")
	snap = app.service.snapshot()
	if snap.tutorial.step == "gesture":
		response = app.service.match_request(&"step_ai", [snap.generation, snap.revision])
		check(response.get("ok", false), "course opponent confirms the losing gesture")
	app.service.poll_session()
	await process_frame
	page._continue_tutorial()
	await process_frame
	check(app.service.snapshot().tutorial.step == "rank", "learner sees the card detail steps")
	var mouse := InputEventMouseButton.new()
	mouse.button_index = MOUSE_BUTTON_LEFT
	mouse.position = page.canvas.tutorial_zone_rect(0, "ash").get_center()
	click_board(page.canvas, mouse.position)
	check(app.service.snapshot().tutorial.step == "rank", "unrelated map region does not advance the card step")
	for detail: String in ["rank", "cost", "type", "effect"]:
		check(app.service.snapshot().tutorial.step == detail, "correct card visits " + detail)
		for hand_card: CardPresentation in page.canvas.field.hand:
			if hand_card.definition_id == "v260-analytic-0006":
				mouse.position = HandInput.point(page.canvas, hand_card) - page.canvas.get_global_rect().position
				click_board(page.canvas, mouse.position)
				break
		await process_frame
	for zone: String in ["hand", "formation", "analysis", "casting", "ash"]:
		check(app.service.snapshot().tutorial.step == "tour-" + zone, "course points at " + zone)
		mouse.position = page.canvas.tutorial_zone_rect(1, "casting").get_center()
		click_board(page.canvas, mouse.position)
		check(app.service.snapshot().tutorial.step == "tour-" + zone, "opponent area never advances the learner tour")
		var area := page.canvas.tutorial_zone_rect(0, zone)
		mouse.position = area.get_center()
		if zone == "analysis" and page.canvas.tutorial_zone_rect(0, "formation").has_point(mouse.position):
			mouse.position = area.position + Vector2(4, 4)
		click_board(page.canvas, mouse.position)
		await process_frame
	check(app.service.snapshot().tutorial.step == "objective", "five distinct on-board targets advance the tour")
	check(app.service.leave_match(), "a course can be interrupted")
	app.navigate(&"guide")
	await process_frame
	(app.page as GuidePage).lesson_requested.emit("lesson-01-first-match")
	await process_frame
	check(app.service.snapshot().tutorial.step == "welcome", "returning after an interruption restarts the lesson")
	check(app.service.leave_match(), "first course closes before starting the editor course")
	app.navigate(&"guide")
	await process_frame
	var last_course: Dictionary = app.service.tutorial_lessons.back()
	check(last_course.get("kind") == "editor", "the eighth guide entry belongs to the deck editor")
	var editor_button := (app.page as GuidePage)._course_list.get_child(7) as Button
	editor_button.pressed.emit()
	await process_frame
	check(app.page is DeckEditorPage and not app.service.match_active, "the eighth guide button opens a real deck editor")
	app.queue_free()
	await process_frame
	print("Catalog guide failures: ", failures)
	quit(0 if failures == 0 else 1)

func capture(label: String) -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	var output: String = ""
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--capture-output":
			output = arguments[index + 1]
	if output.is_empty():
		return
	DirAccess.make_dir_recursive_absolute(output)
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1920, 1080), Vector2i(2560, 1440), Vector2i(1440, 900)]:
		root.size = dimensions
		await process_frame
		await process_frame
		await RenderingServer.frame_post_draw
		check(root.get_texture().get_image().save_png(output.path_join(label + "-%dx%d.png" % [dimensions.x, dimensions.y])) == OK, "screenshot saved")
