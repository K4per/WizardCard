extends SceneTree

var failed := false
var app: WizardFrontend

func check(condition: bool, message: String) -> void:
	if not condition:
		failed = true
		push_error(message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	var user_index := args.find("--user-data")
	if user_index < 0 or not args[user_index + 1].ends_with("tutorial_deck_lesson-user"):
		quit(2)
		return
	var directory := args[user_index + 1]
	var path := directory.path_join("guide-progress.json")
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(path)
	app = preload("res://app/main.tscn").instantiate()
	root.add_child(app)
	await process_frame
	check(app.service.tutorial_lessons.size() == 8 and app._intro.visible, "fresh install exposes eight validated lessons")
	app._intro._actions.get_child(0).pressed.emit()
	await process_frame
	check(app.page is MatchPage and app.service.snapshot().tutorial.order == 1, "new player enters first real match")
	var first: String = app.service.snapshot().tutorial.lessonId
	app._start_lesson("lesson-07-chain")
	check(app.service.snapshot().tutorial.lessonId == first, "active course cannot be replaced")
	app.service.leave_match()
	app.navigate(&"menu")
	for count: int in range(2):
		(app.page as MenuPage)._buttons[2].pressed.emit()
		await process_frame
		check(app.page is DeckEditorPage, "editor course offered on visit %d" % (count + 1))
		(app.page as DeckEditorPage)._skip.pressed.emit()
		await process_frame
		check(app.page is DecksPage and GuideProgress.read(directory).deckSkipCount == count + 1, "skipped lesson opens real deck list")
		app.navigate(&"menu")
	(app.page as MenuPage)._buttons[2].pressed.emit()
	check(app.page is DecksPage, "third deck entry after two skips opens normal library")
	app.navigate(&"guide")
	var course: Dictionary = app.service.tutorial_lessons.back()
	(app.page as GuidePage).lesson_requested.emit(str(course.id))
	await process_frame
	var editor := app.page as DeckEditorPage
	var catalog := root.get_node("Text")
	check(editor._overlay.hint.text == catalog.t(course.steps.open.hintKey), "opening instruction appears in editor")
	check(editor._main_count.get_theme_color(&"font_color") == Color("f1aaa0") and editor._side_count.get_theme_color(&"font_color") == DeckTheme.TEXT, "37/40 highlights the incomplete main deck")
	check(editor._pool_ids.size() == 6 and editor._built.ids.size() == 37 and editor._side.ids.size() == 10, "approved starting deck and six choices")
	editor._save.pressed.emit()
	check(editor._step == "draft" and not GuideProgress.completed(directory).has(course.id), "37/10 is saved as a draft")
	check(editor._notice.text == catalog.t(course.steps.draft.hintKey), "draft receipt uses the lesson text")
	editor._overlay.advance.pressed.emit()
	check(editor._overlay.hint.text == catalog.t(course.steps.rules.hintKey), "deck rules shown after draft explanation")
	editor._side.inspected.emit(editor._side.ids.front())
	check(editor._overlay.hint.text == catalog.t(course.steps.side.hintKey), "inspection explains formation deck")
	var candidates: Array = course.candidates
	var bolt: String = candidates[1]
	editor._pool.adjusted.emit(bolt, 1, "cards")
	editor._pool.adjusted.emit(bolt, 1, "cards")
	check(editor._step == "limit" and int(app.service.editing_draft.cards[bolt]) == 3, "fourth copy hits real editor limit")
	check(editor._overlay.hint.text == catalog.t(course.steps.limit.hintKey), "blocked fourth copy explains the name limit")
	for id: String in [candidates[0], candidates[2]]:
		editor._pool.adjusted.emit(id, 1, "cards")
	var side_cards: Array = editor._side.ids.duplicate()
	for id: String in side_cards:
		editor._side.adjusted.emit(id, -1, "sideCards")
	editor._save.pressed.emit()
	check(editor._step == "sideEmpty" and not GuideProgress.completed(directory).has(course.id), "40/0 saves as incomplete")
	check(editor._overlay.hint.text == catalog.t(course.steps.sideEmpty.hintKey) and editor._notice.text == catalog.t("tutorial.editor.side_empty"), "empty side deck explains start formation")
	for id: String in side_cards:
		check(editor._pool_ids.has(id), "removed side card can be restored")
		editor._pool.adjusted.emit(id, 1, "sideCards")
	editor._save.pressed.emit()
	check(editor._end.visible and editor._step == "complete" and GuideProgress.completed(directory).has(course.id), "legal saved 40/10 earns completion")
	check(editor._main_count.get_theme_color(&"font_color") == DeckTheme.TEXT, "40/40 clears the incomplete count highlight")
	var receipt := editor._end.get_node("%Message") as Label
	check(receipt.is_visible_in_tree() and receipt.text.contains(catalog.t(course.steps.saved.hintKey)) and receipt.text.contains(catalog.t(course.steps.complete.hintKey)), "saved receipt and next instruction are visible together")
	var id: String = app.service.editing_draft.id
	check(app.service.open_draft(id) and app.service.draft_problems().is_empty(), "saved tutorial deck can be reopened")
	editor._end.get_node("%Next").pressed.emit()
	check(app.page is DecksPage, "finished player enters normal deck building")
	app.queue_free()
	await process_frame
	quit(1 if failed else 0)
