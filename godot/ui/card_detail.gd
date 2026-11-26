class_name CardDetail
extends PanelContainer

signal closed()

var card: CardPresentation
var _lesson_definition: String = ""
var _lesson_text: String = ""
@onready var _title: Label = %Title
@onready var _type: Label = %Type
@onready var _art: CardFace = %Art
@onready var _body: RichTextLabel = %Body

func _ready() -> void:
	Typography.style_card_text(_body)
	(%Close as Button).pressed.connect(closed.emit)
	if card != null:
		configure(card)

func set_lesson_hint(definition: String, text: String) -> void:
	_lesson_definition = definition
	_lesson_text = text
	_update_lesson_hint()

func _update_lesson_hint() -> void:
	%LessonHint.text = _lesson_text
	%LessonHint.visible = card != null and not card.hidden and card.definition_id == _lesson_definition and not _lesson_text.is_empty()

func clear_card() -> void:
	card = null
	$Column.hide()
	if is_node_ready():
		_update_lesson_hint()
		_art.configure(null)
		_title.text = ""
		_type.text = ""
		_body.text = ""

func configure(value: CardPresentation) -> void:
	var same_card: bool = card != null and card.instance_id == value.instance_id and card.hidden == value.hidden
	card = value
	$Column.show()
	if not is_node_ready():
		return
	_update_lesson_hint()
	_art.configure(card)
	if card.hidden:
		_title.text = "盖伏卡"
		_type.text = "身份未公开"
		_body.text = "盖伏卡的名称、类型与能力在公开前不可见。"
		return
	_title.text = card.title
	_type.text = card.type_label
	if card.speed > 0:
		_type.text += " · %d速" % card.speed
	if not card.rarity.is_empty():
		_type.text += " · " + card.rarity_text()
	if card.preview:
		_type.text += " · 2.0"
	var content: String = card.detail_text(true)
	if not card.number.is_empty():
		content += "\n\n编号：" + CardPresentation.escape_bbcode(card.number)
	if _body.text != content:
		var scroll: float = _body.get_v_scroll_bar().value
		_body.text = content
		if same_card:
			_body.get_v_scroll_bar().value = scroll
	if not same_card:
		_body.scroll_to_line(0)
