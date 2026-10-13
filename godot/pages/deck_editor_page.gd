class_name DeckEditorPage
extends Control

signal navigation_requested(page: StringName)

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
var _dialog: DeckDialog
var _dialog_kind: String = ""
var _leave_quits: bool = false
var _pool_page: int = 0
var _pool_ids: Array[String] = []
var _building: bool = false

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
	_title = _label("卡组构筑", 21)
	_title.custom_minimum_size.x = 118
	header.add_child(_title)
	_name = LineEdit.new()
	_name.placeholder_text = "卡组名称"
	_name.max_length = 80
	_name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_name.custom_minimum_size.y = 36
	header.add_child(_name)
	_state = _label("尚未保存")
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
	_main_count = _label("主卡组  0 / 40", 17)
	built_column.add_child(_main_count)
	_built = _grid("cards", 10, 4)
	_built.size_flags_stretch_ratio = 4.0
	built_column.add_child(_built)
	_side_count = _label("副卡组  0 / 10", 17)
	built_column.add_child(_side_count)
	_side = _grid("sideCards", 10, 1)
	built_column.add_child(_side)
	_initial = _label("初始阵法在对局开始时从副卡组选择", 13)
	_initial.add_theme_color_override(&"font_color", DeckTheme.MUTED)
	built_column.add_child(_initial)
	_pool_panel = Panel.new()
	_pool_panel.custom_minimum_size.x = 320
	body.add_child(_pool_panel)
	var pool_column: VBoxContainer = _panel_column(_pool_panel)
	var pool_header: HBoxContainer = HBoxContainer.new()
	pool_column.add_child(pool_header)
	_pool_title = _label("卡池", 17)
	_pool_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	pool_header.add_child(_pool_title)
	_reset = _button("重置")
	pool_header.add_child(_reset)
	_search = LineEdit.new()
	_search.placeholder_text = "搜索卡名（部分匹配）"
	_search.custom_minimum_size.y = 34
	pool_column.add_child(_search)
	var filters: GridContainer = GridContainer.new()
	filters.columns = 2
	filters.add_theme_constant_override(&"h_separation", 6)
	filters.add_theme_constant_override(&"v_separation", 6)
	pool_column.add_child(filters)
	for caption: String in ["类型", "稀有度", "费用", "施法", "位阶", "标签"]:
		var filter: OptionButton = OptionButton.new()
		filter.set_meta(&"caption", caption)
		filter.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		filter.custom_minimum_size = Vector2(0, 30)
		DeckTheme.field(filter)
		filter.add_theme_font_size_override(&"font_size", 12)
		filters.add_child(filter)
		_filters.append(filter)
	_pool_hint = _label("右键加入 · 拖入对应卡组", 12)
	_pool_hint.add_theme_color_override(&"font_color", DeckTheme.MUTED)
	pool_column.add_child(_pool_hint)
	_pool = _grid("cards", 3, 3, "pool")
	pool_column.add_child(_pool)
	var pool_pagination: HBoxContainer = HBoxContainer.new()
	pool_column.add_child(pool_pagination)
	_pool_prev = _button("上一页")
	pool_pagination.add_child(_pool_prev)
	_pool_page_label = _label("1 / 1", 13)
	_pool_page_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_pool_page_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	pool_pagination.add_child(_pool_page_label)
	_pool_next = _button("下一页")
	pool_pagination.add_child(_pool_next)
	var footer: HBoxContainer = HBoxContainer.new()
	page_column.add_child(footer)
	_save = _button("保存卡组 / 草稿")
	_save.custom_minimum_size.x = 180
	footer.add_child(_save)
	_notice = _button("构筑检查")
	_notice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_notice.clip_text = true
	footer.add_child(_notice)
	_back = _button("返回卡组列表")
	_back.custom_minimum_size.x = 160
	footer.add_child(_back)

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

