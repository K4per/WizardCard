class_name DeckEditorPage
extends Control

signal navigation_requested(page: StringName)

signal lesson_completed(id: String, order: int)
signal lesson_navigation_requested(action: StringName, id: String, order: int)

const EditorOverlay = preload("res://ui/tutorial_overlay.gd")
const EditorEnd = preload("res://ui/lesson_end_panel.gd")
var lesson: Dictionary = {}
var _overlay: EditorOverlay
var _end: EditorEnd
var _step := ""
var _skip: Button

var service: FrontendService
var _title: Label
var _state: Label
var _name: LineEdit
var _initial: Label
var _detail: DeckDetailPane
var _built_panel: Panel
var _pool_panel: Panel
var _built: DeckCardGrid
var _side: DeckCardGrid
var _pool: DeckCardGrid
var _main_count: Label
var _side_count: Label
var _pool_title: Label
var _search: LineEdit
var _filters: Array[OptionButton] = []
var _filter_values: Array[Array] = []
var _reset: Button
var _pool_hint: Label
var _pool_prev: Button
var _pool_next: Button
var _pool_page_label: Label
var _save: Button
var _back: Button
var _notice: Button
var _storage_picker: FileDialog
var _storage_button: Button
var _dialog: DeckDialog
var _dialog_kind: String = ""
var _leave_quits: bool = false
var _pool_page: int = 0
var _pool_ids: Array[String] = []
var _building: bool = false
var _export_dialog: FileDialog

func _ready() -> void:
	theme = DeckTheme.create()
	_build_interface()
	_dialog = %Dialog
	_dialog.move_to_front()
	for filter: OptionButton in _filters:
		filter.item_selected.connect(_filter_changed)
	_name.text_changed.connect(_rename)
	for grid: DeckCardGrid in [_built, _side, _pool]:
		grid.inspected.connect(_inspect)
		grid.adjusted.connect(_adjust)
	_pool_prev.pressed.connect(_turn_pool.bind(-1))
	_pool_next.pressed.connect(_turn_pool.bind(1))
	_reset.pressed.connect(_reset_query)
	_search.text_changed.connect(_query_changed)
	_save.pressed.connect(_save_draft)
	_back.pressed.connect(request_leave)
	_notice.pressed.connect(_show_problems)
	_dialog.chosen.connect(_dialog_chosen)
	get_window().focus_exited.connect(_cancel_drag)
	_pool.resized.connect(_reflow_pool)

func _label(text: String, font_size: int = 15) -> Label:
	var label: Label = Label.new()
	label.text = text
	label.add_theme_font_size_override(&"font_size", font_size)
	return label

func _button(text: String) -> Button:
	var button: Button = Button.new()
	button.text = text
	DeckTheme.button(button)
	return button

func _panel_column(panel: Panel) -> VBoxContainer:
	var margin: MarginContainer = MarginContainer.new()
	panel.add_child(margin)
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side: StringName in [&"margin_left", &"margin_top", &"margin_right", &"margin_bottom"]:
		margin.add_theme_constant_override(side, 10)
	var column: VBoxContainer = VBoxContainer.new()
	margin.add_child(column)
	return column

func _grid(section: String, columns: int, rows: int, origin: String = "built") -> DeckCardGrid:
	var grid: DeckCardGrid = DeckCardGrid.new()
	grid.section = section
	grid.columns = columns
	grid.rows = rows
	grid.slot_count = columns * rows
	grid.origin = origin
	grid.size_flags_vertical = Control.SIZE_EXPAND_FILL
	grid.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return grid

