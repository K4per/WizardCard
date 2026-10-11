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
	if not _app.service.error.is_empty():
		quit(2)
		return
	_check(_app.service.cards.size() == 46, "All 46 official cards must be browsable")
	await _screenshot("menu")
	var hidden: CardPresentation = CardPresentation.from_projection({"id": "private-1", "hidden": true, "definition": {"name": "secret", "id": "fireball", "type": 4, "text": "secret"}})
	_check(hidden.hidden and hidden.title == "盖伏卡" and hidden.definition_id.is_empty() and hidden.text.is_empty() and hidden.type_key.is_empty(), "Hidden cards must discard identity before art/details")
	_check(CardPainter.frame_id(hidden) == "c2d.card.back.ai", "Hidden cards must use the anonymous original AI back")
	for kind: String in ["analysis", "talisman", "incantation", "formation", "rune"]:
		var frame: Dictionary = ArtBank.metadata("c2d.card.detail." + kind + ".ai")
		var frame_size: Array = frame.get("size", [0, 0])
		_check(int(frame_size[0]) == 1086 and int(frame_size[1]) == 1448 and frame.has("runtimeLayout"), "Original AI frame and measured layout available: " + kind)
		_check(ArtBank.metadata("c2d.card.builder." + kind).is_empty(), "Simplified runtime templates are excluded: " + kind)
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		_app.navigate(&"field")
		await _settle()
		var page: FieldPage = _app.page as FieldPage
		_check(page._detail.visible == false and page._decision.visible == false, "Field must start without persistent detail/decision panels")
		_check(page.canvas.get_child_count() == 0, "Field cards/rings must not create individual UI nodes")
		var bounds: Rect2 = Rect2(Vector2.ZERO, page.canvas.size)
		_check(page.canvas._own_region.size.x >= page.canvas.size.x * 0.7, "Unified analysis area uses most of the battlefield width")
		_check(page.canvas._own_region.end.y >= page.canvas.size.y * 0.97, "Analysis floor reaches the bottom even beneath the hand")
		_check(page.canvas._own_casting_region.size.y >= 44 and not page.canvas._own_casting_region.intersects(page.canvas._own_region), "Casting area remains usable without overlapping the analysis area")
		_check(BattleArt.texture("analysis_floor") != null and BattleArt.texture("casting_floor") != null and BattleArt.texture("spell_art_normal") != null and BattleArt.texture("spell_art_selected") != null, "Generated floor and spell border assets are available")
		for hit: Dictionary in page.canvas._card_hits:
			_check(bounds.encloses(hit["rect"]), "Cards must fit field at " + str(dimensions))
		_check(page.canvas._card_hits.size() >= 8, "Field must contain rendered card hit regions")
		var own: FormationPresentation = page.canvas.field.own_formations[0]
		var host_anchor: Rect2 = page.canvas.card_bounds(own.card)
		_check(host_anchor.position.x < page.canvas._own_region.position.x + page.canvas._own_region.size.x * 0.15 and host_anchor.size.x <= 80, "Small formation tiles begin at the left of the analysis area")
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
			_check(page.canvas.card_bounds(own.card).is_equal_approx(host_anchor), "Changing ring count cannot recenter the formation")
		own.ring_count = 4
		page.canvas.queue_redraw()
		await _settle()
		await _screenshot("field-%dx%d" % [dimensions.x, dimensions.y])
		await _field_input_checks(page)
		await _hand_layout_checks(page)
		await _hand_visibility_checks(page)
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
		# More formations remain browsable as clusters inside the unified area.
		for index: int in range(4):
			var extra: FormationPresentation = FormationPresentation.new()
			extra.card = own.card.duplicate() as CardPresentation
			extra.card.instance_id = "extra-" + str(index)
			extra.ring_count = 7
			page.canvas.field.own_formations.append(extra)
		page.canvas.queue_redraw()
		await _settle()
		page.canvas._scroll_formations(page.canvas._own_region.get_center(), 99)
		await _settle()
		var last_visible: bool = false
		for hit: Dictionary in page.canvas._card_hits:
			var shown: CardPresentation = hit["card"]
			last_visible = last_visible or shown.instance_id == "extra-3"
			_check(bounds.encloses(hit["rect"]), "Formation clusters remain inside field")
		_check(last_visible, "Wheel browsing reaches the last formation at " + str(dimensions))
		# All ordinary pages also reflow at each required size.
		_app.navigate(&"menu")
		await _settle()
		var menu: MenuPage = _app.page as MenuPage
		var menu_bounds: Rect2 = Rect2(Vector2.ZERO, Vector2(dimensions))
		_check(menu._buttons.size() == 5, "Menu preserves the five product entries")
		for button: MenuArtButton in menu._buttons:
			_check(menu_bounds.encloses(button.get_global_rect()), "Menu buttons fit at " + str(dimensions))
			_check(button._has_point(button.size * 0.5) and not button._has_point(Vector2.ZERO), "Menu hit test follows the pointed silhouette")
			var menu_font: Font = button.get_theme_font(&"font")
			var menu_size: int = button.get_theme_font_size(&"font_size")
			var skin: StyleBox = button.get_theme_stylebox(&"normal")
			var outline: int = button.get_theme_constant(&"outline_size") * 2
			_check(menu_font.get_font_weight() == 900, "Menu uses a real Black 900 weight, including disabled entries")
			var text_size: Vector2 = menu_font.get_string_size(button.text, HORIZONTAL_ALIGNMENT_LEFT, -1, menu_size)
			_check(text_size.x + outline <= button.size.x - skin.get_minimum_size().x and menu_font.get_height(menu_size) + outline <= button.size.y - skin.get_minimum_size().y, "Heavy menu text and outline fit without clipping at " + str(dimensions))
			for character: String in button.text:
				_check(menu_font.has_char(character.unicode_at(0)), "Menu Chinese glyph comes from the bundled font: " + character)
		_check(not menu._buttons[0].disabled and not menu._buttons[1].disabled and not menu._buttons[2].disabled, "Single player, multiplayer and deck construction are enabled")
		_check(menu._logo.texture is AtlasTexture and menu._buttons[3]._skin.texture is AtlasTexture, "Menu crops full HD alpha margins using declared texture regions")
		_check(menu_bounds.encloses(menu._tools.get_global_rect()), "Auxiliary menu links fit at " + str(dimensions))
		if _capture:
			var setting_button: MenuArtButton = menu._buttons[3]
			var pointer: InputEventMouseMotion = InputEventMouseMotion.new()
			pointer.position = setting_button.get_global_rect().get_center()
			root.push_input(pointer, true)
			await _settle()
			_check(setting_button._state == "hover", "Menu hover uses the new texture")
			setting_button.grab_focus()
			await _settle()
			_check(setting_button._state == "focus", "Keyboard focus uses corner markers")
			_mouse_button(pointer.position, true)
			await _settle()
			_check(setting_button._state == "pressed", "Pressed texture takes priority over opaque focus")
			_mouse_button(pointer.position, false)
		else:
			menu._buttons[3].pressed.emit()
		await _settle()
		_check(_app.current_page == &"settings", "New menu settings entry keeps working")
		_app.navigate(&"menu")
		await _settle()
		await _screenshot("menu-%dx%d" % [dimensions.x, dimensions.y])
		_app.navigate(&"settings")
		await _settle()
		var refined_settings: SettingsPage = _app.page as SettingsPage
		_check(menu_bounds.encloses(refined_settings._panel.get_global_rect()), "Refined settings panel fits at " + str(dimensions))
		for setting_control: Control in [refined_settings._master, refined_settings._sound, refined_settings._resolution, refined_settings._mode, refined_settings._reduced, refined_settings._automatic, refined_settings._save_button]:
			_check(menu_bounds.encloses(setting_control.get_global_rect()), "Settings controls remain reachable at " + str(dimensions))
		_check(refined_settings._master.get_theme_icon(&"grabber") is AtlasTexture and refined_settings._reduced.get_theme_icon(&"checked") is AtlasTexture, "Settings use generated gem handle and toggle sprites")
		await _screenshot("settings-%dx%d" % [dimensions.x, dimensions.y])
		await _settings_input_checks(refined_settings)
		_app.navigate(&"library")
		await _settle()
		var tiled: LibraryPage = _app.page as LibraryPage
		_check(tiled._grid.get_child_count() == 46, "Library lists all 46 authoritative cards at each size")
		await _screenshot("library-%dx%d" % [dimensions.x, dimensions.y])
		var first_tile: CardTile = tiled._grid.get_child(0) as CardTile
		first_tile.pressed.emit()
		await _settle()
		_check(tiled._detail._art.card == first_tile.card, "Selecting a card displays its complete framed face")
		_check(Rect2(Vector2.ZERO, Vector2(dimensions)).encloses(tiled._detail.get_global_rect()), "Framed details fit inside the viewport at " + str(dimensions))
		await _screenshot("library-ai-detail-%dx%d" % [dimensions.x, dimensions.y])
		tiled._kind_filter.select(4)
		tiled._kind_filter.item_selected.emit(4)
		await _settle()
		var formations: Array[CardPresentation] = _app.service.cards.filter(func(card: CardPresentation) -> bool: return card.type_key == "formation")
		_check(tiled._grid.get_child_count() == formations.size(), "Every formal formation appears under its type")
		var formation_tile: CardTile = tiled._grid.get_child(0) as CardTile
		formation_tile.pressed.emit()
		await _settle()
		_check(tiled._detail._body.text.contains("编号：01") and tiled._detail._body.text.contains("荷载上限："), "Formation detail includes official number and stats")
		_check(Rect2(Vector2.ZERO, Vector2(dimensions)).encloses(tiled._detail.get_global_rect()), "Formation details remain inside every supported viewport")
		await _screenshot("library-formation-%dx%d" % [dimensions.x, dimensions.y])
		tiled._kind_filter.select(0)
		tiled._kind_filter.item_selected.emit(0)
		await _settle()
		_check(tiled._grid.get_child_count() == 46 and not tiled._detail.visible, "Reset type restores the unified card pool and closes stale details")
	if _capture:
		await _frame_preview()
	_app.navigate(&"library")
	await _settle()
	var library: LibraryPage = _app.page as LibraryPage
	await _catalogue_checks(library)
	library._search.text = "火球"
	library._search.text_changed.emit("火球")
	await _settle()
	_check(library._grid.get_child_count() == 1, "Chinese search must filter actual catalogue")
	var long_card: CardPresentation = _app.service.cards[0].duplicate() as CardPresentation
	long_card.title = "用于验证中文长名称和完整卡牌详情显示的测试卡牌"
	long_card.text = "完整卡牌文本应在独立详情中滚动阅读。\n".repeat(60)
	library._show_detail(long_card)
	await _settle()
	_check(library._detail._title.text == long_card.title and library._detail._body.text == long_card.detail_text(), "Long name/text remain complete in details")
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
			report_file.store_string(JSON.stringify({"success": _failures.is_empty(), "checks": _checks, "sizes": ["1280x720", "1600x1000", "1920x1080"], "officialCards": _app.service.cards.size(), "fieldMode": "layout-fixture", "libraryMode": "authoritative", "renderer": DisplayServer.get_name(), "failures": _failures}, "\t"))
	print("Frontend smoke: ", "PASS" if _failures.is_empty() else "FAIL", "; checks=", _checks, "; sizes=1280x720,1600x1000,1920x1080; actual cards=", _app.service.cards.size())
	_app.queue_free()
	await _settle()
	quit(0 if _failures.is_empty() else 1)

