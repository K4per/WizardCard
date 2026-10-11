extends SceneTree

var _app: WizardFrontend
var _checks: int = 0
var _failures: Array[String] = []
var _capture: bool = false
var _output: String = ""
var _user_data: String = ""
var _assets: String = ""

func _initialize() -> void:
	call_deferred(&"_run")
	create_timer(45).timeout.connect(_timeout)

func _timeout() -> void:
	push_error("Deck smoke timed out")
	quit(2)

func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(message)
		push_error(message)

func _settle() -> void:
	await process_frame
	await process_frame
	await process_frame

func _run() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	_capture = args.has("--capture")
	for index: int in range(args.size() - 1):
		match args[index]:
			"--output": _output = args[index + 1]
			"--user-data": _user_data = args[index + 1]
			"--assets": _assets = args[index + 1]
	if _user_data.is_empty() or _assets.is_empty() or _output.is_empty():
		push_error("Isolated --user-data, --assets and --output are required")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(_output)
	_app = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(_app)
	await _settle()
	_check(_app.service.error.is_empty(), "Native bridge initializes: " + _app.service.error)
	_check(_app.service.cards.size() == 45, "Real native card catalogue is used")
	if not _app.service.error.is_empty():
		_finish()
		return
	var menu: MenuPage = _app.page as MenuPage
	menu._buttons[2].pressed.emit()
	await _settle()
	_check(_app.page is DecksPage, "Menu opens the working deck list")
	var list: DecksPage = _app.page as DecksPage
	list._new.pressed.emit()
	await _settle()
	var editor: DeckEditorPage = _app.page as DeckEditorPage
	_check(editor != null and int(_app.service.editing_draft.get("format", 0)) == 2, "New drafts use 40/10 profile")
	var id: String = str(_app.service.editing_draft["id"])
	editor._name.text = "星辉藏书 · 40 / 10"
	editor._rename(editor._name.text)
	# The same UI adjustment path handles right clicks and committed drops.
	editor._adjust("hexagram", 1, "cards")
	_check(DeckUi.total(_app.service.editing_draft["sideCards"]) == 1 and DeckUi.total(_app.service.editing_draft["cards"]) == 0, "Formation cards route into the side deck")
	editor._adjust("hexagram", -1, "sideCards")
	editor._adjust("small-hexagram", 1, "sideCards")
	for definition: Dictionary in _app.service.definitions.values():
		var membership: String = "sideCards" if int(definition["type"]) == 3 else "cards"
		var target: int = 10 if membership == "sideCards" else 40
		var remaining: int = target - DeckUi.total(_app.service.editing_draft[membership])
		var current: int = int(_app.service.editing_draft[membership].get(str(definition["id"]), 0))
		for copy: int in range(mini(3 - current, remaining)):
			editor._adjust(str(definition["id"]), 1, "cards")
	_app.service.editing_draft["baseFormation"] = "small-hexagram"
	editor._choose_section("cards")
	await _settle()
	_check(DeckUi.total(_app.service.editing_draft["cards"]) == 40, "Main deck reaches 40")
	_check(DeckUi.total(_app.service.editing_draft["sideCards"]) == 10, "Side deck reaches 10")
	_check(_app.service.draft_problems().is_empty(), "Native construction validator accepts complete 40/10")
	var complete: Dictionary = _app.service.editing_draft.duplicate(true)
	editor._adjust("fireball", 1, "cards")
	editor._adjust("hexagram", 1, "sideCards")
	_check(_app.service.editing_draft == complete, "Full deck and copy limits reject extra cards")
	_check(editor._tabs[0].text.contains("40 / 40") and editor._tabs[1].text.contains("10 / 10"), "Independent counts remain visible")
	_check(editor._initial_ids.has("small-hexagram"), "Initial formation choices come from own side deck")
	_check(editor._built.ids.size() == 30, "First main page displays 30 individual cards")
	editor._turn_built(1)
	_check(editor._built.ids.size() == 10, "Second main page displays remaining 10 cards")
	editor._turn_built(-1)
	editor._search.text = "火球"
	editor._query_changed("")
	_check(editor._pool_ids == ["fireball"], "Native Chinese substring filter returns actual Fireball")
	editor._filters[0].select(2)
	editor._query_changed("")
	_check(editor._pool_ids.is_empty(), "Combined name and type filters produce empty results")
	editor._reset_query()
	_check(editor._pool_ids.size() == 45, "Reset restores full catalogue")
	_check(_app.service.query_cards({"rank": 0}).is_empty(), "Rank filtering excludes unrelated types")
	_check(_app.service.query_cards({"type": 0, "castCost": 0}).is_empty(), "Removed action type does not match formal cards")
	await _input_checks(editor)
	_app.service.editing_draft = complete.duplicate(true)
	editor._refresh()
	_check(editor._save_draft(), "Native draft save succeeds")
	_check(not _app.service.draft_dirty(), "Save clears the dirty state")
	var reopened: FrontendService = FrontendService.new()
	_check(reopened.initialize(_assets, _user_data) and reopened.open_draft(id), "Independent service reloads saved deck")
	_check(reopened.editing_draft == complete, "Restart preserves all main, side and initial formation data")
	reopened.release()
	for dimensions: Vector2i in [Vector2i(1920, 1080), Vector2i(1600, 1000), Vector2i(1280, 720)]:
		root.size = dimensions
		await _settle()
		editor._choose_section("cards")
		await _settle()
		_layout_checks(editor, dimensions)
		await _screenshot("editor-main-%dx%d" % [dimensions.x, dimensions.y])
		editor._turn_built(1)
		await _settle()
		_check(editor._built._hits.size() == 10, "Second page hit regions match ten cards")
		if dimensions.x == 1920:
			await _screenshot("editor-main-page2-1920x1080")
		editor._choose_section("sideCards")
		await _settle()
		_check(editor._built.ids.size() == 10 and editor._built.slot_count == 10, "Side tab contains ten slots")
		_layout_checks(editor, dimensions)
		await _screenshot("editor-side-%dx%d" % [dimensions.x, dimensions.y])
	root.size = Vector2i(1920, 1080)
	await _settle()
	editor._rename("尚未保存的名字")
	editor.request_leave()
	_check(editor._dialog.visible, "Unsaved changes require a save/discard choice")
	await _screenshot("editor-unsaved-1920x1080")
	editor._dialog._choose(2)
	_check(_app.page == editor and _app.service.draft_dirty(), "Continue editing preserves the draft")
	editor.request_leave()
	editor._dialog._choose(1)
	await _settle()
	list = _app.page as DecksPage
	_check(list != null, "Discard returns to list")
	_check(_app.service.open_draft(id) and _app.service.editing_draft == complete, "Discard leaves persisted deck unchanged")
	await _screenshot("decks-1920x1080")
	list._request_delete(0)
	list._dialog._choose(1)
	_check(_app.service.drafts.size() == 1, "Cancel deletion retains deck")
	list._request_delete(0)
	list._dialog._choose(0)
	_check(_app.service.drafts.is_empty(), "Confirmed deletion persists")
	# An old draft only migrates in memory, never by silently rewriting the file.
	var old_response: Dictionary = _app.service._request(&"create_draft", [""])
	var old: Dictionary = old_response.get("data", {})
	old["format"] = 1
	old.erase("sideCards")
	old["cards"] = {"balance": 3, "engeas-four-point-star": 3, "pentagram-of-life": 3, "reservoir": 3, "fireball": 2}
	_check(bool(_app.service._request(&"save_draft", [old]).get("ok", false)), "Legacy draft can coexist with new drafts")
	_app.service.refresh_library()
	_check(_app.service.open_draft(str(old["id"])), "Legacy draft opens for migration")
	_check(int(_app.service.editing_draft["format"]) == 2 and _app.service.editing_draft["sideCards"].get("hexagram", 0) == 3 and not _app.service.editing_draft["cards"].has("balance") and _app.service.editing_draft["baseFormation"] == "small-hexagram" and _app.service.draft_dirty(), "Legacy formation migrates to the surviving card ID in a dirty draft")
	_app.navigate(&"deck_editor")
	await _settle()
	editor = _app.page as DeckEditorPage
	editor._choose_section("sideCards")
	await _settle()
	_check(editor._built._hits.size() == 13, "Excess legacy side cards remain visible for repair")
	editor._adjust("reservoir", -1, "sideCards")
	editor._adjust("reservoir", -1, "sideCards")
	editor._adjust("reservoir", -1, "sideCards")
	_check(DeckUi.total(_app.service.editing_draft["sideCards"]) == 10, "Legacy excess can be removed without dropping hidden cards")
	_app.service.erase_draft(str(old["id"]))
	_app.navigate(&"decks")
	await _settle()
	list = _app.page as DecksPage
	list._copy_preset()
	await _settle()
	_check(_app.page is DeckEditorPage and not _app.service.editing_draft["sideCards"].is_empty(), "Copied legacy preset separates formations into side deck")
	editor = _app.page as DeckEditorPage
	editor._rename("保存并离开验证")
	editor.request_leave()
	editor._dialog._choose(0)
	await _settle()
	_check(_app.page is DecksPage and _app.service.drafts.size() == 1, "Save-and-leave stores an incomplete draft")
	# A failed save preserves the draft and stays in the editor.
	_app.navigate(&"deck_editor")
	await _settle()
	editor = _app.page as DeckEditorPage
	editor._rename("失败时保留编辑")
	var unsaved: Dictionary = _app.service.editing_draft.duplicate(true)
	var deck_file: String = _user_data.path_join("decks.json")
	var backup_file: String = _user_data.path_join("decks-smoke-backup.json")
	_check(DirAccess.rename_absolute(deck_file, backup_file) == OK and DirAccess.make_dir_absolute(deck_file) == OK, "Isolated save failure fixture installed")
	editor.request_leave()
	editor._dialog._choose(0)
	await _settle()
	_check(_app.page == editor and _app.service.editing_draft == unsaved and _app.service.draft_dirty() and not _app.service.error.is_empty(), "Save failure retains all edits and displays an error")
	_check(DirAccess.remove_absolute(deck_file) == OK and DirAccess.rename_absolute(backup_file, deck_file) == OK, "Isolated persistence restored")
	_app.service.erase_draft(str(unsaved["id"]))
	_finish()

