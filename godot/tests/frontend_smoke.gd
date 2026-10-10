extends SceneTree

var _failures: Array[String] = []
var _confirmed: Array = []
var _cancelled: bool = false
var _app: WizardFrontend
var _capture: bool = false
var _output: String = ""
var _report: String = ""
var _checks: int = 0

func _initialize() -> void:
	call_deferred(&"_run")

func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(message)
		push_error(message)

func _settle() -> void:
	await process_frame
	await process_frame

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	_capture = arguments.has("--capture")
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--output":
			_output = arguments[index + 1]
		elif arguments[index] == "--report":
			_report = arguments[index + 1]
	if not _output.is_empty():
		DirAccess.make_dir_recursive_absolute(_output)
	_app = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(_app)
	await _settle()
	_check(_app.service.error.is_empty(), "Native service must load actual content: " + _app.service.error)
	_check(_app.service.cards.size() == 30, "All 30 official cards must be browsable")
	await _screenshot("menu")
	var hidden: CardPresentation = CardPresentation.from_projection({"id": "private-1", "hidden": true, "definition": {"name": "secret", "id": "fireball", "type": 4, "text": "secret"}})
	_check(hidden.hidden and hidden.title == "盖伏卡" and hidden.definition_id.is_empty() and hidden.text.is_empty() and hidden.type_key.is_empty(), "Hidden cards must discard identity before art/details")
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		_app.navigate(&"field")
		await _settle()
		var page: FieldPage = _app.page as FieldPage
		_check(page._detail.visible == false and page._decision.visible == false, "Field must start without persistent detail/decision panels")
		_check(page.canvas.get_child_count() == 0, "Field cards/rings must not create individual UI nodes")
		var bounds: Rect2 = Rect2(Vector2.ZERO, page.canvas.size)
		for hit: Dictionary in page.canvas._card_hits:
			_check(bounds.encloses(hit["rect"]), "Cards must fit field at " + str(dimensions))
		_check(page.canvas._card_hits.size() >= 8, "Field must contain rendered card hit regions")
		var own: FormationPresentation = page.canvas.field.own_formations[0]
		for rings: int in [0, 1, 7, 12]:
			own.ring_count = rings
			page.canvas.queue_redraw()
			await _settle()
			var count: int = 0
			for hit: Dictionary in page.canvas._ring_hits:
				if hit["host"] == own.card.instance_id:
					count += 1
				_check(bounds.encloses(hit["rect"]), "Dynamic rings must stay within field")
			_check(count == rings, "Layout must use projected ring count " + str(rings))
		own.ring_count = 4
		page.canvas.queue_redraw()
		await _settle()
		await _screenshot("field-%dx%d" % [dimensions.x, dimensions.y])
		await _field_input_checks(page)
		page._show_detail(hidden)
		_check(page._detail._title.text == "盖伏卡" and not page._detail._body.text.contains("secret"), "Details must not reveal facedown identity")
		page._toggle_privacy()
		await _settle()
		_check(page.canvas._card_hits.is_empty() and page.canvas._ring_hits.is_empty() and not page._detail.visible and not page._decision.visible, "Handover clears render/input/private details")
		await _screenshot("privacy-%dx%d" % [dimensions.x, dimensions.y])
		page._toggle_privacy()
		await _settle()
		page._show_decision()
		await _settle()
		_check(page.canvas.mouse_filter == Control.MOUSE_FILTER_IGNORE and page._modal_shade.visible, "Material dialog blocks background field input")
		await _decision_checks(page._decision)
		page._show_decision()
		await _settle()
		await _screenshot("decision-%dx%d" % [dimensions.x, dimensions.y])
		# More formations are browsed by rows, instead of shrinking every card.
		for index: int in range(4):
			var extra: FormationPresentation = FormationPresentation.new()
			extra.card = own.card.duplicate() as CardPresentation
			extra.card.instance_id = "extra-" + str(index)
			extra.ring_count = 7
			page.canvas.field.own_formations.append(extra)
		page.canvas.queue_redraw()
		await _settle()
		page.canvas._scroll_rows(page.canvas._own_region.get_center(), 99)
		await _settle()
		var last_visible: bool = false
		for hit: Dictionary in page.canvas._card_hits:
			var shown: CardPresentation = hit["card"]
			last_visible = last_visible or shown.instance_id == "extra-3"
			_check(bounds.encloses(hit["rect"]), "Five formation rows remain inside field")
		_check(last_visible, "Wheel browsing reaches the last formation at " + str(dimensions))
		# All ordinary pages also reflow at each required size.
		_app.navigate(&"menu")
		await _settle()
		await _screenshot("menu-%dx%d" % [dimensions.x, dimensions.y])
		_app.navigate(&"settings")
		await _settle()
		await _screenshot("settings-%dx%d" % [dimensions.x, dimensions.y])
		_app.navigate(&"library")
		await _settle()
		var tiled: LibraryPage = _app.page as LibraryPage
		_check(tiled._grid.get_child_count() == 30, "Library lists actual cards at each size")
		await _screenshot("library-%dx%d" % [dimensions.x, dimensions.y])
	_app.navigate(&"library")
	await _settle()
	var library: LibraryPage = _app.page as LibraryPage
	library._search.text = "火球"
	library._search.text_changed.emit("火球")
	await _settle()
	_check(library._grid.get_child_count() == 1, "Chinese search must filter actual catalogue")
	var long_card: CardPresentation = _app.service.cards[0].duplicate() as CardPresentation
	long_card.title = "用于验证中文长名称和完整卡牌详情显示的测试卡牌"
	long_card.text = "完整卡牌文本应在独立详情中滚动阅读。\n".repeat(60)
	library._show_detail(long_card)
	await _settle()
	_check(library._detail._title.text == long_card.title and library._detail._body.text == long_card.text, "Long name/text remain complete in details")
	await _screenshot("library-long-text")
	_app.navigate(&"settings")
	await _settle()
	var settings: SettingsPage = _app.page as SettingsPage
	settings._master.value = 37
	settings._reduced.button_pressed = true
	# Headless checks persist without changing the physical desktop mode.
	if not _capture:
		settings.save()
		_check(settings._notice.text == "已保存", "Settings must save through native application")
		var next_service: FrontendService = FrontendService.new()
		var user_directory: String = ""
		for index: int in range(arguments.size() - 1):
			if arguments[index] == "--user-data":
				user_directory = arguments[index + 1]
		_check(not user_directory.is_empty(), "Smoke tests must use an isolated user directory")
		var assets: String = ProjectSettings.globalize_path("res://").path_join("../assets").simplify_path()
		_check(next_service.initialize(assets, user_directory), "Reopen native settings")
		_check(int(next_service.settings.get("masterVolume", 0)) == 37 and bool(next_service.settings.get("reducedMotion", false)), "Settings persist across service instances")
		next_service.release()
	await _screenshot("settings")
	_app.navigate(&"menu")
	await _settle()
	_check(_app.current_page == &"menu", "Navigation returns to menu")
	if not _report.is_empty():
		var report_file: FileAccess = FileAccess.open(_report, FileAccess.WRITE)
		_check(report_file != null, "Write frontend report")
		if report_file != null:
			report_file.store_string(JSON.stringify({"success": _failures.is_empty(), "checks": _checks, "sizes": ["1280x720", "1600x1000", "1920x1080"], "officialCards": _app.service.cards.size(), "fieldMode": "showcase-only", "renderer": DisplayServer.get_name(), "failures": _failures}, "\t"))
	print("Frontend smoke: ", "PASS" if _failures.is_empty() else "FAIL", "; checks=", _checks, "; sizes=1280x720,1600x1000,1920x1080; actual cards=", _app.service.cards.size())
	_app.queue_free()
	await _settle()
	quit(0 if _failures.is_empty() else 1)

