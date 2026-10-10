class_name LibraryPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var _grid: GridContainer
var _search: LineEdit
var _detail: CardDetail
var _content: HBoxContainer
var _notice: Label

func _ready() -> void:
	var margin: MarginContainer = UiFactory.margin(self)
	var column: VBoxContainer = VBoxContainer.new()
	margin.add_child(column)
	UiFactory.heading(column, "卡牌图鉴", navigation_requested.emit.bind(&"menu"))
	_search = LineEdit.new()
	_search.placeholder_text = "搜索名称或能力"
	_search.custom_minimum_size.y = 48
	_search.text_changed.connect(_populate)
	column.add_child(_search)
	_notice = UiFactory.label("", 16)
	column.add_child(_notice)
	_content = HBoxContainer.new()
	_content.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(_content)
	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_content.add_child(scroll)
	_grid = GridContainer.new()
	_grid.add_theme_constant_override(&"h_separation", 20)
	_grid.add_theme_constant_override(&"v_separation", 20)
	scroll.add_child(_grid)
	_detail = CardDetail.new()
	_detail.visible = false
	_detail.closed.connect(_close_detail)
	_content.add_child(_detail)
	resized.connect(_resize_grid)

func configure(value: FrontendService) -> void:
	service = value
	_notice.text = "当前正式卡池 %s · 卡牌迁移方案待审定" % service.rules_version
	if not service.error.is_empty():
		_notice.text = service.error
	_populate("")
	_resize_grid()

func _populate(query: String) -> void:
	for child: Node in _grid.get_children():
		_grid.remove_child(child)
		child.queue_free()
	if service == null:
		return
	for card: CardPresentation in service.cards:
		if not query.is_empty() and not (card.title + card.text).contains(query):
			continue
		var tile: CardTile = CardTile.new()
		_grid.add_child(tile)
		tile.configure(card)
		tile.card_selected.connect(_show_detail)

func _resize_grid() -> void:
	var available: float = size.x - 64 - (360 if _detail.visible else 0)
	_grid.columns = maxi(1, int(available / 196.0))

func _show_detail(card: CardPresentation) -> void:
	_detail.configure(card)
	_detail.show()
	_resize_grid()

func _close_detail() -> void:
	_detail.hide()
	_resize_grid()