func _catalogue_checks(library: LibraryPage) -> void:
	var numbers: Dictionary[String, bool] = {}
	var prefixes: Dictionary[String, String] = {"formation": "01", "analysis": "02", "incantation": "03", "talisman": "04", "rune": "05"}
	for card: CardPresentation in _app.service.cards:
		_check(card.number.length() == 6 and card.number.is_valid_int() and card.number.begins_with(prefixes[card.type_key]) and not numbers.has(card.number), "Each card has a unique six-digit number matching its kind: " + card.title)
		numbers[card.number] = true
		_check(card.rarity_text() in ["普通", "罕见", "稀有", "史诗", "传说"], "Rarity uses the Chinese display label: " + card.title)
		_check(ArtBank.texture("illustration." + card.art_id()) != null, "Every official card resolves an illustration: " + card.title)
		_check(ArtBank.texture(CardPainter.frame_id(card)) != null, "Every official card resolves a frame: " + card.title)
	library._search.text = "010002"
	library._search.text_changed.emit("010002")
	await _settle()
	_check(library._grid.get_child_count() == 1, "Official number search resolves exactly one card")
	if library._grid.get_child_count() > 0:
		var tile: CardTile = library._grid.get_child(0) as CardTile
		tile.pressed.emit()
		await _settle()
		_check(library._detail.card.title == "阿克乌姆之弧" and library._detail._body.text.contains("环位：0"), "Established formation name and zero-ring data are retained")
	library._close_detail()
	library._search.text = ""
	library._search.text_changed.emit("")
	await _settle()

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
	var hand_region: Dictionary = {}
	for hit: Dictionary in page.canvas._card_hits:
		if hit["card"] == hand_card:
			hand_hit = hit["rect"]
			hand_region = hit
			var pose: Transform2D = hit["pose"]
			_check(pose.get_rotation() < 0, "Left hand cards follow the fan angle")
			for other: Dictionary in page.canvas._card_hits:
				if other["card"] == page.canvas.field.hand[2]:
					var middle_pose: Transform2D = other["pose"]
					_check(pose.origin.y > middle_pose.origin.y + 16, "Fan edge cards sit below the middle card")
	# A transparent hand corner may legitimately expose a battlefield card
	# underneath. Isolate this card to check its inverse-rotation hit test.
	var all_hits: Array[Dictionary] = page.canvas._card_hits
	page.canvas._card_hits = [hand_region]
	_check(page.canvas.card_at(hand_hit.position + Vector2.ONE) != hand_card, "Transparent corners of a rotated card cannot capture clicks")
	page.canvas._card_hits = all_hits
	var point: Vector2 = page.canvas.global_position + hand_hit.get_center()
	_mouse_button(point, true)
	_mouse_button(point, false)
	await _settle()
	_check(page._detail.visible and page._detail.card.instance_id == hand_card.instance_id, "Actual viewport mouse input opens the correct card")
	_check(page._actions.visible and page._actions.get_global_rect().end.y <= point.y, "Context actions float above the selected card")
	_check(page._detail.battle_skin, "Field detail uses the new skin while retaining a full framed card")
	var raised: Rect2 = page.canvas.card_bounds(hand_card)
	_check(raised.position.y < hand_hit.position.y - 24, "Selected hand card extends out of the fan")
	_check(page.canvas.card_at(raised.get_center()) == hand_card and page.canvas._card_hits.back()["card"] == hand_card, "Selected card paints and receives clicks above neighbouring cards")
	await _screenshot("battle-selected-detail-%dx%d" % [root.size.x, root.size.y])
	_mouse_button(page.canvas.global_position + raised.get_center(), true, MOUSE_BUTTON_RIGHT)
	_mouse_button(page.canvas.global_position + raised.get_center(), false, MOUSE_BUTTON_RIGHT)
	await _settle()
	_check(page.canvas.selected_id.is_empty() and not page._detail.visible and not page._actions.visible, "Right click retracts the hand card and dismisses its UI")
	_check(page.canvas.card_bounds(hand_card).is_equal_approx(hand_hit), "Right click restores the resting fan position")
	_mouse_button(point, true)
	_mouse_button(point, false)
	await _settle()
	var escape: InputEventKey = InputEventKey.new()
	escape.keycode = KEY_ESCAPE
	escape.pressed = true
	root.push_input(escape, true)
	await _settle()
	_check(not page._detail.visible and _app.current_page == &"field", "Escape closes detail before navigating away")
	_check(not page._actions.visible and page.canvas.selected_id.is_empty(), "Closing details clears context actions and selection")
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
	page._show_decision()
	await _settle()
	page._decision.request.may_cancel = false
	page._decision.configure(page._decision.request)
	await _settle()
	root.push_input(escape, true)
	await _settle()
	_check(page._decision.visible and not page._decision._cancel.visible and page._modal_shade.visible, "Mandatory effect decisions ignore Escape and hide cancellation")
	_mouse_button(point, true, MOUSE_BUTTON_RIGHT)
	_mouse_button(point, false, MOUSE_BUTTON_RIGHT)
	await _settle()
	_check(page._decision.visible and page._modal_shade.visible and not page.canvas.hand_collapsed, "Right click cannot dismiss a mandatory effect decision or hide the underlying hand")
	var choice: DecisionChoice = page._decision.request.choices[0]
	var option: Button = page._decision._options[choice.id]
	_mouse_button(option.get_global_rect().get_center(), true)
	_mouse_button(option.get_global_rect().get_center(), false)
	await _settle()
	_check(page._decision._selected == [choice.id] and page._decision._preview.card == choice.card, "Clicking an effect candidate selects it and displays its complete card")
	_check(not page._decision._confirm.disabled, "A valid effect candidate enables confirmation")
	await _screenshot("battle-effect-selected-%dx%d" % [root.size.x, root.size.y])
	page._decision._confirm.pressed.emit()
	await _settle()
	_check(not page._decision.visible and not page._modal_shade.visible and page.canvas.mouse_filter == Control.MOUSE_FILTER_STOP, "Confirming effect selection restores field input")

