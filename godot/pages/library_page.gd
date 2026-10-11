class_name LibraryPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var _grid: GridContainer
var _search: LineEdit
var _detail: CardDetail
var _content: HBoxContainer
var _notice: Label
var _kind_filter: OptionButton

const KINDS: Array[String] = ["", "analysis", "incantation", "talisman", "formation", "rune"]

func _ready() -> void:
	var margin: MarginContainer = UiFactory.margin(self)
	var column: VBoxContainer = VBoxContainer.new()
	margin.add_child(column)
	UiFactory.heading(column, "卡牌图鉴", navigation_requested.emit.bind(&"menu"))
	var filters: HBoxContainer = HBoxContainer.new()
	column.add_child(filters)
	_search = LineEdit.new()
	_search.placeholder_text = "搜索编号、名称、学派或效果"
	_search.custom_minimum_size.y = 48
	_search.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_search.text_changed.connect(_populate)
	filters.add_child(_search)
	_kind_filter = OptionButton.new()
	_kind_filter.custom_minimum_size = Vector2(224, 48)
	for kind: String in ["全部类型", "解析法术", "言灵法术", "咒符法术", "阵法", "符文"]:
		_kind_filter.add_item(kind)
	_kind_filter.item_selected.connect(_kind_changed)
	filters.add_child(_kind_filter)
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
	_populate("")
	_resize_grid()

func _populate(query: String) -> void:
	for child: Node in _grid.get_children():
		_grid.remove_child(child)
		child.queue_free()
	if service == null:
		return
	var collection: Array[CardPresentation] = service.cards
	for card: CardPresentation in collection:
		if _kind_filter.selected > 0 and card.type_key != KINDS[_kind_filter.selected]:
			continue
		if not query.is_empty() and not (card.title + card.type_label + card.metadata_text + card.text).contains(query):
			continue
		var tile: CardTile = CardTile.new()
		_grid.add_child(tile)
		tile.configure(card)
		tile.card_selected.connect(_show_detail)
	_notice.text = "正式卡池 %s · 显示 %d / %d 张" % [service.rules_version, _grid.get_child_count(), collection.size()]
	if not service.error.is_empty():
		_notice.text = service.error

func _kind_changed(_index: int) -> void:
	_close_detail()
	_populate(_search.text)

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