func _build_interface() -> void:
	var background: ColorRect = ColorRect.new()
	background.color = DeckTheme.BACKGROUND
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(background)
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var margin: MarginContainer = MarginContainer.new()
	add_child(margin)
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side: StringName in [&"margin_left", &"margin_top", &"margin_right", &"margin_bottom"]:
		margin.add_theme_constant_override(side, 12)
	var page_column: VBoxContainer = VBoxContainer.new()
	margin.add_child(page_column)
	var header: HBoxContainer = HBoxContainer.new()
	page_column.add_child(header)
	_title = _label(GameText.label("ui.deck.editor"), 21)
	_title.custom_minimum_size.x = 118
	header.add_child(_title)
	_name = LineEdit.new()
	_name.placeholder_text = GameText.label("ui.deck.name")
	_name.max_length = 80
	_name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_name.custom_minimum_size.y = 36
	header.add_child(_name)
	_state = _label(GameText.label("ui.deck.unsaved"))
	_state.custom_minimum_size.x = 88
	_state.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	header.add_child(_state)
	var body: HBoxContainer = HBoxContainer.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	page_column.add_child(body)
	_detail = (load("res://ui/deck_detail_pane.tscn") as PackedScene).instantiate() as DeckDetailPane
	_detail.custom_minimum_size.x = 250
	body.add_child(_detail)
	_built_panel = Panel.new()
	_built_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_child(_built_panel)
	var built_column: VBoxContainer = _panel_column(_built_panel)
	_main_count = _label(GameText.label("ui.deck.main_empty"), 17)
	built_column.add_child(_main_count)
	_built = _grid("cards", 10, 4)
	_built.size_flags_stretch_ratio = 4.0
	built_column.add_child(_built)
	_side_count = _label(GameText.label("ui.deck.side_empty"), 17)
	built_column.add_child(_side_count)
	_side = _grid("sideCards", 10, 1)
	built_column.add_child(_side)
	_initial = _label(GameText.label("ui.deck.initial_from_side"), 13)
	_initial.add_theme_color_override(&"font_color", DeckTheme.MUTED)
	built_column.add_child(_initial)
	var rules := _label(get_node("/root/Text").t("tutorial.l08.s03"), 13)
	rules.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	built_column.add_child(rules)
	_pool_panel = Panel.new()
	_pool_panel.custom_minimum_size.x = 320
	body.add_child(_pool_panel)
	var pool_column: VBoxContainer = _panel_column(_pool_panel)
	var pool_header: HBoxContainer = HBoxContainer.new()
	pool_column.add_child(pool_header)
	_pool_title = _label(GameText.label("ui.deck.pool"), 17)
	_pool_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	pool_header.add_child(_pool_title)
	_reset = _button(GameText.label("ui.deck.reset"))
	pool_header.add_child(_reset)
	_search = LineEdit.new()
	_search.placeholder_text = GameText.label("ui.deck.search")
	_search.custom_minimum_size.y = 34
	pool_column.add_child(_search)
	var filters: GridContainer = GridContainer.new()
	filters.columns = 2
	filters.add_theme_constant_override(&"h_separation", 6)
	filters.add_theme_constant_override(&"v_separation", 6)
	pool_column.add_child(filters)
	for caption: String in [GameText.label("ui.deck.type"), GameText.label("ui.deck.rarity"), GameText.label("ui.deck.cost"), GameText.label("ui.deck.cast"), GameText.label("ui.deck.rank"), GameText.label("ui.deck.faction"), GameText.label("ui.deck.speed"), GameText.label("ui.deck.damage_type"), GameText.label("ui.deck.tags")]:
		var filter: OptionButton = OptionButton.new()
		filter.set_meta(&"caption", caption)
		filter.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		filter.custom_minimum_size = Vector2(0, 30)
		DeckTheme.field(filter)
		filter.add_theme_font_size_override(&"font_size", 12)
		filters.add_child(filter)
		_filters.append(filter)
	_pool_hint = _label(GameText.label("ui.deck.edit_hint"), 12)
	_pool_hint.add_theme_color_override(&"font_color", DeckTheme.MUTED)
	pool_column.add_child(_pool_hint)
	_pool = _grid("cards", 3, 3, "pool")
	pool_column.add_child(_pool)
	var pool_pagination: HBoxContainer = HBoxContainer.new()
	pool_column.add_child(pool_pagination)
	_pool_prev = _button(GameText.label("ui.navigation.previous_page"))
	pool_pagination.add_child(_pool_prev)
	_pool_page_label = _label("1 / 1", 13)
	_pool_page_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_pool_page_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	pool_pagination.add_child(_pool_page_label)
	_pool_next = _button(GameText.label("ui.navigation.next_page"))
	pool_pagination.add_child(_pool_next)
	var footer: HBoxContainer = HBoxContainer.new()
	page_column.add_child(footer)
	_save = _button(GameText.label("ui.deck.save_draft"))
	_save.custom_minimum_size.x = 180
	footer.add_child(_save)
	_notice = _button(GameText.label("ui.deck.validate"))
	_notice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_notice.clip_text = true
	footer.add_child(_notice)
	_storage_picker = FileDialog.new()
	_storage_picker.title = GameText.label("ui.deck.storage_dir")
	_storage_picker.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	_storage_picker.access = FileDialog.ACCESS_FILESYSTEM
	_storage_picker.dir_selected.connect(_deck_storage_chosen)
	add_child(_storage_picker)
	_storage_button = UiFactory.button(GameText.label("ui.deck.storage_dir"), func() -> void:
		_storage_picker.current_dir = service.save_directory
		_storage_picker.popup_centered_ratio(0.7))
	_storage_button.hide()
	footer.add_child(_storage_button)
	_back = _button(GameText.label("ui.deck.back_to_decks"))
	_back.custom_minimum_size.x = 160
	footer.add_child(_back)
	_skip = _button(get_node("/root/Text").t("ui.lesson.skip"))
	_skip.hide()
	_skip.pressed.connect(_lesson_action.bind(&"skip"))
	footer.add_child(_skip)
	var export_button: Button = _button(GameText.label("ui.deck.export"))
	footer.add_child(export_button)
	_export_dialog = FileDialog.new()
	_export_dialog.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	_export_dialog.access = FileDialog.ACCESS_FILESYSTEM
	_export_dialog.filters = PackedStringArray([GameText.label("ui.deck.export_filter")])
	_export_dialog.current_file = "WizardCard-deck.json"
	_export_dialog.title = GameText.label("ui.deck.export")
	add_child(_export_dialog)
	export_button.pressed.connect(func() -> void: _export_dialog.popup_centered_ratio(0.7))
	_export_dialog.file_selected.connect(_export_deck)

