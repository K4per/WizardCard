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
	page.service.tutorial_lessons.assign([{"order": 1, "id": "fixture", "title": "Guide"}])
	page.navigate(&"menu")
	page._offer_intro()
	check(page._intro.visible and page._intro._actions.get_child(0).has_focus(), "first-run question focuses new player")
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
	page.queue_free()
	await process_frame
	DirAccess.remove_absolute(directory.path_join("guide-progress.json"))
	DirAccess.remove_absolute(directory)
	quit(1 if failed else 0)
