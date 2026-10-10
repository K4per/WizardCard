class_name CardDetail
extends PanelContainer

signal closed()

var card: CardPresentation
var _title: Label
var _type: Label
var _art: TextureRect
var _body: RichTextLabel

func _ready() -> void:
	custom_minimum_size.x = 340
	var column: VBoxContainer = VBoxContainer.new()
	add_child(column)
	var heading: HBoxContainer = HBoxContainer.new()
	column.add_child(heading)
	_title = UiFactory.label("", 24, true)
	heading.add_child(_title)
	var close: Button = UiFactory.button("×", closed.emit)
	close.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	heading.add_child(close)
	_art = TextureRect.new()
	_art.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
	_art.custom_minimum_size = Vector2(280, 180)
	column.add_child(_art)
	_type = UiFactory.label("", 16, true)
	column.add_child(_type)
	_body = RichTextLabel.new()
	_body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_body.custom_minimum_size.y = 100
	_body.bbcode_enabled = false
	column.add_child(_body)
	if card != null:
		configure(card)

func configure(value: CardPresentation) -> void:
	card = value
	if not is_node_ready():
		return
	if card.hidden:
		_title.text = "盖伏卡"
		_type.text = "身份未公开"
		_body.text = "盖伏卡的名称、类型与能力在公开前不可见。"
		_art.texture = ArtBank.texture("c2d.card.back.compact")
		return
	_title.text = card.title
	_type.text = "%s · %d速 · %s" % [card.type_label, card.speed, card.rarity]
	_body.text = card.text
	_art.texture = ArtBank.texture("illustration." + card.definition_id)