func _export_deck(path: String) -> void:
	if service.export_deck(path):
		_dialog.present(GameText.label("ui.deck.export_done"), GameText.label("ui.deck.export_path", {"path": path}), [GameText.label("ui.common.ok")])
	else:
		_dialog.present(GameText.label("ui.deck.export_failed"), service.error, [GameText.label("ui.common.ok")])

func _cancel_drag() -> void:
	if get_viewport().gui_is_dragging():
		get_viewport().gui_cancel_drag()

func configure(value: FrontendService) -> void:
	service = value
	_detail.service = value
	for grid: DeckCardGrid in [_built, _side, _pool]:
		grid.service = value
	_building = true
	_name.text = str(value.editing_draft.get("name", ""))
	_build_filters()
	_building = false
	_query_changed("")
	_refresh()
	if not _pool_ids.is_empty():
		_inspect(_pool_ids[0])
	if not lesson.is_empty():
		_overlay = preload("res://ui/tutorial_overlay.tscn").instantiate() as EditorOverlay
		add_child(_overlay)
		_overlay.continue_requested.connect(_editor_step.bind("rules"))
		_end = preload("res://ui/lesson_end_panel.tscn").instantiate() as EditorEnd
		add_child(_end)
		_end.action_requested.connect(_lesson_action)
		_skip.show()
		_side.inspected.connect(func(_id: String) -> void: _editor_step("side"))
		_initial.text = get_node("/root/Text").t("tutorial.editor.side_detail", {"side": get_node("/root/Text").t(lesson.steps.side.hintKey), "formation": get_node("/root/Text").t(lesson.steps.sideEmpty.hintKey)})
		_initial.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		_editor_step("open")

func _lesson_action(action: StringName) -> void:
	lesson_navigation_requested.emit(action, str(lesson.id), int(lesson.order))

func _editor_rects(id: String) -> Array[Rect2]:
	var control: Control = {"ui:save": _save, "ui:main": _main_count, "ui:side": _side_count, "ui:pool": _pool}.get(id)
	if control == null or not control.is_visible_in_tree():
		return []
	return [control.get_global_transform_with_canvas() * Rect2(Vector2.ZERO, control.size)]

func _editor_step(event: String) -> void:
	if lesson.is_empty() or _overlay == null or _step == "complete":
		return
	_step = event
	var step: Dictionary = lesson.steps[event].duplicate(true)
	step["canContinue"] = event in ["open", "draft", "side", "sideEmpty"]
	_overlay.configure(step, _editor_rects, [_dialog._panel])
	if event == "complete":
		_overlay.hide()
		_skip.hide()
		var catalog := get_node("/root/Text")
		_end.present(str(catalog.t(lesson.steps.saved.hintKey)) + "\n" + str(catalog.t(step.hintKey)), true, true)
		lesson_completed.emit(str(lesson.id), int(lesson.order))


