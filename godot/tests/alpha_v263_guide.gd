extends SceneTree

var failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func check(value: bool, message: String) -> void:
	if not value:
		failures += 1
		push_error("V263_GUIDE: " + message)

func _run() -> void:
	var app: WizardFrontend = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(app)
	await process_frame
	check(app.service.error.is_empty(), "current content loads")
	check((app.page as MenuPage)._buttons[3].text == "巫师指南", "menu has the unified guide entry")
	app.navigate(&"setup")
	await process_frame
	check((app.page as SetupPage)._mode.item_count == 2, "single player has no duplicate tutorial entry")
	app.navigate(&"guide")
	await process_frame
	var guide: GuidePage = app.page as GuidePage
	check(guide.tabs.get_tab_count() == 3 and guide.chapter_buttons.size() == 5, "guide has three offline tabs and five unlocked chapters")
	await capture("guide-chapters")
	guide.tabs.current_tab = 1
	for index: int in range(guide._rule_chapters.item_count):
		guide._read_rule(index)
		check(guide._rule_text.text.contains("2.6.3") and not guide._rule_text.text.contains("\n略。"), "rule chapters are complete and versioned")
	await capture("guide-rulebook")
	guide.tabs.current_tab = 2
	check(guide._library._grid.get_child_count() == 90, "guide catalogue uses all 90 release cards")
	await capture("guide-catalogue")
	guide.tabs.current_tab = 0
	guide.chapter_buttons[0].pressed.emit()
	await process_frame
	var page: MatchPage = app.page as MatchPage
	check(page != null and page._tutorial.visible, "chapter opens real match instructions")
	await capture("guide-tutorial")
	page._inspect_tutorial_area("ash")
	check(int(app.service.snapshot().get("tutorial", {}).get("step", -1)) == 0, "wrong area does not advance")
	for area: String in ["own_status", "opponent_status", "hand", "main_deck", "side_deck", "formation", "casting", "ash", "phase"]:
		page._inspect_tutorial_area(area)
		await process_frame
	check(bool(app.service.snapshot().get("tutorial", {}).get("complete", false)), "area interactions complete chapter one")
	check(GuideProgress.completed(app.service.user_directory).has(1), "completion is persisted locally")
	page._tutorial_retry.pressed.emit()
	await process_frame
	page = app.page as MatchPage
	check(int(app.service.snapshot().get("tutorial", {}).get("step", -1)) == 0, "retry restarts from chapter beginning")
	page._tutorial_return.pressed.emit()
	await process_frame
	check(app.page is GuidePage and (app.page as GuidePage).chapter_buttons[0].text.contains("已完成"), "return shows saved completion")
	for chapter: int in range(2, 6):
		(app.page as GuidePage).start_chapter(chapter)
		await process_frame
		page = app.page as MatchPage
		check(page != null and int(app.service.snapshot().get("tutorial", {}).get("chapter", 0)) == chapter, "chapter starts independently")
		page._tutorial_return.pressed.emit()
		await process_frame
	app.queue_free()
	await process_frame
	print("Alpha v2.6.3 guide failures: ", failures)
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
