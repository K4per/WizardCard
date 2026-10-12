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
	_title = %Title
	_description = %Description
	_new = %New
	_preset = %Preset
	_copy = %Copy
	_empty = %Empty
	_previous = %Previous
	_next = %Next
	_count = %Count
	_notice = %Notice
	_back = %Back
	_dialog = %Dialog
	DeckUi.compact_field(_preset)
	_new.pressed.connect(_create)
	_copy.pressed.connect(_copy_preset)
	_previous.pressed.connect(_turn_page.bind(-1))
	_next.pressed.connect(_turn_page.bind(1))
	_back.pressed.connect(navigation_requested.emit.bind(&"menu"))
	for index: int in range(6):
		var row: DeckListRow = get_node("Margin/Column/Rows/Row%d/Deck%d" % [index, index]) as DeckListRow
		row.pressed.connect(_edit.bind(index))
		_rows.append(row)
		var button: Button = get_node("Margin/Column/Rows/Row%d/Delete%d" % [index, index]) as Button
		button.pressed.connect(_request_delete.bind(index))
		_delete_buttons.append(button)
	_dialog.chosen.connect(_delete_confirmed)

func configure(value: FrontendService) -> void:
	service = value
	service.refresh_library()
	_preset.clear()
	for preset: Dictionary in service.presets:
		_preset.add_item("预设：" + str(preset["name"]))
	_copy.disabled = service.presets.is_empty() or not service.writable
	_new.disabled = not service.writable
	_refresh()

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
