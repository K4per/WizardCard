class_name CardDetail
extends PanelContainer

signal closed()

var card: CardPresentation
var _title: Label
var _type: Label
var _art: CardFace
var _body: RichTextLabel
@export var battle_skin: bool = false
@export var vertical_battle_layout: bool = false
var _battle_close: Button
var _info_skin: TextureRect

func _ready() -> void:
	if has_node("Column"):
		_title = %Title
		_type = %Type
		_art = %Art
		_body = %Body
		(%Close as Button).pressed.connect(closed.emit)
		if card != null:
			configure(card)
		return
	if battle_skin:
		# The battle panel is sized by MatchPage on every viewport change. A
		# fixed minimum here can force the right-anchored panel past the left edge
		# on narrow windows before the responsive layout gets a chance to clamp it.
		custom_minimum_size = Vector2.ZERO
		_build_battle_detail()
		return
	custom_minimum_size.x = 340
	add_theme_stylebox_override(&"panel", DeckTheme.box(DeckTheme.PANEL, DeckTheme.BORDER, 20))
	var column: VBoxContainer = VBoxContainer.new()
	add_child(column)
	var heading: HBoxContainer = HBoxContainer.new()
	column.add_child(heading)
	_title = UiFactory.label("", 24, true)
	heading.add_child(_title)
	var close: Button = UiFactory.button("×", closed.emit)
	close.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	heading.add_child(close)
	_art = CardFace.new()
	_art.custom_minimum_size = Vector2(240, 320 if get_viewport_rect().size.y >= 900 else 160)
	column.add_child(_art)
	_type = UiFactory.label("", 16, true)
	column.add_child(_type)
	_body = RichTextLabel.new()
	_body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_body.custom_minimum_size.y = 60
	_body.bbcode_enabled = false
	column.add_child(_body)
	resized.connect(_resize_face)
	if card != null:
		configure(card)

func _resize_face() -> void:
	if battle_skin:
		_layout_battle_detail()
		return
	_art.custom_minimum_size.y = 320 if get_viewport_rect().size.y >= 900 else 160

func _build_battle_detail() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	var panel_style: StyleBoxFlat = StyleBoxFlat.new()
	panel_style.bg_color = Color("08131f") if vertical_battle_layout else Color.TRANSPARENT
	panel_style.border_color = Color("715033")
	panel_style.set_border_width_all(1 if vertical_battle_layout else 0)
	add_theme_stylebox_override(&"panel", panel_style)
	var holder: Control = Control.new()
	add_child(holder)
	_info_skin = TextureRect.new()
	_info_skin.texture = BattleArt.texture("detail_information_box")
	_info_skin.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_info_skin.mouse_filter = Control.MOUSE_FILTER_IGNORE
	holder.add_child(_info_skin)
	_title = UiFactory.label("", 18)
	_title.add_theme_font_override(&"font", Typography.HEADING)
	_title.clip_text = true
	holder.add_child(_title)
	_type = UiFactory.label("", 14, true)
	holder.add_child(_type)
	_art = CardFace.new()
	_art.mouse_filter = Control.MOUSE_FILTER_IGNORE
	holder.add_child(_art)
	_body = RichTextLabel.new()
	_body.bbcode_enabled = false
	_body.add_theme_font_size_override(&"normal_font_size", 16)
	holder.add_child(_body)
	_battle_close = UiFactory.button("", closed.emit)
	_battle_close.custom_minimum_size = Vector2.ZERO
	_battle_close.tooltip_text = "关闭详情"
	for state: String in ["normal", "hover", "pressed", "disabled"]:
		_battle_close.add_theme_stylebox_override(StringName(state), StyleBoxEmpty.new())
	holder.add_child(_battle_close)
	resized.connect(_layout_battle_detail)
	_layout_battle_detail()
	if card != null:
		configure(card)

func _layout_battle_detail() -> void:
	if _art == null:
		return
	if vertical_battle_layout:
		_layout_vertical_battle_detail()
		return
	var card_width: float = size.x * 0.51
	var card_height: float = minf(size.y, card_width / 0.75)
	_art.position = Vector2.ZERO
	_art.size = Vector2(card_width, card_height)
	_info_skin.position = Vector2(card_width + 12, 0)
	_info_skin.size = Vector2(size.x - card_width - 12, card_height)
	var inset: float = _info_skin.position.x + 18
	var text_width: float = _info_skin.size.x - 36
	_title.position = Vector2(inset, 20)
	_title.size = Vector2(text_width - 24, 28)
	_type.position = Vector2(inset, 54)
	_type.size = Vector2(text_width, 42)
	_body.position = Vector2(inset, 105)
	_body.size = Vector2(text_width, maxf(40, card_height - 130))
	_battle_close.position = _info_skin.position + _info_skin.size * Vector2(0.87, 0.045)
	_battle_close.size = _info_skin.size * Vector2(0.09, 0.075)

func _layout_vertical_battle_detail() -> void:
	var card_width: float = minf(size.x * 0.82, maxf(160, (size.y - 260) * 0.75))
	var card_height: float = card_width / 0.75
	_art.position = Vector2((size.x - card_width) * 0.5, 10)
	_art.size = Vector2(card_width, card_height)
	_info_skin.position = Vector2(12, card_height + 20)
	_info_skin.size = Vector2(size.x - 24, maxf(0, size.y - _info_skin.position.y - 12))
	_title.position = Vector2(_info_skin.position.x + 24, _info_skin.position.y + 14)
	_title.size = Vector2(_info_skin.size.x - 90, 32)
	_type.position = Vector2(_info_skin.position.x + 24, _info_skin.position.y + 50)
	_type.size = Vector2(_info_skin.size.x - 48, 32)
	_body.position = Vector2(_info_skin.position.x + 24, _info_skin.position.y + 88)
	_body.size = Vector2(_info_skin.size.x - 48, maxf(40, _info_skin.size.y - 104))
	_battle_close.position = Vector2(size.x - 54, 4)
	_battle_close.size = Vector2(42, 42)

func configure(value: CardPresentation) -> void:
	card = value
	if not is_node_ready():
		return
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
	_body.text = card.detail_text()
	if not card.number.is_empty():
		_body.text += "\n\n编号：" + card.number
