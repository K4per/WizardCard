class_name DeckDetailPane
extends PanelContainer

var service: FrontendService
var card: CardPresentation
var _title: Label
var _face: DeckCardPainter
var _body: RichTextLabel
var _number: Label
var _hint: Label

func _ready() -> void:
	_title = %Title
	_face = %Face
	_body = %Body
	_number = %Number
	_hint = %Hint
	theme = DeckTheme.create()
	_number.add_theme_color_override(&"font_color", Color("71838e"))
	_hint.add_theme_color_override(&"font_color", DeckTheme.MUTED)

func inspect(id: String) -> void:
	if service == null:
		return
	card = service.card_by_id(id)
	if card == null:
		return
	_title.text = card.title
	_face.configure(card)
	var data: Dictionary = service.definitions.get(id, {})
	var rarity: String = str(data.get("rarityLabel", card.rarity_text()))
	var lines: Array[String] = [card.type_label + (" · " + rarity if not rarity.is_empty() else "")]
	lines.append(card.detail_text())
	if not str(data.get("flavor", "")).is_empty():
		lines.append(str(data["flavor"]))
	_body.text = "\n\n".join(lines)
	_number.text = "编号：" + card.number if not card.number.is_empty() else ""
	_body.scroll_to_line(0)
