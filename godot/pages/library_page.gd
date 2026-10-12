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
	_grid = %Grid
	_search = %Search
	_detail = %Detail
	_content = %Content
	_notice = %Notice
	_kind_filter = %KindFilter
	(%Back as Button).pressed.connect(navigation_requested.emit.bind(&"menu"))
	_search.text_changed.connect(_populate)
	for kind: String in ["全部类型", "解析法术", "言灵法术", "咒符法术", "阵法", "符文"]:
		_kind_filter.add_item(kind)
	_kind_filter.item_selected.connect(_kind_changed)
	_detail.closed.connect(_close_detail)
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