func _build_filters() -> void:
	_filter_values = [[null, 1, 2, 3, 4, 5], [null, "common", "uncommon", "rare", "epic", "legendary"], [null], [null], [null], [null], [null], [null], [null]]
	var types: Array[String] = [GameText.label("ui.deck.all"), GameText.label("terms.card_type.analytic_spell"), GameText.label("terms.card_type.word_spell"), GameText.label("terms.card_type.formation_card"), GameText.label("terms.card_type.rune_card"), GameText.label("terms.card_type.talisman_spell")]
	var rarities: Array[String] = [GameText.label("ui.deck.all"), GameText.label("terms.rarity.common"), GameText.label("terms.rarity.uncommon"), GameText.label("terms.rarity.rare"), GameText.label("terms.rarity.epic"), GameText.label("terms.rarity.legendary")]
	var factions: Array[String] = []
	var speeds: Array[String] = []
	var damage_types: Array[String] = []
	var tags: Array[String] = []
	for key: String in service.filter_term_values:
		if key.begins_with("terms.school."):
			factions.append(key)
		elif key.begins_with("terms.damage."):
			damage_types.append(key)
	var native_term_values: Array = service.filter_term_values.values()
	for definition: Dictionary in service.definitions.values():
		for tag: String in definition.get("tags", []):
			if native_term_values.has(tag):
				continue
			# Speed tags are supplied by the legacy CardQuery contract.
			if tag.ends_with("速"):
				if not speeds.has(tag):
					speeds.append(tag)
			elif tag != "former-action" and not tags.has(tag):
				tags.append(tag)
	factions.sort_custom(func(a: String, b: String) -> bool: return GameText.label(a) < GameText.label(b))
	speeds.sort()
	damage_types.sort_custom(func(a: String, b: String) -> bool: return GameText.label(a) < GameText.label(b))
	tags.sort()
	for number: int in range(8):
		_filter_values[2].append(number)
		_filter_values[3].append(number)
		_filter_values[4].append(number)
	_filter_values[5].append_array(factions)
	_filter_values[6].append_array(speeds)
	_filter_values[7].append_array(damage_types)
	_filter_values[8].append_array(tags)
	for index: int in range(_filters.size()):
		var filter: OptionButton = _filters[index]
		filter.clear()
		var caption: String = str(filter.get_meta(&"caption"))
		for choice: int in range(_filter_values[index].size()):
			var label: String = GameText.label("ui.deck.all") if choice == 0 else str(_filter_values[index][choice])
			if index == 0:
				label = types[choice]
			elif index == 1:
				label = rarities[choice]
			if index in [5, 7] and choice > 0:
				label = GameText.label(label)
			filter.add_item(caption + "：" + label)

func _query_changed(_text: String) -> void:
	if _building or service == null:
		return
	var query: Dictionary = {"name": _search.text}
	var keys: Array[String] = ["type", "rarity", "cost", "castCost", "rank", "faction", "speed", "damageType", "tags"]
	for index: int in range(_filters.size()):
		var selected: int = _filters[index].selected
		if selected > 0:
			var selected_value: Variant = _filter_values[index][selected]
			if index in [5, 7]:
				selected_value = service.filter_term_values[selected_value]
			query[keys[index]] = [selected_value] if index in [5, 6, 7, 8] else selected_value
	_pool_ids = service.query_cards(query)
	if not lesson.is_empty():
		var available: Array = lesson.candidates.duplicate()
		# Removed formations stay available so an incomplete side deck can be repaired.
		for id: String in lesson.sideCards:
			if int(service.editing_draft.sideCards.get(id, 0)) < int(lesson.sideCards[id]):
				available.append(id)
		_pool_ids = _pool_ids.filter(func(id: String) -> bool: return available.has(id))
	_pool_page = 0
	_refresh_pool()

func _filter_changed(_index: int) -> void:
	_query_changed(_search.text)