func _layout_checks(editor: DeckEditorPage, dimensions: Vector2i) -> void:
	var bounds: Rect2 = Rect2(Vector2.ZERO, Vector2(dimensions))
	for control: Control in [editor._name, editor._initial, editor._detail, editor._built_panel, editor._pool_panel, editor._save, editor._back, editor._notice]:
		_check(bounds.encloses(control.get_global_rect()), "Editor controls fit " + str(dimensions) + ": " + control.get_class() + " " + str(control.get_global_rect()))
	_check(not editor._detail.get_global_rect().intersects(editor._built_panel.get_global_rect()) and not editor._built_panel.get_global_rect().intersects(editor._pool_panel.get_global_rect()), "Three columns retain separate bounds")
	for grid: DeckCardGrid in [editor._built, editor._pool]:
		for hit: Dictionary in grid._hits:
			_check(Rect2(Vector2.ZERO, grid.size).encloses(hit["rect"]), "Full card frames remain in grid")
	for filter: OptionButton in editor._filters:
		_check(editor._pool_panel.get_global_rect().encloses(filter.get_global_rect()), "Filters fit right column")
	for kind: String in ["analysis", "talisman", "incantation", "formation", "rune"]:
		var dimensions_array: Array = ArtBank.metadata("c2d.card.detail." + kind + ".ai")["size"]
		_check(int(dimensions_array[0]) == 1086 and int(dimensions_array[1]) == 1448, "Cards keep the original full art frame")