func _decision_checks(panel: DecisionPanel) -> void:
	_confirmed.clear()
	_cancelled = false
	if not panel.confirmed.is_connected(_on_confirmed):
		panel.confirmed.connect(_on_confirmed)
		panel.cancelled.connect(_on_cancelled)
	var request: DecisionPresentation = panel.request
	var old_generation: String = request.generation
	panel._on_choice(true, request.choices[0].id, request.generation, request.revision, request.decision_id)
	_check(not panel._confirm.disabled, "One valid material enables confirmation")
	panel._on_choice(true, request.choices[1].id, request.generation, request.revision, request.decision_id)
	_check(panel._selected.size() == 1 and panel._selected[0] == request.choices[1].id, "Single choice switches cleanly to another material")
	panel._on_confirm(request.generation, request.revision, request.decision_id)
	_check(_confirmed.size() == 4 and _confirmed[0] == "9007199254740993" and _confirmed[2] == "18446744073709551615", "Decision callback preserves uint64 strings")
	panel.configure(request)
	await _settle()
	request.revision = "13"
	panel.configure(request)
	panel._on_choice(true, request.choices[0].id, old_generation, "12", request.decision_id)
	_check(panel._selected.is_empty(), "Old revision callbacks cannot select new materials")
	panel._on_choice(true, "unknown-card", request.generation, request.revision, request.decision_id)
	_check(panel._selected.is_empty(), "Unavailable materials cannot be selected")
	panel._on_cancel(request.generation, request.revision, request.decision_id)
	_check(_cancelled, "Cancel returns intent without payment")
	panel.configure(request)
	panel.show()
	await _settle()