func _reset_query() -> void:
	_building = true
	_search.text = ""
	for filter: OptionButton in _filters:
		filter.select(0)
	_building = false
	_query_changed("")

func _refresh() -> void:
	var main: Dictionary = service.editing_draft.get("cards", {})
	var side: Dictionary = service.editing_draft.get("sideCards", {})
	var main_size := DeckUi.total(main)
	var side_size := DeckUi.total(side)
	_main_count.text = GameText.label("ui.deck.main_count", {"count": main_size})
	_side_count.text = GameText.label("ui.deck.side_count", {"count": side_size})
	if not lesson.is_empty():
		_main_count.add_theme_color_override(&"font_color", Color("f1aaa0") if main_size != 40 else DeckTheme.TEXT)
		_side_count.add_theme_color_override(&"font_color", Color("f1aaa0") if side_size != 10 else DeckTheme.TEXT)
	for grid: DeckCardGrid in [_built, _side]:
		var membership: Dictionary = service.editing_draft.get(grid.section, {})
		var ordered: Array = membership.keys()
		ordered.sort()
		grid.ids.clear()
		for id: String in ordered:
			for copy: int in range(int(membership[id])):
				grid.ids.append(id)
		# Excess legacy cards remain visible for repair in the same fixed section.
		grid.rows = maxi(4 if grid == _built else 1, ceili(grid.ids.size() / 10.0))
		grid.slot_count = grid.rows * grid.columns
		grid.queue_redraw()
	_state.text = GameText.label("ui.deck.unsaved") if service.draft_dirty() else GameText.label("ui.deck.saved")
	_save.text = GameText.label("ui.deck.save_dirty") if service.draft_dirty() else GameText.label("ui.deck.save_again")
	_save.disabled = not service.writable
	_storage_button.visible = service.can_choose_save_directory and not service.writable
	var problems: Array[String] = service.draft_problems()
	_notice.text = GameText.label("ui.deck.valid") if problems.is_empty() else GameText.label("ui.deck.details", {"problem": problems[0]})
	_notice.tooltip_text = "\n".join(problems)
	if not service.writable:
		_notice.text = service.save_directory + " · " + service.notice
		_notice.tooltip_text = _notice.text
	_refresh_pool()

func _refresh_pool() -> void:
	var page_size: int = _pool.columns * _pool.rows
	_pool_page = clampi(_pool_page, 0, maxi(0, ceili(float(_pool_ids.size()) / page_size) - 1))
	_pool.ids.assign(_pool_ids.slice(_pool_page * page_size, (_pool_page + 1) * page_size))
	_pool.queue_redraw()
	_pool_title.text = GameText.label("ui.deck.pool_count", {"count": _pool_ids.size()})
	_pool_prev.disabled = _pool_page == 0
	_pool_next.disabled = (_pool_page + 1) * page_size >= _pool_ids.size()
	_pool_page_label.text = "%d / %d" % [_pool_page + 1, maxi(1, ceili(float(_pool_ids.size()) / page_size))]
	if _pool_ids.is_empty():
		_pool_hint.text = service.error if not service.error.is_empty() else GameText.label("ui.deck.no_matches")
	else:
		_pool_hint.text = get_node("/root/Text").t("tutorial.editor.pick") if not lesson.is_empty() else GameText.label("ui.deck.pool_hint")

func _reflow_pool() -> void:
	var next_rows: int = clampi(floori(_pool.size.y / 115.0), 1, 3)
	if next_rows == _pool.rows:
		return
	var first_index: int = _pool_page * _pool.columns * _pool.rows
	_pool.rows = next_rows
	_pool.slot_count = _pool.columns * next_rows
	_pool_page = first_index / _pool.slot_count
	if service != null:
		_refresh_pool()

func _rename(text: String) -> void:
	if service != null:
		service.editing_draft["name"] = text
		_refresh()

func _turn_pool(delta: int) -> void:
	_pool_page += delta
	_refresh_pool()

func _inspect(id: String) -> void:
	_detail.inspect(id)
	_built.selected_id = id
	_side.selected_id = id
	_pool.selected_id = id
	_built.queue_redraw()
	_side.queue_redraw()
	_pool.queue_redraw()

