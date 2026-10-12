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
var _pool: DeckCardGrid
var _tabs: Array[Button] = []
var _built_prev: Button
var _built_next: Button
var _built_page_label: Label
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
var _section: String = "cards"
var _built_page: int = 0
var _pool_page: int = 0
var _pool_ids: Array[String] = []
var _building: bool = false

func _ready() -> void:
	_title = %Title
	_state = %State
	_name = %Name
	_initial = %InitialNotice
	_detail = %Detail
	_built_panel = %BuiltPanel
	_pool_panel = %PoolPanel
	_built = %Built
	_pool = %Pool
	_tabs.assign([%MainTab, %SideTab])
	_built_prev = %BuiltPrevious
	_built_next = %BuiltNext
	_built_page_label = %BuiltPosition
	_pool_title = %PoolTitle
	_search = %Search
	_filters.assign([%Filter0, %Filter1, %Filter2, %Filter3, %Filter4, %Filter5])
	_reset = %Reset
	_pool_hint = %PoolHint
	_pool_prev = %PoolPrevious
	_pool_next = %PoolNext
	_pool_page_label = %PoolPosition
	_save = %Save
	_back = %Back
	_notice = %Notice
	_dialog = %Dialog
	DeckUi.compact_field(_name, 20)
	DeckUi.compact_field(_search)
	for filter: OptionButton in _filters:
		DeckUi.compact_field(filter, 12)
		filter.item_selected.connect(_filter_changed)
	_name.text_changed.connect(_rename)
	_built.inspected.connect(_inspect)
	_built.adjusted.connect(_adjust)
	_pool.inspected.connect(_inspect)
	_pool.adjusted.connect(_adjust)
	_pool.origin = "pool"
	_pool.columns = 3
	_pool.rows = 3
	_tabs[0].pressed.connect(_choose_section.bind("cards"))
	_tabs[1].pressed.connect(_choose_section.bind("sideCards"))
	_built_prev.pressed.connect(_turn_built.bind(-1))
	_built_next.pressed.connect(_turn_built.bind(1))
	_pool_prev.pressed.connect(_turn_pool.bind(-1))
	_pool_next.pressed.connect(_turn_pool.bind(1))
	_reset.pressed.connect(_reset_query)
	_search.text_changed.connect(_query_changed)
	_save.pressed.connect(_save_draft)
	_back.pressed.connect(request_leave)
	_notice.pressed.connect(_show_problems)
	_dialog.chosen.connect(_dialog_chosen)
	get_window().focus_exited.connect(_cancel_drag)

func _cancel_drag() -> void:
	if get_viewport().gui_is_dragging():
		get_viewport().gui_cancel_drag()

func configure(value: FrontendService) -> void:
	service = value
	_detail.service = value
	_built.service = value
	_pool.service = value
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
	_tabs[0].text = "主卡组  %d / 40" % DeckUi.total(main)
	_tabs[1].text = "副卡组  %d / 10" % DeckUi.total(side)
	_tabs[0].add_theme_color_override(&"font_color", Color("75ecf5") if _section == "cards" else CardPainter.PAPER)
	_tabs[1].add_theme_color_override(&"font_color", Color("75ecf5") if _section == "sideCards" else CardPainter.PAPER)
	var ids: Array[String] = []
	var membership: Dictionary = service.editing_draft.get(_section, {})
	var ordered: Array = membership.keys()
	ordered.sort()
	for id: String in ordered:
		for copy: int in range(int(membership[id])):
			ids.append(id)
	var capacity: int = 30
	_built_page = clampi(_built_page, 0, maxi(0, ceili(ids.size() / float(capacity)) - 1))
	_built.ids.assign(ids.slice(_built_page * capacity, (_built_page + 1) * capacity))
	_built.section = _section
	# Keep excess legacy cards visible so the player can repair an overfull draft.
	_built.slot_count = maxi(10, _built.ids.size()) if _section == "sideCards" else 30
	_built.queue_redraw()
	_built_prev.disabled = _built_page == 0
	_built_next.disabled = (_built_page + 1) * capacity >= ids.size()
	_built_page_label.text = "%d / %d" % [_built_page + 1, maxi(1, ceili(ids.size() / float(capacity)))]
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

func _choose_section(section: String) -> void:
	_section = section
	_built_page = 0
	_refresh()

func _turn_built(delta: int) -> void:
	_built_page += delta
	_refresh()

func _turn_pool(delta: int) -> void:
	_pool_page += delta
	_refresh_pool()

func _inspect(id: String) -> void:
	_detail.inspect(id)
	_built.selected_id = id
	_pool.selected_id = id
	_built.queue_redraw()
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
	_section = membership
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