func _hand_layout_checks(page: FieldPage) -> void:
	var original: Array[CardPresentation] = page.canvas.field.hand
	var many: Array[CardPresentation] = original.duplicate()
	for index: int in range(original.size(), 20):
		var card: CardPresentation = original[index % original.size()].duplicate() as CardPresentation
		card.instance_id = "hand-stress-" + str(index)
		many.append(card)
	page.canvas.field.hand = many
	page.canvas.queue_redraw()
	await _settle()
	var bounds: Rect2 = Rect2(Vector2.ZERO, page.canvas.size)
	for hit: Dictionary in page.canvas._card_hits:
		_check(bounds.encloses(hit["rect"]), "A twenty-card fan stays within the battlefield")
	page._show_detail(many[9])
	await _settle()
	var selected: Rect2 = page.canvas.card_bounds(many[9])
	_check(page.canvas.card_at(selected.get_center()) == many[9], "Raised card remains clickable in a crowded hand")
	if root.size == Vector2i(1920, 1080):
		await _screenshot("battle-hand-many-1920x1080")
	_mouse_button(page.canvas.global_position + selected.get_center(), true, MOUSE_BUTTON_RIGHT)
	_mouse_button(page.canvas.global_position + selected.get_center(), false, MOUSE_BUTTON_RIGHT)
	await _settle()
	_check(page.canvas.selected_id.is_empty(), "Right click retracts a card in a crowded hand")
	page.canvas.field.hand = original
	page.canvas.queue_redraw()
	await _settle()

