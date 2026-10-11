class_name DecksPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var _title: Label
var _description: Label
var _new: Button
var _preset: OptionButton
var _copy: Button
var _rows: Array[DeckListRow] = []
var _delete_buttons: Array[Button] = []
var _empty: Label
var _previous: Button
var _next: Button
var _count: Label
var _notice: Label
var _back: Button
var _dialog: DeckDialog
var _delete_id: String = ""
var _page_index: int = 0

func _ready() -> void:
	_title = UiFactory.label("卡组构筑", 32)
	_description = UiFactory.label("管理本地卡组 · 主卡组 40 张 / 阵法副卡组 10 张", 16)
	_new = _button("新建卡组", _create)
	_preset = OptionButton.new()
	DeckUi.compact_field(_preset)
	_copy = _button("从预设复制", _copy_preset)
	_previous = _button("上一页", _turn_page.bind(-1))
	_next = _button("下一页", _turn_page.bind(1))
	_count = UiFactory.label("", 16)
	_count.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_notice = UiFactory.label("", 14)
	_notice.clip_text = true
	_back = _button("返回主菜单", navigation_requested.emit.bind(&"menu"))
	_empty = UiFactory.label("还没有自建卡组。\n新建草稿，或复制预设开始构筑。", 22, true)
	for control: Control in [_title, _description, _new, _preset, _copy, _previous, _next, _count, _notice, _back, _empty]:
		add_child(control)
	for index: int in range(6):
		var row: DeckListRow = DeckListRow.new()
		row.pressed.connect(_edit.bind(index))
		_rows.append(row)
		add_child(row)
		var button: Button = _button("删除卡组", _request_delete.bind(index))
		_delete_buttons.append(button)
		add_child(button)
	_dialog = DeckDialog.new()
	_dialog.chosen.connect(_delete_confirmed)
	add_child(_dialog)
	resized.connect(_reflow)
	_reflow()

func _button(text: String, callback: Callable) -> Button:
	var result: Button = UiFactory.button(text, callback)
	DeckUi.compact_button(result)
	return result

func configure(value: FrontendService) -> void:
	service = value
	service.refresh_library()
	_preset.clear()
	for preset: Dictionary in service.presets:
		_preset.add_item("预设：" + str(preset["name"]))
	_copy.disabled = service.presets.is_empty() or not service.writable
	_new.disabled = not service.writable
	_refresh()

func _reflow() -> void:
	if _rows.is_empty():
		return
	DeckUi.place(_title, Rect2(32, 20, size.x - 64, 48))
	DeckUi.place(_description, Rect2(32, 74, size.x - 64, 28))
	DeckUi.place(_new, Rect2(32, 112, 180, 40))
	DeckUi.place(_preset, Rect2(232, 112, 400, 40))
	DeckUi.place(_copy, Rect2(652, 112, 220, 40))
	var row_y: float = maxf(170, size.y * 0.2)
	var pagination_y: float = size.y - 122
	var row_height: float = minf(112, (pagination_y - row_y - 44) / 6.0)
	for index: int in range(6):
		DeckUi.place(_rows[index], Rect2(32, row_y + index * (row_height + 6), size.x - 288, row_height))
		DeckUi.place(_delete_buttons[index], Rect2(size.x - 232, row_y + index * (row_height + 6), 200, row_height))
	DeckUi.place(_empty, Rect2(56, row_y + 24, size.x - 112, 120))
	DeckUi.place(_previous, Rect2(32, pagination_y, 180, 38))
	DeckUi.place(_next, Rect2(size.x - 212, pagination_y, 180, 38))
	DeckUi.place(_count, Rect2(232, pagination_y, size.x - 464, 38))
	DeckUi.place(_notice, Rect2(32, size.y - 59, size.x - 328, 36))
	DeckUi.place(_back, Rect2(size.x - 264, size.y - 62, 232, 40))

func _refresh() -> void:
	_page_index = clampi(_page_index, 0, maxi(0, ceili(service.drafts.size() / 6.0) - 1))
	for index: int in range(6):
		var draft_index: int = _page_index * 6 + index
		var available: bool = draft_index < service.drafts.size()
		_rows[index].visible = available
		_delete_buttons[index].visible = available
		_delete_buttons[index].disabled = not service.writable
		if available:
			_rows[index].service = service
			_rows[index].draft = service.drafts[draft_index]
			_rows[index].queue_redraw()
	_previous.disabled = _page_index == 0
	_next.disabled = (_page_index + 1) * 6 >= service.drafts.size()
	_count.text = "%d 个自建卡组 · %d / %d 页" % [service.drafts.size(), _page_index + 1, maxi(1, ceili(service.drafts.size() / 6.0))]
	_empty.visible = service.drafts.is_empty()
	_notice.text = service.error if not service.error.is_empty() else service.notice

func _create() -> void:
	if service.create_draft():
		navigation_requested.emit(&"deck_editor")
	else:
		_notice.text = service.error

func _copy_preset() -> void:
	if _preset.selected >= 0 and service.create_draft(str(service.presets[_preset.selected]["id"])):
		navigation_requested.emit(&"deck_editor")
	else:
		_notice.text = service.error

func _edit(index: int) -> void:
	var draft: Dictionary = service.drafts[_page_index * 6 + index]
	if service.open_draft(str(draft["id"])):
		navigation_requested.emit(&"deck_editor")
	else:
		_notice.text = service.error

func _turn_page(delta: int) -> void:
	_page_index += delta
	_refresh()

func _request_delete(index: int) -> void:
	var draft: Dictionary = service.drafts[_page_index * 6 + index]
	_delete_id = str(draft["id"])
	_dialog.present("删除卡组？", "确认删除「%s」？" % draft["name"], ["确认删除", "取消"])

func _delete_confirmed(index: int) -> void:
	if index == 0:
		service.erase_draft(_delete_id)
		_refresh()