func _adjust(id: String, delta: int, source_section: String) -> void:
	var membership: String = source_section
	if delta > 0:
		if not service.definitions.has(id):
			return
		membership = "sideCards" if int(service.definitions[id].get("type", -1)) == 3 else "cards"
	var cards: Dictionary = service.editing_draft.get(membership, {})
	var count: int = int(cards.get(id, 0))
	if delta > 0 and (service.named_copies(id, membership) >= 3 or DeckUi.total(cards) >= (10 if membership == "sideCards" else 40)):
		if not lesson.is_empty() and service.named_copies(id, membership) >= 3:
			_editor_step("limit")
		_notice.text = GameText.label("ui.deck.limit_reached")
		UiFactory.flash(_notice, Color("f1aaa0"))
		return
	if delta < 0 and count == 0:
		return
	if count + delta <= 0:
		cards.erase(id)
	else:
		cards[id] = count + delta
	service.editing_draft[membership] = cards
	if not lesson.is_empty():
		_editor_step("side" if membership == "sideCards" else "added")
		if membership == "sideCards":
			_query_changed(_search.text)
	_refresh()
	_inspect(id)
	_notice.text = GameText.label("ui.deck.adjustment", {"card": service.card_by_id(id).title, "action": GameText.label("ui.deck.added") if delta > 0 else GameText.label("ui.deck.removed"), "count": maxi(0, count + delta)})
	_notice.tooltip_text = _notice.text
	_pool.flash_card(id)
	(_side if membership == "sideCards" else _built).flash_card(id)

func _save_draft() -> bool:
	if not service.save_draft():
		_notice.text = service.save_directory + " · " + service.error
		_notice.tooltip_text = _notice.text
		_storage_button.visible = service.can_choose_save_directory
		UiFactory.flash(_notice, Color("f1aaa0"))
		return false
	_storage_button.hide()
	_refresh()
	_notice.text = GameText.label("ui.deck.saved_complete") if service.draft_problems().is_empty() else GameText.label("ui.deck.saved_incomplete")
	if not lesson.is_empty():
		var stored := false
		for draft: Dictionary in service.drafts:
			if draft.id == service.editing_draft.id:
				stored = bool(draft.get("matchCompatible", false))
				break
		var event := "saved" if stored else ("sideEmpty" if service.editing_draft.sideCards.is_empty() else "draft")
		_notice.text = get_node("/root/Text").t("tutorial.editor.side_empty" if event == "sideEmpty" else lesson.steps[event].hintKey)
		_editor_step("complete" if stored else event)
	UiFactory.flash(_save)
	return true

func _deck_storage_chosen(directory: String) -> void:
	if service.set_save_directory(directory):
		_save_draft()
	else:
		_notice.text = directory + " · " + service.error
		_notice.tooltip_text = _notice.text

func _show_problems() -> void:
	_dialog_kind = "problems"
	var problems: Array[String] = service.draft_problems()
	_dialog.present(GameText.label("ui.deck.validate"), GameText.label("ui.deck.check_passed") if problems.is_empty() else "\n\n".join(problems), [GameText.label("ui.common.close")])

func request_leave(quit_game: bool = false) -> void:
	if _dialog.visible:
		_dialog.hide()
		return
	_leave_quits = quit_game
	if not lesson.is_empty() and _step != "complete":
		_dialog_kind = "lesson_leave"
		var catalog := get_node("/root/Text")
		_dialog.present(catalog.t("tutorial.general.g02"), catalog.t("tutorial.general.g04"),
			[catalog.t("tutorial.general.g04.exit"), catalog.t("tutorial.general.g04.continue")])
		return
	if service.draft_dirty():
		_dialog_kind = "leave"
		_dialog.present(GameText.label("ui.deck.unsaved_changes"), GameText.label("ui.deck.leave_hint"), [GameText.label("ui.deck.save_and_leave"), GameText.label("ui.deck.discard_changes"), GameText.label("ui.deck.continue_editing")])
	else:
		_finish_leave()

func _dialog_chosen(index: int) -> void:
	if _dialog_kind == "lesson_leave":
		if index == 0:
			_finish_leave()
		return
	if _dialog_kind != "leave":
		return
	if index == 0 and _save_draft():
		_finish_leave()
	elif index == 1:
		_finish_leave()

func _finish_leave() -> void:
	if _leave_quits:
		get_tree().quit()
	else:
		navigation_requested.emit(&"decks")