func _mouse_button(point: Vector2, pressed: bool, button: MouseButton = MOUSE_BUTTON_LEFT) -> void:
	var event: InputEventMouseButton = InputEventMouseButton.new()
	event.position = point
	event.button_index = button
	event.pressed = pressed
	root.push_input(event, true)

func _hand_visibility_checks(page: FieldPage) -> void:
	var cards: Array[CardPresentation] = page.canvas.field.hand
	var blank: Vector2 = page.canvas.global_position + page.canvas.size * Vector2(0.96, 0.48)
	_mouse_button(blank, true, MOUSE_BUTTON_RIGHT)
	_mouse_button(blank, false, MOUSE_BUTTON_RIGHT)
	await _settle()
	_check(page.canvas.hand_collapsed and page._hand_button.text == "展开手牌", "Right click with no selected card collapses the whole hand")
	for hit: Dictionary in page.canvas._card_hits:
		_check(not cards.has(hit["card"]), "Collapsed hand cards have no invisible hit areas")
	_check(page.canvas.field.hand == cards and cards.size() == 5, "Collapsing the hand keeps its projected cards intact")
	await _screenshot("battle-hand-collapsed-%dx%d" % [root.size.x, root.size.y])
	_mouse_button(blank, true, MOUSE_BUTTON_RIGHT)
	_mouse_button(blank, false, MOUSE_BUTTON_RIGHT)
	await _settle()
	_check(not page.canvas.hand_collapsed and page.canvas.card_bounds(cards[0]).has_area(), "Right click expands the whole hand again")
	page._show_detail(cards[0])
	await _settle()
	_mouse_button(page._hand_button.get_global_rect().get_center(), true)
	_mouse_button(page._hand_button.get_global_rect().get_center(), false)
	await _settle()
	_check(page.canvas.hand_collapsed and page.canvas.selected_id.is_empty() and not page._detail.visible and not page._actions.visible, "Hand button collapses every card and closes the selected card UI")
	_mouse_button(page._hand_button.get_global_rect().get_center(), true)
	_mouse_button(page._hand_button.get_global_rect().get_center(), false)
	await _settle()
	_check(not page.canvas.hand_collapsed and page._hand_button.text == "收起手牌", "Hand button restores the fan")