func _motion(position: Vector2, relative: Vector2 = Vector2.ZERO, held: bool = false) -> void:
	var event: InputEventMouseMotion = InputEventMouseMotion.new()
	event.position = position
	event.relative = relative
	event.button_mask = MOUSE_BUTTON_MASK_LEFT if held else 0
	root.push_input(event, true)

func _click(position: Vector2, pressed: bool, button: MouseButton = MOUSE_BUTTON_LEFT) -> void:
	var event: InputEventMouseButton = InputEventMouseButton.new()
	event.position = position
	event.button_index = button
	event.pressed = pressed
	event.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed and button == MOUSE_BUTTON_LEFT else 0
	root.push_input(event, true)

func _start_drag(grid: DeckCardGrid) -> void:
	var point: Vector2 = grid.global_position + (grid._hits[0]["rect"] as Rect2).get_center()
	_motion(point)
	_click(point, true)
	await _settle()
	_motion(point + Vector2(32, 8), Vector2(32, 8), true)
	await _settle()

func _input_checks(editor: DeckEditorPage) -> void:
	editor._choose_section("cards")
	await _settle()
	var first: String = editor._built.ids[0]
	var initial: int = DeckUi.total(_app.service.editing_draft["cards"])
	var point: Vector2 = editor._built.global_position + (editor._built._hits[0]["rect"] as Rect2).get_center()
	_motion(point)
	_click(point, true, MOUSE_BUTTON_RIGHT)
	_click(point, false, MOUSE_BUTTON_RIGHT)
	await _settle()
	_check(DeckUi.total(_app.service.editing_draft["cards"]) == initial - 1, "Real right click removes exactly one card")
	_check(editor._detail._title.text == _app.service.card_by_id(first).title, "Pointer inspection displays actual card details")
	editor._search.text = _app.service.card_by_id(first).title
	editor._query_changed("")
	await _settle()
	point = editor._pool.global_position + (editor._pool._hits[0]["rect"] as Rect2).get_center()
	_motion(point)
	_click(point, true, MOUSE_BUTTON_RIGHT)
	_click(point, false, MOUSE_BUTTON_RIGHT)
	await _settle()
	_check(DeckUi.total(_app.service.editing_draft["cards"]) == initial, "Real pool right click adds one card")
	for cancellation: String in ["escape", "right", "focus", "invalid"]:
		var before: Dictionary = _app.service.editing_draft.duplicate(true)
		await _start_drag(editor._built)
		_check(root.gui_is_dragging(), "Native viewport starts drag: " + cancellation)
		_check(_app.service.editing_draft == before, "Drag start cannot mutate counts")
		match cancellation:
			"escape":
				var key: InputEventKey = InputEventKey.new()
				key.keycode = KEY_ESCAPE
				key.pressed = true
				root.push_input(key, true)
				key = key.duplicate() as InputEventKey
				key.pressed = false
				root.push_input(key, true)
			"right":
				_click(point, true, MOUSE_BUTTON_RIGHT)
				_click(point, false, MOUSE_BUTTON_RIGHT)
			"focus": root.focus_exited.emit()
			"invalid": _motion(Vector2(4, 4), Vector2(20, 20), true)
		_click(Vector2(4, 4), false)
		await _settle()
		_check(not root.gui_is_dragging() and _app.service.editing_draft == before and not editor._dialog.visible, "Cancelled/invalid drag leaves draft unchanged: " + cancellation)
	await _start_drag(editor._built)
	point = editor._pool.get_global_rect().get_center()
	_motion(point, Vector2(10, 10), true)
	await _settle()
	_click(point, false)
	await _settle()
	_check(DeckUi.total(_app.service.editing_draft["cards"]) == initial - 1, "Committed built-to-pool drag removes one")
	await _start_drag(editor._pool)
	point = editor._built.get_global_rect().get_center()
	_motion(point, Vector2(10, 10), true)
	await _settle()
	_click(point, false)
	await _settle()
	_check(DeckUi.total(_app.service.editing_draft["cards"]) == initial, "Committed pool-to-built drag restores one")
	editor._reset_query()

func _screenshot(name: String) -> void:
	if not _capture:
		return
	_motion(Vector2(4, 4))
	await _settle()
	await RenderingServer.frame_post_draw
	var shot: Image = root.get_texture().get_image()
	_check(shot.save_png(_output.path_join(name + ".png")) == OK, "Screenshot saved: " + name)

func _finish() -> void:
	var file: FileAccess = FileAccess.open(_output.path_join("result.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify({"checks": _checks, "failures": _failures, "renderer": "rendered" if _capture else "headless"}, "\t"))
	print("DECK_SMOKE: ", _checks, " checks; ", _failures.size(), " failures")
	_app.queue_free()
	await process_frame
	quit(0 if _failures.is_empty() else 1)
