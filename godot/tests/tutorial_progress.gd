extends SceneTree

var failed := false

func check(ok: bool, message: String) -> void:
	if not ok:
		failed = true
		push_error(message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var directory := ProjectSettings.globalize_path("user://").path_join("wizard-guide-%d" % Time.get_ticks_usec())
	DirAccess.make_dir_recursive_absolute(directory)
	var initial := GuideProgress.read(directory)
	check(not initial.askedIntro and not initial.isVeteran and initial.completed.is_empty(), "fresh guide")
	var page: WizardFrontend = preload("res://app/main.tscn").instantiate()
	root.add_child(page)
	page.service.user_directory = directory
	var catalog: Node = root.get_node("Text")
	page.service.tutorial_lessons.clear()
	for order: int in range(1, 9):
		page.service.tutorial_lessons.append({"order": order, "id": "fixture%d" % order, "title": "Guide"})
	page.navigate(&"menu")
	page._offer_intro()
	check(page._intro.visible and page._intro._actions.get_child(0).has_focus(), "first-run question focuses new player")
	check(page._intro._body.is_visible_in_tree() and page._intro._body.text == catalog.t("tutorial.general.g01"), "G01 first-run question")
	check(page._intro._actions.get_child(0).text == catalog.t("tutorial.general.g01.new") and page._intro._actions.get_child(1).text == catalog.t("tutorial.general.g01.veteran"), "G01 choices")
	page._intro._actions.get_child(1).pressed.emit()
	check(GuideProgress.read(directory).isVeteran and page.current_page == &"menu", "veteran stays at menu")
	page._offer_intro()
	check(not page._intro.visible, "answer persists")
	check(GuideProgress.skip(directory, "deck", true), "first deck skip")
	check(GuideProgress.read(directory).deckSkipCount == 1, "second invitation is available")
	check(GuideProgress.skip(directory, "deck", true) and GuideProgress.skip(directory, "deck", true), "repeat skip")
	check(GuideProgress.read(directory).skipped == ["deck"] and GuideProgress.read(directory).deckSkipCount == 2, "only two automatic deck visits")
	check(GuideProgress.mark(directory, "deck") and GuideProgress.mark(directory, "deck"), "complete and replay")
	check(GuideProgress.read(directory).completed == ["deck"] and GuideProgress.read(directory).skipped.is_empty(), "completion supersedes skip")
	check(GuideProgress.skip(directory, "deck", false) and GuideProgress.read(directory).completed == ["deck"], "skipping a replay preserves completion")
	check(not FileAccess.file_exists(directory.path_join("guide-progress.json.tmp")), "atomic write finished")
	for order: int in range(1, 4):
		check(GuideProgress.mark(directory, "fixture%d" % order), "complete a catalog lesson")
	page.navigate(&"menu")
	var status: Label = page.page.get_node("%Status")
	var notice := page.service.error if not page.service.error.is_empty() else page.service.storage_notice
	var progress_text: String = catalog.t("tutorial.general.g06", {"completed": 3, "total": 8})
	var expected := progress_text if notice.is_empty() else notice + "　" + progress_text
	check(status.is_visible_in_tree() and status.text == expected, "G06 main menu counts installed courses and preserves the storage notice")
	page.queue_free()
	await process_frame
	DirAccess.remove_absolute(directory.path_join("guide-progress.json"))
	DirAccess.remove_absolute(directory)
	quit(1 if failed else 0)