func _on_confirmed(generation: String, revision: String, decision_id: String, selected: Array[String]) -> void:
	_confirmed = [generation, revision, decision_id, selected]

func _on_cancelled(_generation: String, _revision: String, _decision_id: String) -> void:
	_cancelled = true

func _settings_input_checks(settings: SettingsPage) -> void:
	var previous_volume: float = settings._master.value
	settings._master.grab_focus()
	var arrow: InputEventKey = InputEventKey.new()
	arrow.keycode = KEY_LEFT
	arrow.pressed = true
	root.push_input(arrow, true)
	await _settle()
	_check(settings._master.value == maxf(0, previous_volume - 1), "Gem slider keeps keyboard adjustment")
	arrow.pressed = false
	root.push_input(arrow, true)
	settings._master.value = previous_volume
	var previous_toggle: bool = settings._reduced.button_pressed
	var point: Vector2 = settings._reduced.get_global_rect().get_center()
	_mouse_button(point, true)
	_mouse_button(point, false)
	await _settle()
	_check(settings._reduced.button_pressed != previous_toggle, "Generated toggle changes state through pointer input")
	settings._reduced.button_pressed = previous_toggle
	point = settings._resolution.get_global_rect().get_center()
	_mouse_button(point, true)
	_mouse_button(point, false)
	await _settle()
	_check(settings._resolution.get_popup().visible, "Ornate dropdown opens the native resolution list")
	if root.size == Vector2i(1920, 1080):
		await _screenshot("settings-dropdown-1920x1080")
	settings._resolution.get_popup().hide()
	await _settle()