func _build_filters() -> void:
	_filter_values = [[null, 1, 2, 3, 4, 5], [null, "common", "uncommon", "rare", "epic", "legendary"], [null], [null], [null], [null]]
	var types: Array[String] = ["全部", "解析", "言灵", "阵法", "符文", "咒符"]
	var rarities: Array[String] = ["全部", "普通", "罕见", "稀有", "史诗", "传说"]
	var tags: Array[String] = []
	for definition: Dictionary in service.definitions.values():
		for tag: String in definition.get("tags", []):
			if not tags.has(tag):
				tags.append(tag)
	tags.sort()
	for number: int in range(8):
		_filter_values[2].append(number)
		_filter_values[3].append(number)
		_filter_values[4].append(number)
	_filter_values[5].append_array(tags)
	for index: int in range(6):
		var filter: OptionButton = _filters[index]
		filter.clear()
		var caption: String = str(filter.get_meta(&"caption"))
		for choice: int in range(_filter_values[index].size()):
			var label: String = "全部" if choice == 0 else str(_filter_values[index][choice])
			if index == 0:
				label = types[choice]
			elif index == 1:
				label = rarities[choice]
			filter.add_item(caption + "：" + label)

func _query_changed(_text: String) -> void:
	if _building or service == null:
		return
	var query: Dictionary = {"name": _search.text}
	var keys: Array[String] = ["type", "rarity", "cost", "castCost", "rank", "tags"]
	for index: int in range(6):
		var selected: int = _filters[index].selected
		if selected > 0:
			var selected_value: Variant = _filter_values[index][selected]
			query[keys[index]] = [selected_value] if index == 5 else selected_value
	_pool_ids = service.query_cards(query)
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
	_main_count.text = "主卡组  %d / 40" % DeckUi.total(main)
	_side_count.text = "副卡组  %d / 10" % DeckUi.total(side)
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
	_state.text = "尚未保存" if service.draft_dirty() else "已保存"
	_save.text = "保存卡组 / 草稿 *" if service.draft_dirty() else "已保存 · 再次保存"
	_save.disabled = not service.writable
	var problems: Array[String] = service.draft_problems()
	_notice.text = "构筑检查通过" if problems.is_empty() else problems[0] + " · 查看详情"
	_notice.tooltip_text = "\n".join(problems)
	_refresh_pool()

func _refresh_pool() -> void:
	_pool_page = clampi(_pool_page, 0, maxi(0, ceili(_pool_ids.size() / 9.0) - 1))
	_pool.ids.assign(_pool_ids.slice(_pool_page * 9, (_pool_page + 1) * 9))
	_pool.queue_redraw()
	_pool_title.text = "卡池 · %d 种" % _pool_ids.size()
	_pool_prev.disabled = _pool_page == 0
	_pool_next.disabled = (_pool_page + 1) * 9 >= _pool_ids.size()
	_pool_page_label.text = "%d / %d" % [_pool_page + 1, maxi(1, ceili(_pool_ids.size() / 9.0))]
	if _pool_ids.is_empty():
		_pool_hint.text = service.error if not service.error.is_empty() else "无匹配卡牌，请调整筛选"
	else:
		_pool_hint.text = "阵法入副卡组 · 右键或拖入"

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
		_notice.text = "已达到卡组或同名份数上限"
		return
	if count + delta <= 0:
		cards.erase(id)
	else:
		cards[id] = count + delta
	service.editing_draft[membership] = cards
	_refresh()
	_inspect(id)

func _save_draft() -> bool:
	if not service.save_draft():
		_notice.text = service.error
		return false
	_refresh()
	_notice.text = "卡组已保存" if service.draft_problems().is_empty() else "草稿已保存，请补齐主／副卡组"
	return true

func _show_problems() -> void:
	_dialog_kind = "problems"
	var problems: Array[String] = service.draft_problems()
	_dialog.present("构筑检查", "构筑检查通过。" if problems.is_empty() else "\n\n".join(problems), ["关闭"])

func request_leave(quit_game: bool = false) -> void:
	if _dialog.visible:
		_dialog.hide()
		return
	_leave_quits = quit_game
	if service.draft_dirty():
		_dialog_kind = "leave"
		_dialog.present("尚有未保存的修改", "保存草稿后离开，或放弃本次修改。", ["保存并离开", "放弃修改", "继续编辑"])
	else:
		_finish_leave()

func _dialog_chosen(index: int) -> void:
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