func _field_input_checks(page: FieldPage) -> void:
	var hand_card: CardPresentation = page.canvas.field.hand[0]
	var hand_hit: Rect2
	for hit: Dictionary in page.canvas._card_hits:
		if hit["card"] == hand_card:
			hand_hit = hit["rect"]
	var point: Vector2 = page.canvas.global_position + hand_hit.get_center()
	_mouse_button(point, true)
	_mouse_button(point, false)
	await _settle()
	_check(page._detail.visible and page._detail.card.instance_id == hand_card.instance_id, "Actual viewport mouse input opens the correct card")
	var escape: InputEventKey = InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	root.push_input(escape, true)
	await _settle()
	_check(not page._detail.visible and _app.current_page == &"field", "Escape closes detail before navigating away")
	var ring_hit: Rect2
	for hit: Dictionary in page.canvas._ring_hits:
		if hit["host"] == page.canvas.field.own_formations[0].card.instance_id and hit["ring"] == 1:
			ring_hit = hit["rect"]
	var target: Vector2 = page.canvas.global_position + ring_hit.get_center()
	var count: int = page.canvas.field.hand.size()
	_mouse_button(point, true)
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = target
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	root.push_input(motion, true)
	_mouse_button(target, false)
	await _settle()
	_check(page._notice.text.contains("环位 2"), "Actual viewport drag identifies the projected ring")
	_check(page.canvas.field.hand.size() == count and not page.canvas.field.own_formations[0].occupants.has(1), "Dragging does not pay or mutate projected state")

func _mouse_button(point: Vector2, pressed: bool) -> void:
	var event: InputEventMouseButton = InputEventMouseButton.new()
	event.position = point
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = pressed
	root.push_input(event, true)

func _on_confirmed(generation: String, revision: String, decision_id: String, selected: Array[String]) -> void:
	_confirmed = [generation, revision, decision_id, selected]

func _on_cancelled(_generation: String, _revision: String, _decision_id: String) -> void:
	_cancelled = true

func _screenshot(name: String) -> void:
	if not _capture or _output.is_empty():
		return
	await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	_check(not image.is_empty(), "Rendered capture cannot be empty")
	var result: Error = image.save_png(_output.path_join(name + ".png"))
	_check(result == OK, "Save rendered capture " + name)