func _frame_preview() -> void:
	_app.hide()
	var preview: Control = Control.new()
	root.add_child(preview)
	preview.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var backing: ColorRect = ColorRect.new()
	backing.color = Color("08131f")
	preview.add_child(backing)
	backing.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var heading: Label = UiFactory.label("WizardCard · 原始卡框接入预览", 36)
	heading.position = Vector2(84, 160)
	preview.add_child(heading)
	var subtitle: Label = UiFactory.label("Godot 实际渲染 · 五种完整框体与稀有度宝石 · 以下为美术排版示例", 20)
	subtitle.position = Vector2(84, 225)
	preview.add_child(subtitle)
	var kinds: Array[String] = ["analysis", "talisman", "incantation", "formation", "rune"]
	var labels: Array[String] = ["解析", "咒符", "言灵", "阵法", "符文"]
	var rarities: Array[String] = ["common", "uncommon", "rare", "legendary", "epic"]
	for index: int in range(kinds.size()):
		var sample: CardPresentation = _app.service.cards[index].duplicate() as CardPresentation
		sample.type_key = kinds[index]
		sample.type_label = labels[index]
		sample.title = labels[index] + "卡框示例"
		sample.text = "保留原始金属纹饰与羊皮纸面板。此卡仅展示美术排版，卡池数据保持不变。"
		sample.rarity = rarities[index]
		var face: CardFace = CardFace.new()
		preview.add_child(face)
		face.position = Vector2(84 + index * 355, 300)
		face.size = Vector2(330, 440)
		face.configure(sample)
		var caption: Label = UiFactory.label(labels[index] + " · " + rarities[index], 20)
		caption.position = face.position + Vector2(18, 470)
		preview.add_child(caption)
		_check(ArtBank.texture(CardPainter.frame_id(sample)) != null, "Every original frame renders: " + kinds[index])
	await _settle()
	await _screenshot("card-frame-types-1920x1080")
	preview.queue_free()
	_app.show()
	await _settle()

func _screenshot(name: String) -> void:
	if not _capture or _output.is_empty():
		return
	await RenderingServer.frame_post_draw
	var image: Image = root.get_texture().get_image()
	_check(not image.is_empty(), "Rendered capture cannot be empty")
	var result: Error = image.save_png(_output.path_join(name + ".png"))
	_check(result == OK, "Save rendered capture " + name)
